#include "thermocycler-refresh/errors.hpp"

using namespace errors;

const char* const NO_ERROR = "";
const char* const USB_TX_OVERRUN = "ERR001:tx buffer overrun\n";
const char* const INTERNAL_QUEUE_FULL = "ERR002:internal queue full\n";
const char* const UNHANDLED_GCODE = "ERR003:unhandled gcode\n";
const char* const GCODE_CACHE_FULL = "ERR004:gcode cache full\n";
const char* const BAD_MESSAGE_ACKNOWLEDGEMENT =
    "ERR005:bad message acknowledgement\n";
const char* const SYSTEM_SERIAL_NUMBER_INVALID =
    "ERR301:system:serial number invalid format\n";
const char* const SYSTEM_SERIAL_NUMBER_HAL_ERROR =
    "ERR302:system:HAL error, busy, or timeout\n";

const char* const UNKNOWN_ERROR = "ERR-1:unknown error code\n";

// NOLINTNEXTLINE(cppcoreguidelines-macro-usage)
#define HANDLE_CASE(errname) \
    case ErrorCode::errname: \
        return errname

auto errors::errorstring(ErrorCode code) -> const char* {
    switch (code) {
        HANDLE_CASE(NO_ERROR);
        HANDLE_CASE(USB_TX_OVERRUN);
        HANDLE_CASE(INTERNAL_QUEUE_FULL);
        HANDLE_CASE(UNHANDLED_GCODE);
        HANDLE_CASE(GCODE_CACHE_FULL);
        HANDLE_CASE(BAD_MESSAGE_ACKNOWLEDGEMENT);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_INVALID);
        HANDLE_CASE(SYSTEM_SERIAL_NUMBER_HAL_ERROR);
    }
    return UNKNOWN_ERROR;
}