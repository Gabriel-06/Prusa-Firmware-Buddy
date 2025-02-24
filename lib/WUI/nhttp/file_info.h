#pragma once

#include "step.h"

#include <http/types.h>
#include <segmented_json.h>
#include <unique_dir_ptr.hpp>

#include <string_view>
#include <variant>

// Why does FILE_PATH_BUFFER_LEN lives in *gui*!?
#include "../../src/gui/file_list_defs.h"

namespace nhttp::printer {

// TODO: Figure a way not to taint the server part with specific implementations
/**
 * \brief Handler for serving file info and directory listings.
 */
class FileInfo {
public:
    enum class ReqMethod {
        Get,
        Head
    };

private:
    char filename[FILE_PATH_BUFFER_LEN];
    bool can_keep_alive : 1;
    bool after_upload : 1;
    bool json_errors : 1;
    ReqMethod method;

    /// Marker for the state before we even tried anything.
    class Uninitialized {};

    /// Marker that we want to send the last chunk (if in chunked mode).
    class LastChunk {};

    struct DirState {
        char *filename = nullptr;
        unique_dir_ptr dir;
        dirent *ent = nullptr;
        bool first = true;
        DirState() = default;
        DirState(FileInfo *owner, DIR *dir)
            : filename(owner->filename)
            , dir(dir) {}
        DirState(DirState &&other) = default;
        DirState &operator=(DirState &&other) = default;
    };

    /// The JSON renderer for the directory listing.
    class DirRenderer final : public json::JsonRenderer<DirState> {
    protected:
        virtual json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, DirState &state) const override;

    public:
        DirRenderer()
            : JsonRenderer(DirState()) {}
        DirRenderer(FileInfo *owner, DIR *dir)
            : JsonRenderer(DirState(owner, dir)) {}
    };

    struct FileState {
        FileInfo *owner;
        int64_t size;
        FileState(FileInfo *owner, size_t size)
            : owner(owner)
            , size(size) {}
        FileState(FileState &&other) = default;
        FileState &operator=(FileState &&other) = default;
    };

    /// Renderer for the file info.
    class FileRenderer final : public json::JsonRenderer<FileState> {
    private:
    protected:
        virtual json::JsonResult renderState(size_t resume_point, json::JsonOutput &output, FileState &state) const override;

    public:
        FileRenderer(FileInfo *owner, int64_t size)
            : JsonRenderer(FileState(owner, size)) {}
    };
    friend class FileRenderer;

    std::variant<Uninitialized, FileRenderer, DirRenderer, LastChunk> renderer;

public:
    FileInfo(const char *filename, bool can_keep_alive, bool json_error, bool after_upload, ReqMethod method);
    bool want_read() const { return false; }
    bool want_write() const { return true; }
    handler::Step step(std::string_view input, bool terminated_by_client, uint8_t *buffer, size_t buffer_size);
};

}
