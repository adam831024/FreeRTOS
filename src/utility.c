/******************************************************************************
 * Copyright (C)
 *
 * NAME:
 *		utility.c
 * DESCRIPTION:
 *
*******************************************************************************/
/******************************************************************************
 * Includes
 *******************************************************************************/
/*Standard include*/
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include "Nano100Series.h" 

/*Free-RTOS include*/
#include "FreeRTOSConfig.h"

/*Application include*/
#include "utility.h"
#include "main.h"
/******************************************************************************
 * Module Preprocessor Constants
 *******************************************************************************/
#define CONSTANT 5

/******************************************************************************
 * Module Preprocessor Macros
 *******************************************************************************/

/******************************************************************************
 * Module Typedefs
 *******************************************************************************/

/******************************************************************************
 * Module Variable Definitions
 *******************************************************************************/
static uint8_t fac_us = 0;
static uint16_t fac_ms = 0;
/******************************************************************************
 * Function Prototypes
 *******************************************************************************/

/******************************************************************************
 * Function Definitions
 *******************************************************************************/
/******************************************************************************
 * @brief     syste tick delay init
 * @return                              void
 *******************************************************************************/
void delay_init(void)
{
    /*systick HCLK，interrupt interval 1ms*/
    SysTick_Config(SystemCoreClock / 1000);
    fac_us = SystemCoreClock / 4000000; // TODO: not sure
    fac_ms = (uint16_t)fac_us * 1000;
#ifdef ADAM_DEBUG
    printf("fac_us = %d, fac_ms = %dr\n", fac_us, fac_ms);
#endif
}

/******************************************************************************
 * @brief     system delay ns
 * @param[in] us                		microseconds
 * @return                              void
 *******************************************************************************/
void delayUs(uint16_t us)
{
    uint32_t temp;
    SysTick->LOAD = (uint32_t)us * fac_us;
    SysTick->VAL = 0x00;
    SysTick->CTRL = 0x01;
    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && (!(temp & (1 << 16))));
    SysTick->CTRL = 0x00;
    SysTick->VAL = 0x00;
}

/******************************************************************************
 * @brief     system delay ms
 * @param[in] ms                		milliseconds
 * @return                              void
 *******************************************************************************/
void delayMs(uint16_t ms)
{
    uint32_t temp;
    SysTick->LOAD = (uint32_t)ms * fac_ms;
    SysTick->VAL = 0x00;
    SysTick->CTRL = 0x01;
    do
    {
        temp = SysTick->CTRL;
    } while ((temp & 0x01) && (!(temp & (1 << 16))));
    SysTick->CTRL = 0x00;
    SysTick->VAL = 0x00;
}
/******************************************************************************
 * DESCRIPTION: malloc
 * @param[in] size
 *      malloc size
 * RETURNS: pointer 
*******************************************************************************/
void *osMalloc(uint16_t size)
{
    void *memory = (void *)pvPortMalloc(size);
    memset(memory, 0, size);
    return memory;
}
/******************************************************************************
 * DESCRIPTION: 
 *      Free malloc
 * @param[in] ptr
 *      The pointer needed to be free
 * RETURNS: void 
*******************************************************************************/
void osFree(void *ptr)
{
    if (ptr)
    {
        vPortFree(ptr);
    }
}
/******************************************************************************
 * DESCRIPTION: 
 *      send stack task data to another task
 * @param[in] src
 *      source task no.
 * @param[in] dest
 *      destination task no.
 * @param[in] pData
 *      data pointer
 * RETURNS: TRUE/FALSE 
*******************************************************************************/
bool osMessageSend(taskType_t src, taskType_t dest, void *pData)
{
    taskData_t *tBuf = (taskData_t *)osMalloc(sizeof(taskData_t));
    tBuf->src = src;
    tBuf->dest = dest;
    tBuf->pData = pData;
    return xQueueSendToBack(stackQueueHandle, tBuf, 0);
}
/*************** END OF FUNCTIONS *********************************************/
