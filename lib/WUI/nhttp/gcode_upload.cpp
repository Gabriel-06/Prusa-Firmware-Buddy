#include "gcode_upload.h"
#include "upload_state.h"
#include "file_info.h"
#include "handler.h"
#include "../../src/common/gcode_filename.h"
#include "../wui_api.h"

#include <cassert>
#include <cstring>
#include <unistd.h>
#include <errno.h>
#include <sys/statvfs.h>

extern "C" {

// Inject for tests, which are compiled on systems without it in the header.
size_t strlcpy(char *, const char *, size_t);
size_t strlcat(char *, const char *, size_t);
}

#define USB_MOUNT_POINT        "/usb/"
#define USB_MOUNT_POINT_LENGTH (strlen(USB_MOUNT_POINT))
// Length of the temporary file name. Due to the template below, we are sure to
// fit.
static const size_t TMP_BUFF_LEN = 20;
static const char *const UPLOAD_TEMPLATE = USB_MOUNT_POINT "%zu.tmp";
static const char *const CHECK_FILENAME = USB_MOUNT_POINT "check.tmp";

namespace nhttp::printer {

using handler::Continue;
using handler::RequestParser;
using handler::StatusPage;
using handler::Step;
using http::Status;
using std::get;
using std::holds_alternative;
using std::make_tuple;
using std::move;
using std::string_view;
using transfers::Monitor;

GcodeUpload::GcodeUpload(UploadParams &&uploader, Monitor::Slot &&slot, bool json_errors, size_t length, size_t upload_idx, unique_file_ptr file, UploadedNotify *uploaded)
    : upload(move(uploader))
    , monitor_slot(move(slot))
    , uploaded_notify(uploaded)
    , size_rest(length)
    , json_errors(json_errors)
    , cleanup_temp_file(true)
    , tmp_upload_file(move(file))
    , file_idx(upload_idx)
    , filename_checked(false) {
}

GcodeUpload::GcodeUpload(GcodeUpload &&other)
    : upload(move(other.upload))
    , monitor_slot(move(other.monitor_slot))
    , uploaded_notify(other.uploaded_notify)
    , size_rest(other.size_rest)
    , json_errors(other.json_errors)
    , cleanup_temp_file(other.cleanup_temp_file)
    , tmp_upload_file(move(other.tmp_upload_file))
    , file_idx(other.file_idx)
    , filename_checked(other.filename_checked) {
    // The ownership of the temp file is passed to the new instance.
    other.cleanup_temp_file = false;
}

GcodeUpload &GcodeUpload::operator=(GcodeUpload &&other) {
    upload = move(other.upload);
    uploaded_notify = other.uploaded_notify;
    size_rest = other.size_rest;
    json_errors = other.json_errors;

    cleanup_temp_file = other.cleanup_temp_file;
    // The ownership of the temp file is passed to the new instance.
    other.cleanup_temp_file = false;

    filename_checked = other.filename_checked;

    tmp_upload_file = move(other.tmp_upload_file);
    file_idx = other.file_idx;

    return *this;
}

GcodeUpload::~GcodeUpload() {
    if (cleanup_temp_file) {
        // This is area is entered in the last instance in a move chain. Note
        // that even in that case, the tmp_upload_file may be null (already
        // closed, before attempt to rename it) and the temp file to remove not
        // exist (because it got moved).
        //
        // This code is mostly responsible for cleanup during failure cases
        // (the success case being the above).
        //
        // We still perform the attempt to remove it even in case the file is
        // already closed (and therefore nullptr) because there _is_ a failure
        // case where we closed the file before renaming and failed to rename.
        // Even in that case we want to remove the temp file.
        tmp_upload_file.reset();
        char fname[TMP_BUFF_LEN];
        snprintf(fname, sizeof fname, UPLOAD_TEMPLATE, file_idx);
        remove(fname);
    } else {
        // Leftover open file in moved object :-O
        assert(tmp_upload_file.get() == nullptr);
    }
}

GcodeUpload::UploadResult GcodeUpload::start(const RequestParser &parser, UploadedNotify *uploaded, bool json_errors, UploadParams &&uploadParams) {
    // Note: authentication already checked by the caller.
    // Note: We return errors with connection-close because we don't know if the client sent part of the data.

    // One day we may want to support chunked and connection-close, but we are not there yet.
    if (!parser.content_length.has_value()) {
        return StatusPage(Status::LengthRequired, StatusPage::CloseHandling::ErrorClose, json_errors);
    }

    const char *path = holds_alternative<PutParams>(uploadParams) ? get<PutParams>(uploadParams).filepath.data() : nullptr;
    auto slot = Monitor::instance.allocate(Monitor::Type::Link, path, *parser.content_length);
    if (!slot.has_value()) {
        //FIXME: Is this the right status to return? Change would need to be
        // defined in the API spec first.
        return StatusPage(Status::Conflict, parser, "Another transfer in progress");
    }

    static size_t upload_idx = 0;
    const size_t file_idx = upload_idx++;
    char fname[TMP_BUFF_LEN];
    snprintf(fname, sizeof fname, UPLOAD_TEMPLATE, file_idx);
    unique_file_ptr file(fopen(fname, "wb"));
    if (!file) {
        // Missing USB -> Insufficient storage.
        return StatusPage(Status::InsufficientStorage, StatusPage::CloseHandling::ErrorClose, json_errors, "Missing USB drive");
    }
    struct statvfs svfs;
    if (statvfs(fname, &svfs) || (svfs.f_bavail * svfs.f_frsize * svfs.f_bsize) < *parser.content_length) {
        file.reset();
        remove(fname);
        return StatusPage(Status::InsufficientStorage, StatusPage::CloseHandling::ErrorClose, json_errors, "USB drive full");
    }
    const size_t csize = svfs.f_frsize * svfs.f_bsize;
    /* Pre-allocate the area needed for the file to prevent frequent metadata updates during the upload.
       We do the pre-allocation incrementally cluster-by-cluster to minimize the chance of stalling
       the media and blocking other tasks potentially waiting for the file system lock.
       The file size doesn't need to be exactly the size of the uploaded content. We will truncate
       the file later on. */
    for (unsigned long sz = csize; sz < *parser.content_length; sz += csize) {
        if (fseek(file.get(), sz, SEEK_SET)) {
            file.reset();
            remove(fname);
            return StatusPage(Status::InsufficientStorage, StatusPage::CloseHandling::ErrorClose, json_errors, "USB drive full");
        }
    }
    fsync(fileno(file.get()));
    fseek(file.get(), 0, SEEK_SET);

    return GcodeUpload(move(uploadParams), move(*slot), json_errors, *parser.content_length, file_idx, move(file), uploaded);
}

Step GcodeUpload::step(string_view input, bool terminated_by_client, uint8_t *, size_t) {
    if (terminated_by_client && size_rest > 0) {
        return { 0, 0, StatusPage(Status::BadRequest, StatusPage::CloseHandling::ErrorClose, json_errors, "Truncated body") };
    }

    const size_t read = std::min(input.size(), size_rest);
    monitor_slot.progress(read);
    return std::visit([input, read, this](auto &uploadParams) -> Step { return step(input, read, uploadParams); }, upload);
}

Step GcodeUpload::step(string_view input, const size_t read, UploadState &uploader) {
    uploader.setup(this);

    uploader.feed(input.substr(0, read));
    size_rest -= read;

    if (const auto err = uploader.get_error(); get<0>(err) != Status::Ok) {
        return { read, 0, StatusPage(get<0>(err), StatusPage::CloseHandling::ErrorClose, json_errors, get<1>(err)) };
    }

    if (size_rest == 0) {
        // Note: We do connection-close here. We are lazy to pass the
        // can-keep-alive flag around and it's unlikely one would want to reuse
        // the upload connection anyway.
        if (uploader.done()) {
            char filename[FILE_NAME_BUFFER_LEN + 5];
            strcpy(filename, USB_MOUNT_POINT);
            const char *orig_filename = uploader.get_filename();
            strlcpy(filename + USB_MOUNT_POINT_LENGTH, orig_filename, sizeof(filename) - USB_MOUNT_POINT_LENGTH);
            return { read, 0, FileInfo(filename, false, json_errors, true, FileInfo::ReqMethod::Get) };
        } else {
            return { read, 0, StatusPage(Status::BadRequest, StatusPage::CloseHandling::ErrorClose, json_errors, "Missing file") };
        }
    } else {
        return { read, 0, Continue() };
    }
}

Step GcodeUpload::step(string_view input, const size_t read, PutParams &putParams) {
    // remove the "/usb/" prefix
    const char *filename = putParams.filepath.data() + USB_MOUNT_POINT_LENGTH;

    // bit of a hack, would make more sense checking this in GcodeUpload::start,
    // but that is a static method and we need check_filename to be virtual, so
    // it can be used inside UploadState as a function of UploadHooks.
    if (!filename_checked) {
        auto filename_error = check_filename(filename);
        if (std::get<0>(filename_error) != Status::Ok)
            return { read, 0, StatusPage(std::get<0>(filename_error), StatusPage::CloseHandling::ErrorClose, json_errors, std::get<1>(filename_error)) };
        filename_checked = true;
    }

    auto error = data(input.substr(0, read));
    if (std::get<0>(error) != Status::Ok) {
        return { read, 0, StatusPage(std::get<0>(error), StatusPage::CloseHandling::ErrorClose, json_errors, std::get<1>(error)) };
    }

    size_rest -= read;
    if (size_rest == 0) {
        auto finish_error = finish(filename, putParams.print_after_upload);
        if (std::get<0>(finish_error) != Status::Ok)
            return { read, 0, StatusPage(std::get<0>(finish_error), StatusPage::CloseHandling::ErrorClose, json_errors, std::get<1>(finish_error)) };

        return { read, 0, FileInfo(putParams.filepath.data(), false, json_errors, true, FileInfo::ReqMethod::Get) };
    }

    return { read, 0, Continue() };
}

UploadHooks::Result GcodeUpload::data(std::string_view data) {
    assert(tmp_upload_file);
    for (;;) {
        const size_t written = fwrite(data.begin(), 1, data.size(), tmp_upload_file.get());
        if (written < data.size()) {
            if (errno == EAGAIN) {
                continue;
            }
            // Data won't fit into the flash drive -> Insufficient stogare.
            return make_tuple(Status::InsufficientStorage, "USB write error or USB full");
        } else {
            return make_tuple(Status::Ok, nullptr);
        }
    }
}

namespace {

