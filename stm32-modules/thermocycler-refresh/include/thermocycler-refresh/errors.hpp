#pragma once
#include <charconv>
#include <cstdint>

#include "thermocycler-refresh/utility.hpp"

namespace errors {

enum class ErrorCode {
    NO_ERROR = 0,
    USB_TX_OVERRUN = 1,
    INTERNAL_QUEUE_FULL = 2,
    UNHANDLED_GCODE = 3,
    GCODE_CACHE_FULL = 4,
    BAD_MESSAGE_ACKNOWLEDGEMENT = 5,
    SYSTEM_SERIAL_NUMBER_INVALID = 301,
    SYSTEM_SERIAL_NUMBER_HAL_ERROR = 302,
};

auto errorstring(ErrorCode code) -> const char*;

template <typename Input, typename Limit>
requires std::forward_iterator<Input> && std::sized_sentinel_for<Limit, Input>
constexpr auto write_into(Input start, Limit end, ErrorCode code) -> Input {
    const char* str = errorstring(code);
    return write_string_to_iterpair(start, end, str);
}
};  // namespace errors