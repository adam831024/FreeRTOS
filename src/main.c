/******************************************************************************
 * Copyright (C)
 *
 * NAME:
 *		main.c
 * DESCRIPTION:
 *
*******************************************************************************/
/******************************************************************************
 * Includes
 *******************************************************************************/
/*Standard include*/
#include <stdio.h>
#include <stdint.h>
#include "Nano100Series.h"              // Device header
#include "uart.h"
/*Free-RTOS include*/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"

/*Application include*/
#include "utility.h"
#include "fifo.h"
#include "main.h"

/******************************************************************************
 * Module Preprocessor Constants
 *******************************************************************************/
/*CMD*/
#define CMD_MAX_SIZE 800
#define CMD_HEAD_LEN 5

/*MAIN TASK EVENTS */
#define OS_EVT_1 0x0001
#define OS_EVT_2 0x0002
#define OS_EVT_3 0x0004
#define OS_EVT_4 0x0008
#define OS_EVT_5 0x0010
#define OS_EVT_6 0x0020
#define OS_EVT_7 0x0040
#define OS_EVT_8 0x0080
#define STATE_CHECK_EVT 0x0400

/******************************************************************************
 * Module Preprocessor Macros
 *******************************************************************************/

/******************************************************************************
 * Module Typedefs
 *******************************************************************************/
typedef enum
{
	PARSER_SUCCESS = 0x00,
	PARSER_ERROR,
	PARSER_WAITING,				/*still have some data in the buffer */
	PARSER_EMPTY,				/*there is no data in buffer*/
	PARSER_FULL,				/*Buffer full, lost some incoming data*/
	PARSER_CRASH,				/*important error*/
	PARSER_TIMEOUT_FLUSH 		/*5s timeout flush the waiting data in buffer*/
} parserState_t;

/******************************************************************************
 * Module Variable Definitions
 *******************************************************************************/
/*OS event */
static uint16_t events;
SemaphoreHandle_t eventsHandle = NULL;

/*fifo*/
Fifo_t fifoBuffer;
uint8_t uartFifoBuffer[CMD_MAX_SIZE] = {0};
uint8_t uartData[CMD_MAX_SIZE] = {0};

/*timer*/
TimerHandle_t stateCheckTimerHandle = NULL;
TimerHandle_t parserTimerHandle = NULL;

/*stack*/
taskType_t taskType;
QueueHandle_t stackQueueHandle = NULL;

/******************************************************************************
 * Function Prototypes
 *******************************************************************************/
static void mainTask(void *pvParameters);
static void uartTask(void *pvParameters);
void rgbTask(void *pvParameters); //TODO:need to move
/******************************************************************************
 * Function Definitions
 *******************************************************************************/
/**
 * @brief     create task
 */
void taskCreate(void)
{
	xTaskCreate(uartTask, "uartTask", 2048 /*usStackDepth = 1024*16bits*/, (void *)UART_TASK, 5 /*Priority*/, NULL /*CreatedTaskHandle*/);
	xTaskCreate(mainTask, "mainTask", 1024, (void *)MAIN_TASK, 4, NULL /*pxCreatedTask*/);
	xTaskCreate(rgbTask, "rgbTask", 512, (void *)RGB_TASK, 3, NULL /*pxCreatedTask*/);
}
/******************************************************************************
 * @brief     UART data parser
 * @param[in] data                		uart data
 * @param[in] dataLength                uart data length
 * @return                              parserState_t
 *******************************************************************************/
