/**
 * @file client_response.hpp
 * @brief every phase in dialog can have some buttons
 * buttons are generalized on this level as responses
 * because non GUI/WUI client can also use them
 * bound to ClientFSM in src/common/client_fsm_types.h
 */

#pragma once

#include "general_response.hpp"
#include <cstdint>
#include <cstddef>
#include <array>

enum { RESPONSE_BITS = 4,                   //number of bits used to encode response
    MAX_RESPONSES = (1 << RESPONSE_BITS) }; //maximum number of responses in one phase

using PhaseResponses = std::array<Response, MAX_RESPONSES>;

//count enum class members (if "_first" and "_last" is defined)
template <class T>
constexpr size_t CountPhases() {
    return static_cast<size_t>(T::_last) - static_cast<size_t>(T::_first) + 1;
}
//use this when creating an event
//encodes enum to position in phase
template <class T>
constexpr uint8_t GetPhaseIndex(T phase) {
    return static_cast<size_t>(phase) - static_cast<size_t>(T::_first);
}

template <class T>
constexpr T GetEnumFromPhaseIndex(size_t index) {
    return static_cast<T>(static_cast<size_t>(T::_first) + index);
}

//define enum classes for responses here
//and YES phase can have 0 responses
//every enum must have "_first" and "_last"
//"_first" ==  "previous_enum::_last" + 1
//EVERY response shall have a unique ID (so every button in GUI is unique)
enum class PhasesLoadUnload : uint16_t {
    _first = 0,
    Parking_stoppable,
    Parking_unstoppable,
    WaitingTemp_stoppable,
    WaitingTemp_unstoppable,
    PreparingToRam_stoppable,
    PreparingToRam_unstoppable,
    Ramming_stoppable,
    Ramming_unstoppable,
    Unloading_stoppable,
    Unloading_unstoppable,
    RemoveFilament,
    IsFilamentUnloaded,
    FilamentNotInFS,
    ManualUnload,
    UserPush_stoppable,
    UserPush_unstoppable,
    MakeSureInserted_stoppable,
    MakeSureInserted_unstoppable,
    Inserting_stoppable,
    Inserting_unstoppable,
    IsFilamentInGear,
    Ejecting_stoppable,
    Ejecting_unstoppable,
    Loading_stoppable,
    Loading_unstoppable,
    Purging_stoppable,
    Purging_unstoppable,
    IsColor,
    IsColorPurge,
    Unparking,
    _last = Unparking
};

enum class PhasesPreheat : uint16_t {
    _first = static_cast<uint16_t>(PhasesLoadUnload::_last) + 1,
    UserTempSelection,
    _last = UserTempSelection
};

enum class PhasesPrintPreview : uint16_t {
    _first = static_cast<uint16_t>(PhasesPreheat::_last) + 1,
    main_dialog = _first,
    wrong_printer,
    filament_not_inserted,
    mmu_filament_inserted,
    wrong_filament,
    _last = wrong_filament
};

// GUI phases of selftest/wizard
enum class PhasesSelftest : uint16_t {
    _first = static_cast<uint16_t>(PhasesPrintPreview::_last) + 1,
    _none = _first,

    _first_WizardPrologue,
    WizardPrologue_ask_run = _first_WizardPrologue,
    WizardPrologue_ask_run_dev, // developer version has ignore button
    WizardPrologue_info,
    WizardPrologue_info_detailed,
    _last_WizardPrologue = WizardPrologue_info_detailed,

    _first_ESP,
    ESP_instructions = _first_ESP,
    ESP_USB_not_inserted, // must be before ask_gen/ask_gen_overwrite, because it depends on file existence
    ESP_ask_gen,
    ESP_ask_gen_overwrite,
    ESP_makefile_failed,
    ESP_eject_USB,
    ESP_insert_USB,
    ESP_invalid,
    ESP_uploading_config,
    ESP_enabling_WIFI,
    ESP_uploaded,
    _last_ESP = ESP_uploaded,

    _first_ESP_progress,
    ESP_progress_info = _first_ESP_progress,
    ESP_progress_upload,
    ESP_progress_passed,
    ESP_progress_failed,
    _last_ESP_progress = ESP_progress_failed,

    _first_ESP_qr,
    ESP_qr_instructions_flash = _first_ESP_qr,
    ESP_qr_instructions,
    _last_ESP_qr = ESP_qr_instructions,

    _first_Fans,
    Fans = _first_Fans,
    _last_Fans = Fans,

    _first_Axis,
    Axis = _first_Axis,
    _last_Axis = Axis,

    _first_Heaters,
    Heaters = _first_Heaters,
    _last_Heaters = Heaters,

    _first_FirstLayer,
    FirstLayer_mbl = _first_FirstLayer,
    FirstLayer_print,
    _last_FirstLayer = FirstLayer_print,

