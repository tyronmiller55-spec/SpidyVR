#pragma once
// How Spidy connects to the OpenXR runtime, kept free of OpenXR's headers so it
// is tested on its own: the names of the result codes and session states for
// the logs, and the wait for a headset the runtime calls unavailable.
//
// November 11: five starts in a row stopped at xrGetSystem with
// XR_ERROR_FORM_FACTOR_UNAVAILABLE (-35) from Meta Quest Link, seconds after
// the launcher's probe had found the headset. The OpenXR specification says
// that code means the headset is supported but not available right now (asleep,
// off the head, Link not started) and that a later call may succeed; Spidy gave
// up on the first one. It now asks again for a while, saying so.
#include <cstdint>
#include <cstdio>
#include <string>

namespace spidy::xr_connect {

// XrResult values (openxr.h), by number, so this header needs no OpenXR.
inline constexpr int32_t formFactorUnavailable = -35;
inline constexpr int32_t sessionLost = -17;
inline constexpr int32_t instanceLost = -13;
inline constexpr int32_t runtimeUnavailable = -51;

inline const char* resultName(int32_t code) {
    switch (code) {
    case 0: return "XR_SUCCESS";
    case 1: return "XR_TIMEOUT_EXPIRED";
    case 3: return "XR_SESSION_LOSS_PENDING";
    case 4: return "XR_EVENT_UNAVAILABLE";
    case 7: return "XR_SPACE_BOUNDS_UNAVAILABLE";
    case 8: return "XR_SESSION_NOT_FOCUSED";
    case 9: return "XR_FRAME_DISCARDED";
    case -1: return "XR_ERROR_VALIDATION_FAILURE";
    case -2: return "XR_ERROR_RUNTIME_FAILURE";
    case -3: return "XR_ERROR_OUT_OF_MEMORY";
    case -4: return "XR_ERROR_API_VERSION_UNSUPPORTED";
    case -6: return "XR_ERROR_INITIALIZATION_FAILED";
    case -7: return "XR_ERROR_FUNCTION_UNSUPPORTED";
    case -8: return "XR_ERROR_FEATURE_UNSUPPORTED";
    case -9: return "XR_ERROR_EXTENSION_NOT_PRESENT";
    case -10: return "XR_ERROR_LIMIT_REACHED";
    case -11: return "XR_ERROR_SIZE_INSUFFICIENT";
    case -12: return "XR_ERROR_HANDLE_INVALID";
    case -13: return "XR_ERROR_INSTANCE_LOST";
    case -14: return "XR_ERROR_SESSION_RUNNING";
    case -16: return "XR_ERROR_SESSION_NOT_RUNNING";
    case -17: return "XR_ERROR_SESSION_LOST";
    case -18: return "XR_ERROR_SYSTEM_INVALID";
    case -19: return "XR_ERROR_PATH_INVALID";
    case -20: return "XR_ERROR_PATH_COUNT_EXCEEDED";
    case -25: return "XR_ERROR_SWAPCHAIN_RECT_INVALID";
    case -26: return "XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED";
    case -28: return "XR_ERROR_SESSION_NOT_READY";
    case -29: return "XR_ERROR_SESSION_NOT_STOPPING";
    case -30: return "XR_ERROR_TIME_INVALID";
    case -31: return "XR_ERROR_REFERENCE_SPACE_UNSUPPORTED";
    case -32: return "XR_ERROR_FILE_ACCESS_ERROR";
    case -33: return "XR_ERROR_FILE_CONTENTS_INVALID";
    case -34: return "XR_ERROR_FORM_FACTOR_UNSUPPORTED";
    case -35: return "XR_ERROR_FORM_FACTOR_UNAVAILABLE";
    case -36: return "XR_ERROR_API_LAYER_NOT_PRESENT";
    case -37: return "XR_ERROR_CALL_ORDER_INVALID";
    case -38: return "XR_ERROR_GRAPHICS_DEVICE_INVALID";
    case -41: return "XR_ERROR_VIEW_CONFIGURATION_TYPE_UNSUPPORTED";
    case -42: return "XR_ERROR_ENVIRONMENT_BLEND_MODE_UNSUPPORTED";
    case -46: return "XR_ERROR_ACTIONSET_NOT_ATTACHED";
    case -50: return "XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING";
    case -51: return "XR_ERROR_RUNTIME_UNAVAILABLE";
    default: return code < 0 ? "XR_ERROR (unnamed)" : "XR (unnamed)";
    }
}

// XrSessionState values, by number.
inline const char* stateName(int32_t state) {
    switch (state) {
    case 0: return "UNKNOWN";
    case 1: return "IDLE";
    case 2: return "READY";
    case 3: return "SYNCHRONIZED";
    case 4: return "VISIBLE";
    case 5: return "FOCUSED";
    case 6: return "STOPPING";
    case 7: return "LOSS_PENDING";
    case 8: return "EXITING";
    default: return "?";
    }
}

// "Find headset XrResult=-35 (XR_ERROR_FORM_FACTOR_UNAVAILABLE)": the code as
// before, so old reports still read the same, with its name.
inline std::string describe(const char* where, int32_t code) {
    return std::string(where) + " XrResult=" + std::to_string(code) + " (" + resultName(code) + ")";
}

// Whether a failed call may succeed if asked again a little later.
inline bool retryable(int32_t code) {
    return code == formFactorUnavailable;
}

// The wait for an unavailable headset: quick at first (Link wakes in a second
// or two), then once a second, for at most limitSeconds.
struct Wait {
    double limitSeconds = 120;
    double firstDelay = .25, maxDelay = 1;
    double delay(unsigned attempt) const {
        double d = firstDelay;
        for (unsigned i = 1; i < attempt && d < maxDelay; ++i)
            d *= 2;
        return d < maxDelay ? d : maxDelay;
    }
};

// Calls call() until it returns other than a retryable code, the wait's limit
// passes, or keepGoing(seconds waited, attempt, last code) says to stop.
// sleep(seconds) waits (a test passes a fake clock). Returns the last code;
// waited and attempts say how long it took.
template <class Call, class Sleep, class KeepGoing>
int32_t retry(Call&& call, Sleep&& sleep, KeepGoing&& keepGoing, const Wait& wait, double& waited,
              unsigned& attempts) {
    waited = 0;
    attempts = 1;
    int32_t code = call();
    while (retryable(code) && waited < wait.limitSeconds) {
        if (!keepGoing(waited, attempts, code))
            break;
        const double d = wait.delay(attempts);
        sleep(d);
        waited += d;
        ++attempts;
        code = call();
    }
    return code;
}

// The player's line while waiting, by runtime ("Meta Quest Link" and the like).
inline std::string waitingLine(const char* runtime, double seconds, double limit) {
    char text[320];
    std::snprintf(text, sizeof(text),
                  "Waiting for the headset: %s says it is not available right now "
                  "(XR_ERROR_FORM_FACTOR_UNAVAILABLE). Put the headset on and make sure Link is "
                  "connected and active. %.0f of %.0f s",
                  runtime && *runtime ? runtime : "the VR runtime", seconds, limit);
    return text;
}

// November 17: Meta's own logs settled the November 11 to 17 waits. The launcher's check found the headset; seconds
// later the USB Link connection dropped (Meta: "detached event with error code -4503: Flaky cable") and came back
// asking for Link to be started again in the headset, and the game asked in between. The headset was gone, not
// refused. The launcher checks again just before the game asks (run_game_vr.py), and the wait says what happened.
inline constexpr const char* linkDropped =
    "The Link connection dropped. Put the headset on, re-enable Link (or reconnect the cable), and the game will "
    "continue.";

// Whether the runtime is Meta's (Quest Link or Air Link): it calls itself "Oculus".
inline bool metaRuntime(const char* runtime) {
    if (!runtime)
        return false;
    const std::string name(runtime);
    return name.find("Oculus") != std::string::npos || name.find("Meta") != std::string::npos;
}

// The player's line while waiting after the launcher's check had found the headset: the connection dropped in
// between. Under 256 characters (XrData::message).
inline std::string droppedLine(const char* runtime, double seconds, double limit) {
    char text[256];
    if (metaRuntime(runtime))
        std::snprintf(text, sizeof(text), "%s (%.0f of %.0f s)", linkDropped, seconds, limit);
    else
        std::snprintf(text, sizeof(text),
                      "The headset's connection dropped. Put the headset on, reconnect it in %s, and the game will "
                      "continue. (%.0f of %.0f s)",
                      runtime && *runtime ? runtime : "its VR app", seconds, limit);
    return text;
}

// When a long wait starts over with a new OpenXR instance (xr_session.cpp): a runtime that lost its headset may
// keep answering an instance made before the loss as it did then. First at `first` seconds, then each `every`.
struct Renew {
    double first = 20, every = 30;
    double next = first;
    bool due(double waited) {
        if (waited < next)
            return false;
        next = waited + every;
        return true;
    }
};

} // namespace spidy::xr_connect