static parserState_t uartParser(uint8_t *data, uint16_t dataLength)
{
	uint16_t fifoCnt = 0;
	uint16_t i = 0;
	if (IsFifoEmpty(&fifoBuffer))
	{
		FifoFlush(&fifoBuffer);
	}
	
	for ( i = 0; i < dataLength; i++)
	{
		if (IsFifoFull(&fifoBuffer))
		{
			/*buffer full, Incoming Data lost!!*/
			FifoFlush(&fifoBuffer);
			return PARSER_FULL;
		}
		else
		{
			/*push data into fifo*/
			FifoPush(&fifoBuffer, data[i]);
		}
	}

RELOOP:
	fifoCnt = FifoCount(&fifoBuffer);
	if (fifoCnt < CMD_HEAD_LEN)
	{
		return PARSER_WAITING;
	}
	if ((0xAA != FifoPrePop(&fifoBuffer, 0)) || (0xBB != FifoPrePop(&fifoBuffer, 1))) /*filter*/
	{
		/*0x12 0xAA 0xBB 0x15 0x00 0x01 0x01 0xAA*/
		/*0xBB 0x12 0x00 0x01 0x01*/
		/*0x12 0x34 0xAA*/
		fifoCnt = FifoCount(&fifoBuffer);
		if (fifoCnt)
		{
			uint16_t i = 0;
			for (i = 0; i < fifoCnt; i++)
			{
				if (0xAA == FifoPrePop(&fifoBuffer, 0))
				{
					if (0xBB == FifoPrePop(&fifoBuffer, 1))
					{
						/*0xAA 0xBB ...*/
						break;
					}
					else if (i == (fifoCnt - 1))
					{
						/*last is 0xAA, keep*/
						break;
					}
					else
					{
						/*0xAA 0x12 ...*/
						uint8_t die = FifoPop(&fifoBuffer);
					}
				}
				else
				{
					/*0x12 0x34 0x56 ...*/
					uint8_t die = FifoPop(&fifoBuffer);
				}
			}
		}
		else
		{
			FifoFlush(&fifoBuffer);
			return PARSER_EMPTY;
		}
	}

	fifoCnt = FifoCount(&fifoBuffer);
	if (fifoCnt >= CMD_HEAD_LEN) /*enough data*/
	{
		/*0xAA 0xBB 0x12 0x34 0x56 ...*/

		uint16_t payLoadLen = ((FifoPrePop(&fifoBuffer, 3) << 8) | FifoPrePop(&fifoBuffer, 4));

		if (fifoCnt >= (payLoadLen + CMD_HEAD_LEN))
		{
			/*0xAA 0xBB 0x12 0x00 0x03 0x11 0x22 0x33 0x44 0x55*/
			for (i = 0; i < (payLoadLen + CMD_HEAD_LEN); i++)
			{
				if (!IsFifoEmpty(&fifoBuffer))
				{
					uartData[i] = FifoPop(&fifoBuffer);
				}
				else
				{
					/*buffer empty*/
					FifoFlush(&fifoBuffer);
					return PARSER_CRASH; /*important error*/
				}
			}
			// TODO:	nfAmzCmdHandler(&uartData[2], payLoadLen + 3); /*0x12 0x00 0x03 0x11 0x22 0x33*/
			fifoCnt = FifoCount(&fifoBuffer);
			if (fifoCnt)
			{
				/*0x44 0x55*/
				goto RELOOP;
			}
			else
			{
				FifoFlush(&fifoBuffer);
				return PARSER_SUCCESS;
			}
		}
		else
		{
			/*0xAA 0xBB 0x17 0xAA 0xBB 0x15 0x01 0x01*/
			/*0xaa 0xbb 0xaa 0x12 0x01 0x01*/
			for(i=0; i<(fifoCnt-3); i++) /*(fifoCnt-3):  message already started at 0xAA 0xBB, do not need to filter*/
			{
				if ((0xAA == FifoPrePop(&fifoBuffer, i + 2)) && (0xBB == FifoPrePop(&fifoBuffer, i + 3)))
				{
					FifoPop(&fifoBuffer);
					FifoPop(&fifoBuffer);
					goto RELOOP;
				}
			}
			return PARSER_WAITING;
		}
	}
	else if (fifoCnt)
	{
		/*0xAA or 0xAA 0xBB or 0xAA 0xBB 0x12 or 0xAA 0xBB 0x12 0x00*/
		return PARSER_WAITING;
	}
	else
	{
		/*no data in buffer*/
		FifoFlush(&fifoBuffer);
		return PARSER_EMPTY;
	}
}

/******************************************************************************
 * @brief     UART parser task
 * @param[out] pvParameters             event arg
 * @return                              void
 *******************************************************************************/
static void uartTask(void *pvParameters)
{
	while (1)
	{
		parserState_t status = PARSER_ERROR;
		if (xTimerIsTimerActive(parserTimerHandle) == pdFALSE)
		{
			/* Timer is not active. */
			xTimerStart(parserTimerHandle, 0);
		}
		/*parser*/
		// TODO: status = uartParser(p_data, (uint16_t)length);
		if (PARSER_SUCCESS == status || PARSER_EMPTY == status)
		{
			/*stop timer*/
			if (xTimerIsTimerActive(parserTimerHandle) == pdTRUE)
			{
				/* Timer is active. */
				xTimerStop(parserTimerHandle, 0);
			}
		}
		else if (status == PARSER_ERROR || status == PARSER_CRASH || PARSER_FULL == status)
		{
			/*send log message*/
		}
	}
}

/******************************************************************************
 * @brief     RGB event task
 * @param[out] pvParameters             event arg
 * @return                              void
 *******************************************************************************/
void rgbTask(void *pvParameters) //TODO: have to move to rgb.c
{
	taskData_t *recData = NULL;
	uint8_t srcTask = (uint8_t)pvParameters;
	while (1)
	{
		if(pdTRUE==xQueueReceive(stackQueueHandle, recData, 0))
		{
			switch (recData->dest)
			{
				case MAIN_TASK:
				{
					uint8_t *tBuf = (uint8_t *)osMalloc((recData->dataLength)*sizeof(uint8_t));
					if(!tBuf)
					{
						return;
					}
					memcpy(tBuf, recData->pData, recData->dataLength);
					// uart0Send(tBuf, recData->dataLength);
					osFree(tBuf);
				}
				break;
				default:
					break;
			}
			if(recData)
			{
				osFree(recData);
			}
		}
	}
	
}
/******************************************************************************
 * @brief     OS event task
 * @param[out] pvParameters             event arg
 * @return                              void
 *******************************************************************************/
