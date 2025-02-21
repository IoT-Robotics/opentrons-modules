#include "stm32f3xx_hal.h"
#include "stm32f3xx_hal_rcc.h"
#include "stm32f3xx_it.h"
#include "stm32f3xx_hal_cortex.h"
#include "system_hardware.h"
#include "systemwide.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#define BASE_PWM_REGISTER               0x04
#define UPDATE_REGISTER                 0x13
#define BASE_WHITE_REGISTER             0x17 //left white LED is on driver channel 4
#define BASE_RED_REGISTER               0x1A //right red LED is on driver channel 7
#define SHUTDOWN_REGISTER               0x00
#define REGISTER_SIZE                   0x01

#define LED_OUT_HI                      0x30 //full current output
#define LED_PWM_OUT_HI                  0xFF //full pwm output
#define LED_OUT_MID                     0x13 //low current output
#define LED_PWM_OUT_MID                 0x4B //low pwm output

#define I2C_ADDRESS        0x6C
/* I2C TIMING Register is defined when I2C clock source is SYSCLK (true) */
/* I2C TIMING is calculated using SYSCLK = 72 MHz, I2C Speed Frequency = 100 KHz, Rise Time = 100ns, and Fall Time = 100ns */
#define I2C_TIMING      0x00201D2B

static TaskHandle_t xTaskToNotify = NULL;
I2C_HandleTypeDef I2cHandle;

