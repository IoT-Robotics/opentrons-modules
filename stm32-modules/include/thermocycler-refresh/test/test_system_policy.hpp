#pragma once
#include <array>

#include "systemwide.h"
#include "thermocycler-refresh/errors.hpp"

class TestSystemPolicy {
  private:
    bool entered = false;
    bool serial_number_set = false;
    static constexpr std::size_t SYSTEM_SERIAL_NUMBER_LENGTH =
        SYSTEM_WIDE_SERIAL_NUMBER_LENGTH;
    std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> system_serial_number = {};
    errors::ErrorCode set_serial_number_return = errors::ErrorCode::NO_ERROR;

  public:
    auto enter_bootloader() -> void;
    auto reset_bootloader_entered() -> void;
    auto bootloader_entered() const -> bool;
    auto set_serial_number(
        std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH> new_system_serial_number)
        -> errors::ErrorCode;
    auto get_serial_number(void)
        -> std::array<char, SYSTEM_SERIAL_NUMBER_LENGTH>;
};
