#include "board_display.h"
#include "board_sdram.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"
#include "app_touchgfx.h"

volatile BoardDisplayDiagnostics display_diag;
static StaticTask_t gui_task_control;
static StackType_t gui_task_stack[4096];
static osThreadId gui_task;

HAL_StatusTypeDef Board_DisplayMemoryInit(SDRAM_HandleTypeDef *sdram)
{
    HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_RESET);
    if (Board_SDRAM_Init(sdram) != HAL_OK) return HAL_ERROR;

    /* Startup-only test: neither LTDC nor TouchGFX is using these buffers yet.
     * Write both buffers before reading to catch aliasing as well as data faults. */
    for (unsigned buffer = 0; buffer < 2; ++buffer) {
        volatile uint16_t *memory = (volatile uint16_t *)(buffer == 0 ?
            BOARD_LCD_FRAMEBUFFER0_ADDRESS : BOARD_LCD_FRAMEBUFFER1_ADDRESS);
        for (uint32_t i = 0; i < BOARD_LCD_FRAMEBUFFER_BYTES / 2U; ++i)
            memory[i] = (uint16_t)(i ^ (buffer ? 0x5AA5U : 0xA55AU));
    }
    __DSB();
    for (unsigned buffer = 0; buffer < 2; ++buffer) {
        volatile uint16_t *memory = (volatile uint16_t *)(buffer == 0 ?
            BOARD_LCD_FRAMEBUFFER0_ADDRESS : BOARD_LCD_FRAMEBUFFER1_ADDRESS);
        for (uint32_t i = 0; i < BOARD_LCD_FRAMEBUFFER_BYTES / 2U; ++i) {
            if (memory[i] != (uint16_t)(i ^ (buffer ? 0x5AA5U : 0xA55AU)))
                ++display_diag.memory_test_errors;
            ++display_diag.memory_test_words;
        }
    }
    if (display_diag.memory_test_errors != 0U) return HAL_ERROR;
    if (Board_SDRAM_ClearFramebuffers() != HAL_OK) return HAL_ERROR;
    display_diag.memory_ready = 1U;
    return HAL_OK;
}

static void DisplayTask(void const *argument)
{
    (void)argument;
    display_diag.task_started = 1U;
    /* RTOS backend: this is a blocking render loop and needs its own task. */
    MX_TouchGFX_Process();
    Error_Handler();
}

void Board_DisplayStartTask(void)
{
    if (!display_diag.memory_ready) Error_Handler();
    osThreadStaticDef(touchgfx, DisplayTask, osPriorityAboveNormal, 0,
                      4096, gui_task_stack, &gui_task_control);
    gui_task = osThreadCreate(osThread(touchgfx), NULL);
    if (gui_task == NULL) Error_Handler();
    display_diag.task_created = 1U;
}

void Board_DisplayFrameComplete(void)
{
    ++display_diag.frames;
    if (display_diag.frames == 2U) {
        /* Show the panel after real rendering has begun. */
        HAL_GPIO_WritePin(LCD_DISP_GPIO_Port, LCD_DISP_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LCD_BL_CTRL_GPIO_Port, LCD_BL_CTRL_Pin, GPIO_PIN_SET);
    }
    if (display_diag.frames % 60U == 0U) {
        display_diag.gui_stack_free_words = uxTaskGetStackHighWaterMark(NULL);
        display_diag.rtos_free_heap_bytes = xPortGetFreeHeapSize();
    }
}
