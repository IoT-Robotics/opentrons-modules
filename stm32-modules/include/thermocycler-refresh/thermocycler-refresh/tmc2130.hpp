/**
 * @file tmc2130.hpp
 * @brief Interface to control a TMC2130 IC
 */
#pragma once

#include <bitset>
#include <concepts>
#include <cstdint>
#include <optional>

#include "systemwide.h"

namespace tmc2130 {

// Register mapping

enum class Registers {
    GCONF = 0x00,
    GSTAT = 0x01,
    IOIN = 0x04,
    IHOLD_IRUN = 0x10,
    TPOWERDOWN = 0x11,
    TSTEP = 0x12,
    TPWMTHRS = 0x13,
    TCOOLTHRS = 0x14,
    THIGH = 0x15,
    XDIRECT = 0x2D,
    VDCMIN = 0x33,
    CHOPCONF = 0x6C,
    COOLCONF = 0x6D,
    DCCTRL = 0x6E,
    DRVSTATUS = 0x6F,
    PWMCONF = 0x70,
    ENCM_CTRL = 0x72,
    MSLUT_0 = 0x60,
    MSLUT_1 = 0x61,
    MSLUT_2 = 0x62,
    MSLUT_3 = 0x63,
    MSLUT_4 = 0x64,
    MSLUT_5 = 0x65,
    MSLUT_6 = 0x66,
    MSLUT_7 = 0x67,
    MSLUTSEL = 0x68,
    MSLUTSTART = 0x69,
    MSCNT = 0x6A,
    MSCURACT = 0x6B,
    PWM_SCALE = 0x71,
    LOST_STEPS = 0x73,
};

struct GConfig {
    static constexpr Registers address = Registers::GCONF;
    static constexpr bool readable = true;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 17;

    uint8_t i_scale_analog : 1 = 0;
    uint8_t internal_rsense : 1 = 0;
    uint8_t en_pwm_mode : 1 = 0;
    uint8_t enc_commutation : 1 = 0;  // MUST be 0
    uint8_t shaft : 1 = 0;
    uint8_t diag0_error : 1 = 0;
    uint8_t diag0_otpw : 1 = 0;
    uint8_t diag0_stall : 1 = 0;
    uint8_t diag1_stall : 1 = 0;
    uint8_t diag1_index : 1 = 0;
    uint8_t diag1_onstate : 1 = 0;
    uint8_t diag1_steps_skipped : 1 = 0;
    uint8_t diag0_int_pushpull : 1 = 0;
    uint8_t diag1_pushpull : 1 = 0;
    uint8_t small_hysteresis : 1 = 0;
    uint8_t stop_enable : 1 = 0;
    uint8_t direct_mode : 1 = 0;
    uint8_t test_mode : 1 = 0;  // MUST be 0
} __attribute__((packed));

struct GStatus {
    static constexpr Registers address = Registers::GSTAT;
    static constexpr bool readable = true;
    static constexpr bool writable = false;
    static constexpr uint64_t bitlen = 3;

    uint8_t reset : 1 = 0;
    uint8_t driver_error : 1 = 0;
    uint8_t undervoltage_error : 1 = 0;
} __attribute__((packed));

struct CurrentControl {
    static constexpr Registers address = Registers::IHOLD_IRUN;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 20;

    uint8_t hold_current : 5 = 0;
    uint8_t bit_padding_1 : 3 = 0;
    uint8_t run_current : 5 = 0;
    uint8_t bit_padding_2 : 3 = 0;
    uint8_t hold_current_delay : 4 = 0;
} __attribute__((packed));

/**
 * This is the time to delay between
 */
struct PowerDownDelay {
    static constexpr Registers address = Registers::TPOWERDOWN;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 8;

    static constexpr double max_time = 4.0F;
    static constexpr uint64_t max_val = 0xFF;

    auto seconds() -> double {
        return (static_cast<double>(time) / static_cast<double>(max_val)) *
               max_time;
    }
    auto set_time(double seconds) -> bool {
        if (seconds > max_time) {
            return false;
        }
        time = static_cast<uint8_t>((seconds / max_time) *
                                    static_cast<double>(max_val));
        return true;
    }

    uint8_t time = 0;
} __attribute__((packed));

/**
 * This is the threshold velocity for switching on smart energy coolStep
 * and stallGuard.
 */
struct TCoolThreshold {
    static constexpr Registers address = Registers::TCOOLTHRS;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 20;

