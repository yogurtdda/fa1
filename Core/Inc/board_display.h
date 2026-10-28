#ifndef BOARD_DISPLAY_H
#define BOARD_DISPLAY_H

#include "stm32f7xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t memory_ready;
    uint32_t memory_test_words;
    uint32_t memory_test_errors;
    uint32_t task_created;
    uint32_t task_started;
    uint32_t frames;
    uint32_t gui_stack_free_words;
    uint32_t rtos_free_heap_bytes;
} BoardDisplayDiagnostics;

extern volatile BoardDisplayDiagnostics display_diag;

/* Called inside MX_FMC_Init, before LTDC or TouchGFX use external RAM. */
HAL_StatusTypeDef Board_DisplayMemoryInit(SDRAM_HandleTypeDef *sdram);
/* Called before starting the scheduler. Uses its own static task storage. */
void Board_DisplayStartTask(void);
/* Called by the GUI thread after a completed frame. */
void Board_DisplayFrameComplete(void);

#ifdef __cplusplus
}
#endif
#endif