    _first_FirstLayerQuestions,
    FirstLayer_filament_known_and_not_unsensed = _first_FirstLayerQuestions,
    FirstLayer_filament_not_known_or_unsensed,
    FirstLayer_calib,
    FirstLayer_use_val,
    FirstLayer_start_print,
    FirstLayer_reprint,
    FirstLayer_clean_sheet,
    FirstLayer_failed,
    _last_FirstLayerQuestions = FirstLayer_failed,

    _first_Result,
    Result = _first_Result,
    _last_Result = Result,

    _first_WizardEpilogue,
    WizardEpilogue_ok = _first_WizardEpilogue, // ok is after result
    WizardEpilogue_nok,                        // nok is before result
    _last_WizardEpilogue = WizardEpilogue_nok,

    _last = _last_WizardEpilogue
};

enum class PhasesCrashRecovery : uint16_t {
    _first = static_cast<uint16_t>(PhasesSelftest::_last) + 1,
    check_X = _first, //in this case is safe to have check_X == _first
    check_Y,
    home,
    axis_NOK, //< just for unification of the two below
    axis_short,
    axis_long,
    repeated_crash,
    _last = repeated_crash
};

//static class for work with fsm responses (like button click)
//encode responses - get them from marlin client, to marlin server and decode them again
class ClientResponses {
    ClientResponses() = delete;
    ClientResponses(ClientResponses &) = delete;

    //declare 2d arrays of single buttons for radio buttons
    static const PhaseResponses LoadUnloadResponses[CountPhases<PhasesLoadUnload>()];
    static const PhaseResponses PreheatResponses[CountPhases<PhasesPreheat>()];
    static const PhaseResponses PrintPreviewResponses[CountPhases<PhasesPrintPreview>()];
    static const PhaseResponses SelftestResponses[CountPhases<PhasesSelftest>()];
    static const PhaseResponses CrashRecoveryResponses[CountPhases<PhasesCrashRecovery>()];

    //methods to "bind" button array with enum type
    static const PhaseResponses &getResponsesInPhase(PhasesLoadUnload phase) { return LoadUnloadResponses[static_cast<size_t>(phase)]; }
    static const PhaseResponses &getResponsesInPhase(PhasesPreheat phase) { return PreheatResponses[static_cast<size_t>(phase) - static_cast<size_t>(PhasesPreheat::_first)]; }
    static const PhaseResponses &getResponsesInPhase(PhasesPrintPreview phase) { return PrintPreviewResponses[static_cast<size_t>(phase) - static_cast<size_t>(PhasesPrintPreview::_first)]; }
    static const PhaseResponses &getResponsesInPhase(PhasesSelftest phase) { return SelftestResponses[static_cast<size_t>(phase) - static_cast<size_t>(PhasesSelftest::_first)]; }
    static const PhaseResponses &getResponsesInPhase(PhasesCrashRecovery phase) { return CrashRecoveryResponses[static_cast<size_t>(phase) - static_cast<size_t>(PhasesCrashRecovery::_first)]; }

public:
    //get index of single response in PhaseResponses
    //return -1 (maxval) if does not exist
    template <class T>
    static uint8_t GetIndex(T phase, Response response) {
        const PhaseResponses &cmds = getResponsesInPhase(phase);
        for (size_t i = 0; i < MAX_RESPONSES; ++i) {
            if (cmds[i] == response)
                return i;
        }
        return -1;
    }

    //get response from PhaseResponses by index
    template <class T>
    static Response GetResponse(T phase, uint8_t index) {
        if (index >= MAX_RESPONSES)
            return Response::_none;
        const PhaseResponses &cmds = getResponsesInPhase(phase);
        return cmds[index];
    }

    //get all responses accepted in phase
    template <class T>
    static const PhaseResponses &GetResponses(T phase) {
        return getResponsesInPhase(phase);
    }
    template <class T>
    static bool HasButton(T phase) {
        return GetResponse(phase, 0) != Response::_none; // this phase has no responses
    }

    //encode phase and client response (in GUI radio button and clicked index) into int
    //use on client side
    //return -1 (maxval) if does not exist
    template <class T>
    static uint32_t Encode(T phase, Response response) {
        uint8_t clicked_index = GetIndex(phase, response);
        if (clicked_index >= MAX_RESPONSES)
            return -1; // this phase does not have response with this index
        return ((static_cast<uint32_t>(phase)) << RESPONSE_BITS) + uint32_t(clicked_index);
    }
};

enum class SelftestParts {
    WizardPrologue,
    ESP,
    ESP_progress,
    ESP_qr,
    Axis,
    Fans,
    Heaters,
    FirstLayer,
    FirstLayerQuestions,
    Result,
    WizardEpilogue,
    _none, //cannot be created, must have same index as _count
    _count = _none
};

