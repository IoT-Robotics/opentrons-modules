/*
 * the primary interface to the host communications task
 */
#pragma once

#include <concepts>
#include <variant>

#include "hal/message_queue.hpp"
#include "heater-shaker/ack_cache.hpp"
#include "heater-shaker/messages.hpp"
#include "heater-shaker/tasks.hpp"
#include "heater-shaker/version.hpp"
#include "systemwide.h"

namespace tasks {
template <template <class> class QueueImpl>
struct Tasks;
};

namespace system_task {

template <typename Policy>
concept SystemExecutionPolicy = requires(Policy& p, const Policy& cp) {
    {p.enter_bootloader()};
    {
        p.set_serial_number(std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>{
            "TESTSNXxxxxxxxxxxxxxxxx"})
        } -> std::same_as<errors::ErrorCode>;
    {
        p.get_serial_number()
        } -> std::same_as<std::array<char, SYSTEM_WIDE_SERIAL_NUMBER_LENGTH>>;
    {p.start_set_led(LED_MODE::WHITE_ON)} -> std::same_as<errors::ErrorCode>;
};

struct LEDBlinkState {
    enum LEDBlinkTaskStatus {
        BLINK_OFF = 0,
        BLINK_ON_WAITING = 1,
        BLINK_OFF_WAITING = 2
    };
    LEDBlinkTaskStatus status;
};

using Message = messages::SystemMessage;

// By using a template template parameter here, we allow the code instantiating
// this template to do so as SystemTask<SomeQueueImpl> rather than
// SystemTask<SomeQueueImpl<Message>>
template <template <class> class QueueImpl>
requires MessageQueue<QueueImpl<Message>, Message>
class SystemTask {
    static constexpr const uint32_t LED_BLINK_WAIT_MS = 500;
    using BootloaderPrepAckCache =
        AckCache<3, messages::SetTemperatureMessage, messages::SetRPMMessage,
                 messages::ForceUSBDisconnectMessage>;