uint8_t PWMInitBuffer[SYSTEM_WIDE_TXBUFFERSIZE] = {LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI, LED_PWM_OUT_HI};
uint8_t OutputInitBuffer[SYSTEM_WIDE_TXBUFFERSIZE] = {LED_OUT_HI, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
uint8_t UpdateBuffer[1] = {0x00};
uint8_t ShutdownBuffer[1] = {0x01};
uint8_t WhiteOnBuffer[1] = {LED_OUT_HI};
uint8_t WhiteOffBuffer[1] = {0x00};
uint8_t RedOnBuffer[9] = {LED_OUT_HI};
uint8_t RedOffBuffer[9] = {0x00};
bool CallbackStatus = false;

void system_hardware_setup(void) {
    GPIO_InitTypeDef gpio_init = {
      .Pin = SOFTPOWER_BUTTON_SENSE_PIN | SOFTPOWER_UNPLUG_SENSE_PIN,
      .Mode = GPIO_MODE_INPUT,
      .Pull = GPIO_NOPULL,
      .Speed = GPIO_SPEED_FREQ_LOW,
    };
    __HAL_RCC_GPIOB_CLK_ENABLE();
    HAL_GPIO_Init(SOFTPOWER_PORT, &gpio_init);

    /*##-1- Configure the I2C peripheral ######################################*/
    I2cHandle.Instance             = I2Cx;
    I2cHandle.Init.Timing          = I2C_TIMING;
    I2cHandle.Init.OwnAddress1     = I2C_ADDRESS;
    I2cHandle.Init.AddressingMode  = I2C_ADDRESSINGMODE_7BIT;
    I2cHandle.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    I2cHandle.Init.OwnAddress2     = 0xFF;
    I2cHandle.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    I2cHandle.Init.NoStretchMode   = I2C_NOSTRETCH_DISABLE;

    HAL_I2C_Init(&I2cHandle);
    HAL_I2CEx_ConfigAnalogFilter(&I2cHandle,I2C_ANALOGFILTER_ENABLE);
}

// This is the start of the sys memory region from the datasheet. It should be the
// same for at least all the f303s, including the vdt6 which is on the nffs and the
// vc that will be on the ff.
#define SYSMEM_START 0x1fffd800
#define SYSMEM_BOOT (SYSMEM_START + 4)

// address 4 in the bootable region is the address of the first instruction that
// should run, aka the data that should be loaded into $pc.
const uint32_t *const sysmem_boot_loc = (uint32_t*)SYSMEM_BOOT;

void system_hardware_enter_bootloader(void) {

  // We have to uninitialize as many of the peripherals as possible, because the bootloader
  // expects to start as the system comes up

  // The HAL has ways to turn off all the core clocking and the clock security system
  HAL_RCC_DisableCSS();
  HAL_RCC_DeInit();

  // systick should be off at boot
  SysTick->CTRL = 0;
  SysTick->LOAD = 0;
  SysTick->VAL = 0;

  // We have to make sure that the processor is mapping the system memory region to address 0,
  // which the bootloader expects
  __HAL_SYSCFG_REMAPMEMORY_SYSTEMFLASH();
  // and now we're ready to set the system up to start executing system flash.
  // arm cortex initialization means that

  // address 0 in the bootable region is the address where the processor should start its stack
  // which we have to do as late as possible because as soon as we do this the c and c++ runtime
  // environment is no longer valid
  __set_MSP(*((uint32_t*)SYSMEM_START));

  // finally, jump to the bootloader. we do this in inline asm because we need
  // this to be a naked call (no caller-side prep like stacking return addresses)
  // and to have a naked function you need to define it as a function, not a
  // function pointer, and we don't statically know the address here since it is
  // whatever's contained in that second word of the bsystem memory region.
asm volatile (
  "bx %0"
  : // no outputs
  : "r" (*sysmem_boot_loc)
  : "memory"  );
}

bool system_hardware_setup_led(void) {
  bool status;

  status = system_hardware_set_led_send(SHUTDOWN_REGISTER, ShutdownBuffer, sizeof(ShutdownBuffer));
  if (status) {
    status = system_hardware_set_led_send(BASE_PWM_REGISTER, PWMInitBuffer, SYSTEM_WIDE_TXBUFFERSIZE);
    if (status) {
      status = system_hardware_set_led_send(BASE_WHITE_REGISTER, OutputInitBuffer, SYSTEM_WIDE_TXBUFFERSIZE);
      if (status) {
        status = system_hardware_set_led_send(UPDATE_REGISTER, UpdateBuffer, sizeof(UpdateBuffer));
      }
    }
  }
  return status;
}

bool system_hardware_set_led(LED_MODE mode) {
  uint16_t register_address;
  uint8_t* set_buffer;
  uint16_t buffer_size;
  bool status;

  switch (mode) {
    case WHITE_ON:
      register_address = BASE_WHITE_REGISTER;
      set_buffer = WhiteOnBuffer;
      buffer_size = sizeof(WhiteOnBuffer);
      break;
    case WHITE_OFF:
      register_address = BASE_WHITE_REGISTER;
      set_buffer = WhiteOffBuffer;
      buffer_size = sizeof(WhiteOffBuffer);
      break;
    case RED_ON:
      register_address = BASE_RED_REGISTER;
      set_buffer = RedOnBuffer;
      buffer_size = sizeof(RedOnBuffer);
      break;
    case RED_OFF:
      register_address = BASE_RED_REGISTER;
      set_buffer = RedOffBuffer;
      buffer_size = sizeof(RedOffBuffer);
      break;
    default:
      register_address = BASE_WHITE_REGISTER;
      set_buffer = OutputInitBuffer;
      buffer_size = sizeof(OutputInitBuffer);
      break;
  }

  status = system_hardware_set_led_send(register_address, set_buffer, buffer_size);
  if (status) {
    status = system_hardware_set_led_send(UPDATE_REGISTER, UpdateBuffer, sizeof(UpdateBuffer));
  }
  return status;
}

bool system_hardware_set_led_send(uint16_t register_address, uint8_t* set_buffer, uint16_t buffer_size) {
  uint32_t ulNotificationValue;
  const TickType_t xMaxBlockTime = pdMS_TO_TICKS( 100 );
  HAL_StatusTypeDef status;

  if (xTaskToNotify != NULL) {
    return false;
  } else {
    xTaskToNotify = xTaskGetCurrentTaskHandle();
    status = HAL_I2C_Mem_Write_IT(&I2cHandle, ((uint16_t)I2C_ADDRESS)<<1, (uint16_t)register_address,
      (uint16_t)REGISTER_SIZE, (uint8_t*)set_buffer, (uint16_t)buffer_size);
    ulNotificationValue = ulTaskNotifyTake( pdTRUE, xMaxBlockTime );
    return ((status == HAL_OK) && (ulNotificationValue == 1) && (I2cHandle.State == HAL_I2C_STATE_READY) && (CallbackStatus));
  }
}

void HAL_I2C_MemTxCpltCallback(I2C_HandleTypeDef *I2cHandle)
{
  system_hardware_handle_i2c_callback();
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *I2cHandle)
{
  CallbackStatus = false;
  system_hardware_handle_i2c_callback();
}

void system_hardware_handle_i2c_callback(void) {
  BaseType_t xHigherPriorityTaskWoken = pdFALSE;
  if (xTaskToNotify == NULL) {
    CallbackStatus = false;
  } else {
    CallbackStatus = true;
    vTaskNotifyGiveFromISR( xTaskToNotify, &xHigherPriorityTaskWoken );
    xTaskToNotify = NULL;
    portYIELD_FROM_ISR( xHigherPriorityTaskWoken );
  }
}

bool system_hardware_I2C_ready(void) {
  return (HAL_I2C_GetState(&I2cHandle) == HAL_I2C_STATE_READY);
}

/******************************************************************************/
/*                 STM32F3xx Peripherals Interrupt Handlers                  */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f3xx.s).                                               */
/******************************************************************************/
/**
  * @brief  This function handles I2C event interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "system_hardware.h" and related to I2C data transmission
  */
void I2Cx_EV_IRQHandler(void)
{
  HAL_I2C_EV_IRQHandler(&I2cHandle);
}

/**
  * @brief  This function handles I2C error interrupt request.
  * @param  None
  * @retval None
  * @Note   This function is redefined in "system_hardware.h" and related to I2C error
  */
void I2Cx_ER_IRQHandler(void)
{
  HAL_I2C_ER_IRQHandler(&I2cHandle);
}
