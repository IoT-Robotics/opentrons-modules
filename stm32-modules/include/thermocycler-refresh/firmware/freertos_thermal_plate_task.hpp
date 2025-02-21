/*
 * Interface for the firmware-specific parts of the thermal plate control task
 */
#pragma once

#include "FreeRTOS.h"
#include "firmware/freertos_message_queue.hpp"
#include "task.h"
#include "thermocycler-refresh/tasks.hpp"

namespace thermal_plate_control_task {
// Function that starts the task
auto start()
    -> tasks::Task<TaskHandle_t,
                   thermal_plate_task::ThermalPlateTask<FreeRTOSMessageQueue>>;
}  // namespace thermal_plate_control_task
