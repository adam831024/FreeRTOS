/******************************************************************************
 * Copyright (C)
 *
 * NAME:
 *		main.h
 * DESCRIPTION:
 *      header file of main.c
*******************************************************************************/
#ifndef _MAIN_H_
#define _MAIN_H_

/******************************************************************************
 * Includes
 *******************************************************************************/
/*Standard include*/
#include <stdint.h>

/*Free-RTOS include*/
#include "FreeRTOS.h"
#include "queue.h"

/*Application include*/

/******************************************************************************
 * Preprocessor Constants
 *******************************************************************************/
/**
 * This constant is
 */

/******************************************************************************
 * Configuration Constants
 *******************************************************************************/

/******************************************************************************
 * Macros
 *******************************************************************************/

/******************************************************************************
 * Typedefs
 *******************************************************************************/
typedef enum
{
    MAIN_TASK = 0,
    UART_TASK = 1, 
    LCD_TASK =  2,
    RGB_TASK =  3    
}taskType_t;

typedef struct 
{
    taskType_t src;
    taskType_t dest;
    uint16_t dataLength;
    void *pData;
}taskData_t;
/******************************************************************************
 * Variables
 *******************************************************************************/
extern QueueHandle_t stackQueueHandle;
/******************************************************************************
 * Function Prototypes
 *******************************************************************************/
void temp(uint8_t tBuf);

#endif /*_MAIN_H_*/

/*************** END OF FUNCTIONS *********************************************/