  public:
    using Queue = QueueImpl<Message>;
    explicit SystemTask(Queue& q)
        : led_blink_state{.status = LEDBlinkState::BLINK_OFF},
          message_queue(q),
          task_registry(nullptr),
          // NOLINTNEXTLINE(readability-redundant-member-init)
          prep_cache() {}
    SystemTask(const SystemTask& other) = delete;
    auto operator=(const SystemTask& other) -> SystemTask& = delete;
    SystemTask(SystemTask&& other) noexcept = delete;
    auto operator=(SystemTask&& other) noexcept -> SystemTask& = delete;
    ~SystemTask() = default;
    auto get_message_queue() -> Queue& { return message_queue; }
    void provide_tasks(tasks::Tasks<QueueImpl>* other_tasks) {
        task_registry = other_tasks;
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto run_once(Policy& policy) -> void {
        auto message = Message(std::monostate());

        message_queue.recv(&message);

        auto visit_helper = [this, &policy](auto& message) -> void {
            this->visit_message(message, policy);
        };
        std::visit(visit_helper, message);
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::EnterBootloaderMessage& message,
                       Policy& policy) -> void {
        // When we go into the bootloader, we're going to do a system reset
        // essentially - we want to undo our clock setup, gate off clocks to
        // peripherals, in general try and make the system look like it just
        // booted. We'd like to not abruptly shut off a bunch of hardware when
        // this happens, so let's try and turn off the rest of the hardware
        // nicely just in case.
        auto stop_message = messages::SetRPMMessage{
            .id = 0, .target_rpm = 0, .from_system = true};
        auto stop_id = prep_cache.add(stop_message);
        stop_message.id = stop_id;
        if (!task_registry->motor->get_message_queue().try_send(stop_message,
                                                                1)) {
            prep_cache.remove_if_present(stop_id);
        }

        auto cool_message = messages::SetTemperatureMessage{
            .id = 0, .target_temperature = 0, .from_system = true};
        auto cool_id = prep_cache.add(cool_message);
        cool_message.id = cool_id;
        if (!task_registry->heater->get_message_queue().try_send(cool_message,
                                                                 1)) {
            prep_cache.remove_if_present(cool_id);
        }

        auto disconnect_message = messages::ForceUSBDisconnectMessage{.id = 0};
        auto disconnect_id = prep_cache.add(disconnect_message);
        disconnect_message.id = disconnect_id;
        if (!task_registry->comms->get_message_queue().try_send(
                disconnect_message, 1)) {
            prep_cache.remove_if_present(disconnect_id);
        }

        auto ack_message =
            messages::AcknowledgePrevious{.responding_to_id = message.id};
        static_cast<void>(
            task_registry->comms->get_message_queue().try_send(ack_message, 1));

        // Somehow we couldn't send any of the messages, maybe system deadlock?
        // Enter bootloader regardless
        if (prep_cache.empty()) {
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const messages::AcknowledgePrevious& message,
                       Policy& policy) -> void {
        // handle an acknowledgement for one of the prep tasks we've dispatched
        auto cache_entry =
            prep_cache.remove_if_present(message.responding_to_id);
        // See if the ack has an error in it so we can forward if necessary
        auto error_result = std::visit(
            [&policy, &message](auto cache_element) -> errors::ErrorCode {
                using T = std::decay_t<decltype(cache_element)>;
                if constexpr (std::is_same_v<std::monostate, T>) {
                    return errors::ErrorCode::BAD_MESSAGE_ACKNOWLEDGEMENT;
                }
                return message.with_error;
            },
            cache_entry);
        if (error_result != errors::ErrorCode::NO_ERROR) {
            auto error_message = messages::ErrorMessage{.code = error_result};
            static_cast<void>(
                task_registry->comms->get_message_queue().try_send(
                    error_message, 1));
        }
        // No remaining setup tasks, enter bootloader
        if (prep_cache.empty()) {
            policy.enter_bootloader();
        }
    }

    template <typename Policy>
    auto visit_message(const messages::SetSerialNumberMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        response.with_error = policy.set_serial_number(msg.serial_number);
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::GetSystemInfoMessage& msg,
                       Policy& policy) -> void {
        auto response = messages::GetSystemInfoResponse{
            .responding_to_id = msg.id,
            .serial_number = policy.get_serial_number(),
            .fw_version = version::fw_version(),
            .hw_version = version::hw_version()};
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::SetLEDMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (!policy.check_I2C_ready()) {
            response.with_error = errors::ErrorCode::SYSTEM_LED_I2C_NOT_READY;
        } else {
            response.with_error = policy.start_set_led(msg.mode);
        }
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::IdentifyModuleStartLEDMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (!policy.check_I2C_ready()) {
            response.with_error = errors::ErrorCode::SYSTEM_LED_I2C_NOT_READY;
        } else {
            response.with_error = policy.start_set_led(LED_MODE::WHITE_ON);
        }
        led_blink_state.status = LEDBlinkState::BLINK_ON_WAITING;
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
        policy.delay_time_ms(LED_BLINK_WAIT_MS);
        static_cast<void>(get_message_queue().try_send(
            messages::CheckLEDBlinkStatusMessage{}));
    }

    template <typename Policy>
    auto visit_message(const messages::IdentifyModuleStopLEDMessage& msg,
                       Policy& policy) -> void {
        auto response =
            messages::AcknowledgePrevious{.responding_to_id = msg.id};
        if (!policy.check_I2C_ready()) {
            response.with_error = errors::ErrorCode::SYSTEM_LED_I2C_NOT_READY;
        } else {
            response.with_error = policy.start_set_led(LED_MODE::WHITE_OFF);
        }
        led_blink_state.status = LEDBlinkState::BLINK_OFF;
        static_cast<void>(task_registry->comms->get_message_queue().try_send(
            messages::HostCommsMessage(response)));
    }

    template <typename Policy>
    auto visit_message(const messages::CheckLEDBlinkStatusMessage& msg,
                       Policy& policy) -> void {
        bool bStatus = true;
        if ((led_blink_state.status == LEDBlinkState::BLINK_ON_WAITING) || (led_blink_state.status == LEDBlinkState::BLINK_OFF_WAITING)) {
            if (!policy.check_I2C_ready()) {
                bStatus = false;
                static_cast<void>(
                    task_registry->comms->get_message_queue().try_send(
                        messages::AcknowledgePrevious{
                            .with_error = errors::ErrorCode::SYSTEM_LED_I2C_NOT_READY}));
            }
            if (bStatus) {
                if (led_blink_state.status == LEDBlinkState::BLINK_ON_WAITING) {
                    if (policy.start_set_led(LED_MODE::WHITE_OFF) != errors::ErrorCode::NO_ERROR) {
                        bStatus = false;
                        static_cast<void>(
                            task_registry->comms->get_message_queue().try_send(
                                messages::AcknowledgePrevious{
                                    .with_error = errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR}));
                    }
                    if (bStatus) {
                        led_blink_state.status = LEDBlinkState::BLINK_OFF_WAITING;
                    }
                } else if (led_blink_state.status == LEDBlinkState::BLINK_OFF_WAITING) {
                    if (policy.start_set_led(LED_MODE::WHITE_ON) != errors::ErrorCode::NO_ERROR) {
                        bStatus = false;
                        static_cast<void>(
                            task_registry->comms->get_message_queue().try_send(
                                messages::AcknowledgePrevious{
                                    .with_error = errors::ErrorCode::SYSTEM_LED_TRANSMIT_ERROR}));
                    }
                    if (bStatus) {
                        led_blink_state.status = LEDBlinkState::BLINK_ON_WAITING;
                    }
                }
                if (bStatus) {
                    policy.delay_time_ms(LED_BLINK_WAIT_MS);
                    static_cast<void>(get_message_queue().try_send(
                        messages::CheckLEDBlinkStatusMessage{}));
                }
            }
        }
    }

    template <typename Policy>
    requires SystemExecutionPolicy<Policy>
    auto visit_message(const std::monostate& message, Policy& policy) -> void {
        static_cast<void>(message);
        static_cast<void>(policy);
    }

  private:
    LEDBlinkState led_blink_state;
    Queue& message_queue;
    tasks::Tasks<QueueImpl>* task_registry;
    BootloaderPrepAckCache prep_cache;
};

};  // namespace system_task
