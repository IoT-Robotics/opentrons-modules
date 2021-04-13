/*
 * firmware-specific internals and hooks for heater control
 */

#include "firmware/freertos_heater_task.hpp"

#include <array>

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "heater-shaker/heater_task.hpp"
#include "heater-shaker/tasks.hpp"
#include "task.h"

namespace heater_control_task {

enum class Notifications : uint8_t {
    INCOMING_MESSAGE = 1,
};

static constexpr uint32_t _stack_size = 500;
// Stack as an array because there's no added overhead and why not
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static std::array<StackType_t, _stack_size> _stack;

// internal data structure for freertos to store task state
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static StaticTask_t _data;

static FreeRTOSMessageQueue<heater_task::Message>
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    _heater_queue(static_cast<uint8_t>(Notifications::INCOMING_MESSAGE),
                  "Heater Message Queue");

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
static auto _task = heater_task::HeaterTask(_heater_queue);

// Actual function that runs the task
void run(void *param) {  // NOLINT(misc-unused-parameters)
    static constexpr uint32_t delay_ticks = 100;
    auto *task_class = static_cast<decltype(_task) *>(param);
    static_cast<void>(task_class);
    while (true) {
        vTaskDelay(delay_ticks);
    }
}

// Starter that spins up the thread
auto start() -> tasks::Task<TaskHandle_t,
                            heater_task::HeaterTask<FreeRTOSMessageQueue>> {
    auto *handle = xTaskCreateStatic(run, "HeaterControl", _stack.size(),
                                     &_task, 1, _stack.data(), &_data);
    _heater_queue.provide_handle(handle);
    return tasks::Task<TaskHandle_t, decltype(_task)>{.handle = handle,
                                                      .task = _task};
}
}  // namespace heater_control_task