static void mainTask(void *pvParameters)
{
	uint8_t srcTask = (uint8_t)pvParameters;
	while (1)
	{
		/*semaphore check*/
		if (uxSemaphoreGetCount(eventsHandle))
		{
			if (events & OS_EVT_6)
			{
				uint8_t *tBuf = (uint8_t *)osMalloc(4);
				events &= ~OS_EVT_6;
				tBuf[0] = 0xBB;
				tBuf[1] = 0x00;	/*size*/
				tBuf[2] = 0x01;	/*size*/
				tBuf[3] = 0xFF;

				osMessageSend(srcTask, RGB_TASK, tBuf, 4);
				osFree(tBuf);
			}
			else if (events & STATE_CHECK_EVT)
			{
				uint8_t data[4];
				static count = 0;
				events &= ~STATE_CHECK_EVT;
				data[0] = 0xAA;
				data[1] = 0x00;
				data[2] = 0x01;
				data[3] = count;
				uart0Send(data, 4);
				count++;
				xTimerStart(stateCheckTimerHandle, 0);
			}
			/*release semaphore*/
			xSemaphoreGive(eventsHandle);
		}
	}
}

/******************************************************************************
 * @brief     state check timer callack every second
 * @param[out] xTimer             		the pointer to the TimerHandle_t.
 * @return                              void
 *******************************************************************************/
static void stateCheckTimerCb(TimerHandle_t xTimer)
{
	/*run every second*/
	events |= STATE_CHECK_EVT; // arg or to events
	xSemaphoreTake(eventsHandle, 0);
}

/******************************************************************************
 * @brief     Parser timeout callback, flush fifo buffer
 * @param[out] xTimer             		the pointer to the TimerHandle_t.
 * @return                              void
 *******************************************************************************/
static void parserTimerCb(TimerHandle_t xTimer)
{
	if (!IsFifoEmpty(&fifoBuffer))
	{
		FifoFlush(&fifoBuffer);
	}
	/*flush, send log message*/
}

/******************************************************************************
 * @brief     init timer
 * @return                              void
 *******************************************************************************/
static void timerInit(void)
{
	stateCheckTimerHandle = xTimerCreate("stateCheck" /* The timer name. */,
										1000 / portTICK_PERIOD_MS /*const TickType_t xTimerPeriodInTicks*/,
										pdFALSE /*const UBaseType_t uxAutoReload, pdFALSE for on shot, pdTRUE for period*/,
										NULL /*void * const pvTimerID*/,
										stateCheckTimerCb /*TimerCallbackFunction_t pxCallbackFunction*/);
	parserTimerHandle = xTimerCreate("nfParserTimer" /* The timer name. */,
										5000 / portTICK_PERIOD_MS /*const TickType_t xTimerPeriodInTicks*/,
										pdFALSE /*const UBaseType_t uxAutoReload, pdFALSE for on shot, pdTRUE for period*/,
										NULL /*void * const pvTimerID*/,
										parserTimerCb /*TimerCallbackFunction_t pxCallbackFunction*/);
	xTimerStart(stateCheckTimerHandle, 0);
}
/******************************************************************************
 * @brief     init HCLK
 * @return                              void
 *******************************************************************************/
void init_HCLK(void)
{
	SYS_UnlockReg();
	CLK_EnableXtalRC(CLK_PWRCTL_HXT_EN_Msk); /*¶}±ÒCPUªºclock*/
	CLK_WaitClockReady(CLK_CLKSTATUS_HXT_STB_Msk);
	CLK_SetHCLK(CLK_CLKSEL0_HCLK_S_HXT, CLK_HCLK_CLK_DIVIDER(1));
	SYS_LockReg();
}

/******************************************************************************
 * @brief     main
 * @param[out] argc                     is the link handle.
 * @param[out] argv                     the role type of the local device.
 * @return                              void
 *******************************************************************************/
int main(int argc, char const *argv[])
{
	taskENTER_CRITICAL();

	eventsHandle = xSemaphoreCreateCounting(10, 0);
	if( !eventsHandle ) 
	{ 
		/*semaphore fail create*/ 
	} 
	stackQueueHandle = xQueueCreate(10, sizeof(uint8_t *));
	init_HCLK();
	delay_init();
	taskCreate();
	timerInit();
	init_UART0(115200);
	/*fifo init*/
	FifoInit(&fifoBuffer, uartFifoBuffer, CMD_MAX_SIZE);
	printf("init finish");

	taskEXIT_CRITICAL();
	/* Start the scheduler. */
  vTaskStartScheduler();
	return 0;
}
/*************** END OF FUNCTIONS *********************************************/
/**
 * further step
 * 1. UART parser, handler
 * 2. data pass through task
 * 3. LCD
 * 4. RGB
*/