    uint32_t threshold : 20 = 0;
} __attribute__((packed));

/**
 * This is the velocity threshold at which the controller will automatically
 * move into a different chopper mode w/ fullstepping to maximize torque,
 * applied whenever TSTEP < THIGH
 */
struct THigh {
    static constexpr Registers address = Registers::THIGH;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 20;

    uint32_t threshold : 20 = 0;
} __attribute__((packed));

/**
 * The CHOPCONFIG register contains a number of configuration options for the
 * Chopper control.
 */
struct ChopConfig {
    static constexpr Registers address = Registers::CHOPCONF;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 31;

    /** 0 = Driver disable
     *  1 = "use only with TBL >= 2"
     *  2...15 sets duration of slow decay phase:
     * Nclk = 12 + 32 * TOFF
     */
    uint8_t toff : 4 = 0;
    /**
     * CHM = 0: sets hysteresis start value added to HEND
     *  - Add 1,2,...,8 to hysteresis low value HEND
     *
     * CHM = 1: sets fast decay time, TFD :
     *  - Nclk = 32*HSTRT
     */
    uint8_t hstrt : 3 = 0;
    /**
     * CHM = 0: Hysteresis is -3, -2, -1, ... 12. This is the hysteresis
     * value used for the hysteresis chopper
     *
     * CHM = 1: Sine wave offset. 1/512 of the val gets added to abs of each
     * sine wave entry
     */
    uint8_t hend : 4 = 0;
    /**
     * CHM = 1: MSB of fast decay time setting TFD
     */
    uint8_t fd3 : 1 = 0;
    /**
     * CHM = 1: Fast decay mode. Set to 1 to disable current comparator
     * usage for termination of fast decay cycle.
     */
    uint8_t disfdcc : 1 = 0;
    /**
     * 0 = chopper OFF time fixed as set by TOFF (aka hstrt and fd3)
     *
     * 1 = Random mode, TOFF is modulated by [-12,3] clocks
     */
    uint8_t rndtf : 1 = 0;
    /**
     * Chopper mode. 0 = standard mode, 1 = constant off-time with fast decay.
     */
    uint8_t chm : 1 = 0;
    /**
     * Blank Time Select. Sets comparator blank time to 16,24,36,54
     */
    uint8_t tbl : 2 = 2;
    /**
     * 0 = low sensitivity, high sense resistor voltage
     *
     * 1 = high sensitivity, low sense resistor voltage
     */
    uint8_t vsense : 1 = 0;
    /**
     * High velocity fullstep selection: Enables switching to fullstep when
     * VHIGH is exceeded. Only switches at 45º position.
     */
    uint8_t vhighfs : 1 = 0;
    /**
     * High Velocity Chopper Mode: Enables switching to chm=1 and fd=0 when
     * VHIGH is exceeded. If set, the TOFF setting automatically becomes
     * doubled during high velocity operation.
     */
    uint8_t vhighchm : 1 = 0;
    /**
     * SYNC PWM synchronization clock: Allows synchroniation of the chopper
     * for both phases of a two phase motor to avoid occurrence of a beat.
     * Automatically switched off above VHIGH
     *
     * 0 = disabled
     *
     * 1...15 = synchronized with fsync = fclk/(sync*64)
     */
    uint8_t sync : 4 = 0;
    /**
     * Microstep resolution:
     *
     * 0 = native 256 microstep setting
     *
     * 0b1...0b1000 = 128,64,32,16,8,4,2,FULLSTEP
     *
     * Reduced microstep resolution for STEP/DIR operation. Resolution gives
     * the number of microstep entries per sine quarter wave.
     */
    uint8_t mres : 4 = 0;
    /**
     * Interpolation to 256 microsteps: If set, the actual MRES becomes
     * extrapolated to 256 usteps for smoothest motor operation
     */
    uint8_t intpol : 1 = 0;
    /**
     * Enable double edge step pulses: If set, enable step impulse at each
     * step edge to reduce step frequency requirement
     */
    uint8_t dedge : 1 = 0;
    /**
     * Short to GND protectino disable:
     *
     * 0 = short to gnd protection on
     *
     * 1 = short to gnd protection disabled
     */
    uint8_t diss2g : 1 = 0;
} __attribute__((packed));

/**
 * COOLCONF contains information to configure the Coolstep and Smartguard (SG)
 * features in the TMC2130
 */
struct CoolConf {
    static constexpr Registers address = Registers::COOLCONF;
    static constexpr bool readable = false;
    static constexpr bool writable = true;
    static constexpr uint64_t bitlen = 25;