static constexpr PhasesSelftest SelftestGetFirstPhaseFromPart(SelftestParts part) {
    switch (part) {
    case SelftestParts::WizardPrologue:
        return PhasesSelftest::_first_WizardPrologue;
    case SelftestParts::ESP:
        return PhasesSelftest::_first_ESP;
    case SelftestParts::ESP_progress:
        return PhasesSelftest::_first_ESP_progress;
    case SelftestParts::ESP_qr:
        return PhasesSelftest::_first_ESP_qr;
    case SelftestParts::Axis:
        return PhasesSelftest::_first_Axis;
    case SelftestParts::Fans:
        return PhasesSelftest::_first_Fans;
    case SelftestParts::Heaters:
        return PhasesSelftest::_first_Heaters;
    case SelftestParts::FirstLayer:
        return PhasesSelftest::_first_FirstLayer;
    case SelftestParts::FirstLayerQuestions:
        return PhasesSelftest::_first_FirstLayerQuestions;
    case SelftestParts::Result:
        return PhasesSelftest::_first_Result;
    case SelftestParts::WizardEpilogue:
        return PhasesSelftest::_first_WizardEpilogue;
    case SelftestParts::_none:
        break;
    }
    return PhasesSelftest::_none;
}

static constexpr PhasesSelftest SelftestGetLastPhaseFromPart(SelftestParts part) {
    switch (part) {
    case SelftestParts::WizardPrologue:
        return PhasesSelftest::_last_WizardPrologue;
    case SelftestParts::ESP:
        return PhasesSelftest::_last_ESP;
    case SelftestParts::ESP_progress:
        return PhasesSelftest::_last_ESP_progress;
    case SelftestParts::ESP_qr:
        return PhasesSelftest::_last_ESP_qr;
    case SelftestParts::Axis:
        return PhasesSelftest::_last_Axis;
    case SelftestParts::Fans:
        return PhasesSelftest::_last_Fans;
    case SelftestParts::Heaters:
        return PhasesSelftest::_last_Heaters;
    case SelftestParts::FirstLayer:
        return PhasesSelftest::_last_FirstLayer;
    case SelftestParts::FirstLayerQuestions:
        return PhasesSelftest::_last_FirstLayerQuestions;
    case SelftestParts::Result:
        return PhasesSelftest::_last_Result;
    case SelftestParts::WizardEpilogue:
        return PhasesSelftest::_last_WizardEpilogue;
    case SelftestParts::_none:
        break;
    }
    return PhasesSelftest::_none;
}

static constexpr bool SelftestPartContainsPhase(SelftestParts part, PhasesSelftest ph) {
    const uint16_t ph_u16 = uint16_t(ph);

    return (ph_u16 >= uint16_t(SelftestGetFirstPhaseFromPart(part))) && (ph_u16 <= uint16_t(SelftestGetLastPhaseFromPart(part)));
}

static constexpr SelftestParts SelftestGetPartFromPhase(PhasesSelftest ph) {
    for (size_t i = 0; i < size_t(SelftestParts::_none); ++i) {
        if (SelftestPartContainsPhase(SelftestParts(i), ph))
            return SelftestParts(i);
    }

    if (SelftestPartContainsPhase(SelftestParts::WizardPrologue, ph))
        return SelftestParts::WizardPrologue;

    if (SelftestPartContainsPhase(SelftestParts::ESP, ph))
        return SelftestParts::ESP;
    if (SelftestPartContainsPhase(SelftestParts::ESP_progress, ph))
        return SelftestParts::ESP_progress;
    if (SelftestPartContainsPhase(SelftestParts::ESP_qr, ph))
        return SelftestParts::ESP_qr;

    if (SelftestPartContainsPhase(SelftestParts::Fans, ph))
        return SelftestParts::Fans;

    if (SelftestPartContainsPhase(SelftestParts::Axis, ph))
        return SelftestParts::Axis;

    if (SelftestPartContainsPhase(SelftestParts::Heaters, ph))
        return SelftestParts::Heaters;

    if (SelftestPartContainsPhase(SelftestParts::WizardEpilogue, ph))
        return SelftestParts::WizardEpilogue;

    if (SelftestPartContainsPhase(SelftestParts::Result, ph))
        return SelftestParts::Result;

    return SelftestParts::_none;
};

enum class FSM_action {
    no_action,
    create,
    destroy,
    change
};

#include <optional>

/**
 * @brief determine if correct state of fsm is se
 * useful for FSM with no data
 *
 * @tparam T          - type of current / wanted state
 * @param current     - current state
 * @param should_be   - wanted state
 * @return FSM_action - what action needs to be done
 */
template <class T>
FSM_action IsFSM_action_needed(std::optional<T> current, std::optional<T> should_be) {
    if (!current && !should_be)
        return FSM_action::no_action;

    if (!current && should_be)
        return FSM_action::create;

    if (current && !should_be)
        return FSM_action::destroy;

    // current && should_be
    if (*current == *should_be)
        return FSM_action::no_action;

    return FSM_action::change;
}