    UploadHooks::Result prepend_usb_path(const char *filename, char *filepath_out, size_t filepath_size) {
        const uint32_t fname_length = strlen(filename);

        if ((fname_length + USB_MOUNT_POINT_LENGTH) >= filepath_size) {
            // The Request header fields too large is a bit of a stretch...
            return make_tuple(Status::RequestHeaderFieldsTooLarge, "File name too long");
        }
        strlcpy(filepath_out, USB_MOUNT_POINT, USB_MOUNT_POINT_LENGTH + 1);
        strlcat(filepath_out, filename, FILE_PATH_BUFFER_LEN - USB_MOUNT_POINT_LENGTH);

        return make_tuple(Status::Ok, nullptr);
    }

    template <class F>
    UploadHooks::Result try_rename(const char *src, const char *dest_fname, bool overwrite, F f) {
        char fn[FILE_NAME_BUFFER_LEN];
        auto error = prepend_usb_path(dest_fname, fn, sizeof(fn));
        if (std::get<0>(error) != Status::Ok) {
            return error;
        }

        FILE *attempt = fopen(fn, "r");
        if (attempt) {
            fclose(attempt);
            if (overwrite) {
                int result = remove(fn);
                if (result == -1) {
                    remove(src);
                    switch (errno) {
                    case EBUSY:
                        return make_tuple(Status::Conflict, "File is busy");
                    default:
                        return make_tuple(Status::InternalServerError, "Unknown error");
                    }
                }
            } else {
                remove(src);
                return make_tuple(Status::Conflict, "File already exists");
            }
        }

        int result = rename(src, fn);
        if (result != 0) {
            remove(src); // No check - no way to handle errors anyway.
            // we already checked for existence of the file, so we guess
            // it is weird file name/forbidden chars that contain (422 Unprocessable Entity).
            return make_tuple(Status::UnprocessableEntity, "Filename contains invalid characters");
        } else {
            return f(fn);
        }
    }
}

UploadHooks::Result GcodeUpload::check_filename(const char *filename) const {
    if (!filename_is_gcode(filename)) {
        return make_tuple(Status::UnsupportedMediaType, "Not a GCODE");
    }

    auto putParams = std::get_if<PutParams>(&upload);
    bool overwrite = putParams != nullptr && putParams->overwrite;
    if (overwrite) {
        char filepath[FILE_NAME_BUFFER_LEN];
        auto error = prepend_usb_path(filename, filepath, sizeof(filepath));
        if (std::get<0>(error) != Status::Ok)
            return error;
        if (wui_is_file_being_printed(filepath)) {
            return make_tuple(Status::Conflict, "File is busy");
        } else {
            return make_tuple(Status::Ok, nullptr);
        }
    }
    // We try to create and then delete the file. This places an empty file
    // there for a really short while, but it's there anyway ‒ something could
    // detect it. Any better way to check if the file name is fine?
    FILE *tmp = fopen(CHECK_FILENAME, "wb");
    if (!tmp) {
        return make_tuple(Status::InternalServerError, "Failed to create a check temp file");
    }
    fclose(tmp);
    return try_rename(CHECK_FILENAME, filename, false, [](const char *fname) -> UploadHooks::Result {
        remove(fname);
        return make_tuple(Status::Ok, nullptr);
    });
}

UploadHooks::Result GcodeUpload::finish(const char *final_filename, bool start_print) {
    char fname[TMP_BUFF_LEN];
    snprintf(fname, sizeof fname, UPLOAD_TEMPLATE, file_idx);

    // Remove extra pre-allocated space (ignore the result explicitly to avoid warnings)
    (void)ftruncate(fileno(tmp_upload_file.get()), ftell(tmp_upload_file.get()));
    // Close the file first, otherwise it can't be moved
    tmp_upload_file.reset();
    auto putParams = std::get_if<PutParams>(&upload);
    bool overwrite = putParams != nullptr && putParams->overwrite;
    return try_rename(fname, final_filename, overwrite, [&](char *filename) -> UploadHooks::Result {
        monitor_slot.done(Monitor::Outcome::Finished);
        if (uploaded_notify != nullptr) {
            if (uploaded_notify(filename, start_print)) {
                return make_tuple(Status::Ok, nullptr);
            } else {
                return make_tuple(Status::Conflict, "Can't print right now");
            }
        } else {
            return make_tuple(Status::Ok, nullptr);
        }
    });
}

}