    /**
     * Minimum SG value for smart current control & smart current enable.
     *
     * If SG result falls below SEMIN*32, motor current increases to
     * reduce motor load angle.
     *
     * 0 = smart current control coolStep OFF
     *
     * 1..15 = set threshold value
     */
    uint8_t semin : 4 = 0;
    uint8_t padding_1 : 1 = 0;
    /**
     * Current up step width: Current increment steps per measured SG value:
     *
     * 1,2,4,8
     */
    uint8_t seup : 2 = 0;
    uint8_t padding_2 : 1 = 0;
    /**
     * If the SG result is >= (SEMIN+SEMAX+1)*32, motor current decreases
     * to save energy.
     */
    uint8_t semax : 4 = 0;
    uint8_t padding_3 : 1 = 0;
    /**
     * Current down step speed:
     *
     * 0: for each 32 SG values, decrease by one
     *
     * 1: for each 8 SG values, decrease by one
     *
     * 2: for each 2 SG values, decrease by one
     *
     * 3: for each SG value, decrease by one
     */
    uint8_t sedn : 2 = 0;
    /**
     * Minimum current for smart current control:
     *
     * 0 = 1/2 of current setting in IRUN
     *
     * 1 = 1/4 of current setting in IRUN
     */
    uint8_t seimin : 1 = 0;
    /**
     * SG Threshold Value: This signed value controlls SG level for stall
     * output and sets optimum measurement range for readout. A lower val
     * gives a higher sensitivity. Zero is starting value working with most
     * motors.
     *
     * -64 to +63: Higher value makes SG less sensitive and requires more
     * torque to indicate a stall
     */
    int8_t sgt : 7 = 0;
    uint8_t padding_4 : 1 = 0;
    /**
     * SG filter enable:
     *
     * 0 = standard mode, high time res for SG
     *
     * 1 = filtered mode, SG signal updated for each 4 full steps
     */
    uint8_t sfilt : 1 = 0;
} __attribute__((packed));

/** Hardware abstraction policy for the TMC2130 communication.*/
template <typename Policy>
concept TMC2130Policy = requires(Policy& p, Registers addr, uint64_t value) {
    // A function to write to a register
    { p.write_register(addr, value) } -> std::same_as<bool>;
    // A function to read from a register
    { p.read_register(addr) } -> std::same_as<std::optional<uint64_t>>;
    // TODO a function to commence a movement
};

/** Template concept to constrain what structures encapsulate registers.*/
template <typename Reg>
concept TMC2130Register = requires(Reg& r, uint64_t value) {
    // Struct has a valid register address
    std::same_as<decltype(Reg::address), Registers&>;
    // Struct has a bool saying whether it can be read
    std::same_as<decltype(Reg::readable), bool>;
    // Struct has a bool saying whether it can be written
    std::same_as<decltype(Reg::writable), bool>;
    // Struct has an integer with the total number of bits in a register
    std::integral<decltype(Reg::bitlen)>;
};

class TMC2130 {
  public:
    /**
     * @brief Set a register on the TMC2130
     *
     * @tparam Reg The type of register to set
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @param[in] reg The register to write
     * @return True if the register could be written, false otherwise.
     * Attempts to write to an unwirteable register will throw a static
     * assertion.
     */
    template <TMC2130Register Reg, TMC2130Policy Policy>
    auto set_register(Policy& policy, Reg& reg) -> bool {
        static_assert(Reg::writable);
        auto serialized = *reinterpret_cast<uint64_t*>(&reg);
        serialized &= ((uint64_t)1 << Reg::bitlen) - 1;
        return policy.write_register(reg.address, serialized);
    }
    /**
     * @brief Read a register on the TMC2130
     *
     * @tparam Reg The type of register to read
     * @tparam Policy Abstraction class for actual writing
     * @param[in] policy Instance of the abstraction policy to use
     * @return The contents of the register, or nothing if the register
     * can't be read.
     */
    template <TMC2130Register Reg, TMC2130Policy Policy>
    auto get_register(Policy& policy) -> std::optional<Reg> {
        using RT = std::optional<Reg>;
        static_assert(Reg::readable);
        auto ret = policy.read_register(Reg::address);
        if (!ret.has_value()) {
            return RT();
        }
        return RT(*reinterpret_cast<Reg*>(&ret.value()));
    }
};
}  // namespace tmc2130
