/******************************************************************************
Copyright (C) nFore Technology Co.,Ltd. 2005-2021

FILE NAME
		main.c

DESCRIPTION
		module responsible for tts selection

*******************************************************************************/
/******************************************************************************
 * Includes
 *******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include "Nano100Series.h"              // Device header
#include "utility.h"
#include "fifo.h"

/*Free-RTOS*/
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "semphr.h"
#include "task.h"
#include "timers.h"
/******************************************************************************
 * Module Preprocessor Constants
 *******************************************************************************/
#define CMD_HEAD_LEN 5
/*MAIN TASK EVENTS */
#define SBP_STATE_CHANGE_EVT 0x0001
#define SBP_CHAR_CHANGE_EVT 0x0002
#define SBP_GUA_PAIRING_STATE_EVT 0x0004
#define SBP_CONN_EVT_END_EVT 0x0008
#define SBP_GUA_PASSCODE_EVT 0x0010
#define SBP_DISCONNECT_EVT 0x0020
#define SBP_BONDING_CONNECTION_EVT 0x0040
#define PAIRSTAT_EVT 0x0080
#define STAFF_EVT 0x0400
#define MAX_AMZ_CMD_SIZE 800
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
	PARSER_WAITING,			 /*still have some data in the buffer */
	PARSER_EMPTY,				 /*there is no data in buffer*/
	PARSER_FULL,				 /*Buffer full, lost some incoming data*/
	PARSER_CRASH,				 /*important error*/
	PARSER_TIMEOUT_FLUSH /*5s timeout flush the waiting data in buffer*/
} nfParser_t;
/******************************************************************************
 * Module Variable Definitions
 *******************************************************************************/
/*events flag for internal application events. */
static uint16_t events;
SemaphoreHandle_t eventsHandle = NULL;
/*fifo*/
Fifo_t nfFifoBuffer;
uint8_t nfUartFifoBuffer[MAX_AMZ_CMD_SIZE] = {0};
uint8_t nfUartData[MAX_AMZ_CMD_SIZE] = {0};

TimerHandle_t nfStaffTimerHandle = NULL;
TimerHandle_t nfParserTimerHandle = NULL;

/******************************************************************************
 * Function Prototypes
 *******************************************************************************/
static void vTaskEvent(void *pvParameters);
static void vTaskUartParser(void *pvParameters);

/******************************************************************************
 * Function Definitions
 *******************************************************************************/
/**
 * @brief     create task
 */
void nfTaskCreate(void)
{
	xTaskCreate(vTaskUartParser, "vTaskUartParser", 2048 /*usStackDepth = 1024*16bits*/, NULL /*parameter*/, 5 /*Priority*/, NULL /*CreatedTaskHandle*/);
	xTaskCreate(vTaskEvent, "vTaskEvent", 1024, NULL /*pvParameters parameter for C function*/, 5, NULL /*pxCreatedTask*/);
}
/******************************************************************************
 * @brief     UART data parser
 * @param[in] data                		uart data
 * @param[in] dataLength                uart data length
 * @return                              nfParser_t
 *******************************************************************************/
static nfParser_t nfUartParser(uint8_t *data, uint16_t dataLength)
{
	uint16_t fifoCnt = 0;
	uint16_t i = 0;
	if (IsFifoEmpty(&nfFifoBuffer))
	{
		FifoFlush(&nfFifoBuffer);
	}
	
	for ( i = 0; i < dataLength; i++)
	{
		if (IsFifoFull(&nfFifoBuffer))
		{
			/*buffer full, Incoming Data lost!!*/
			FifoFlush(&nfFifoBuffer);
			return PARSER_FULL;
		}
		else
		{
			/*push data into fifo*/
			FifoPush(&nfFifoBuffer, data[i]);
		}
	}

RELOOP:
	fifoCnt = FifoCount(&nfFifoBuffer);
	if (fifoCnt < CMD_HEAD_LEN)
	{
		return PARSER_WAITING;
	}
	if ((0xAA != FifoPrePop(&nfFifoBuffer, 0)) || (0xBB != FifoPrePop(&nfFifoBuffer, 1))) /*filter*/
	{
		/*0x12 0xAA 0xBB 0x15 0x00 0x01 0x01 0xAA*/
		/*0xBB 0x12 0x00 0x01 0x01*/
		/*0x12 0x34 0xAA*/
		fifoCnt = FifoCount(&nfFifoBuffer);
		if (fifoCnt)
		{
			uint16_t i = 0;
			for (i = 0; i < fifoCnt; i++)
			{
				if (0xAA == FifoPrePop(&nfFifoBuffer, 0))
				{
					if (0xBB == FifoPrePop(&nfFifoBuffer, 1))
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
						uint8_t die = FifoPop(&nfFifoBuffer);
						// nfUartDebugSend(&die, 1);
					}
				}
				else
				{
					/*0x12 0x34 0x56 ...*/
					uint8_t die = FifoPop(&nfFifoBuffer);
					// nfUartDebugSend(&die, 1);
				}
			}
		}
		else
		{
			FifoFlush(&nfFifoBuffer);
			return PARSER_EMPTY;
		}
	}

	fifoCnt = FifoCount(&nfFifoBuffer);
	// nfUartDebugSend(&fifoCnt, 1);
	if (fifoCnt >= CMD_HEAD_LEN) /*enough data*/
	{
		/*0xAA 0xBB 0x12 0x34 0x56 ...*/

		uint16_t payLoadLen = ((FifoPrePop(&nfFifoBuffer, 3) << 8) | FifoPrePop(&nfFifoBuffer, 4));

		if (fifoCnt >= (payLoadLen + CMD_HEAD_LEN))
		{
			/*0xAA 0xBB 0x12 0x00 0x03 0x11 0x22 0x33 0x44 0x55*/
			for (i = 0; i < (payLoadLen + CMD_HEAD_LEN); i++)
			{
				if (!IsFifoEmpty(&nfFifoBuffer))
				{
					nfUartData[i] = FifoPop(&nfFifoBuffer);
				}
				else
				{
					/*buffer empty*/
					FifoFlush(&nfFifoBuffer);
					return PARSER_CRASH; /*important error*/
				}
			}
			// TODO:	nfAmzCmdHandler(&nfUartData[2], payLoadLen + 3); /*0x12 0x00 0x03 0x11 0x22 0x33*/
			fifoCnt = FifoCount(&nfFifoBuffer);
			if (fifoCnt)
			{
				/*0x44 0x55*/
				goto RELOOP;
			}
			else
			{
				FifoFlush(&nfFifoBuffer);
				return PARSER_SUCCESS;
			}
		}
		else
		{
			/*0xAA 0xBB 0x17 0xAA 0xBB 0x15 0x01 0x01*/
			/*0xaa 0xbb 0xaa 0x12 0x01 0x01*/
			for(i=0; i<(fifoCnt-3); i++) /*(fifoCnt-3):  message already started at 0xAA 0xBB, do not need to filter*/
			{
				if ((0xAA == FifoPrePop(&nfFifoBuffer, i + 2)) && (0xBB == FifoPrePop(&nfFifoBuffer, i + 3)))
				{
					FifoPop(&nfFifoBuffer);
					FifoPop(&nfFifoBuffer);
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
		FifoFlush(&nfFifoBuffer);
		return PARSER_EMPTY;
	}
}
/**
 * @brief     parser task
 * @param[in] pvParameters				event arg
 */
static void vTaskUartParser(void *pvParameters)
{
	while (1)
	{
		nfParser_t status = PARSER_ERROR;
		if (xTimerIsTimerActive(nfParserTimerHandle) == pdFALSE)
		{
			/* Timer is not active. */
			xTimerStart(nfParserTimerHandle, 0);
		}
		/*parser*/
		// TODO: status = nfUartParser(p_data, (uint16_t)length);
		if (PARSER_SUCCESS == status || PARSER_EMPTY == status)
		{
			/*stop timer*/
			if (xTimerIsTimerActive(nfParserTimerHandle) == pdTRUE)
			{
				/* Timer is active. */
				xTimerStop(nfParserTimerHandle, 0);
			}
		}
		else if (status == PARSER_ERROR || status == PARSER_CRASH || PARSER_FULL == status)
		{
			/*send log message*/
		}
	}
}

/**
 * @brief     event task
 * @param[in] pvParameters				event arg
 */
static void vTaskEvent(void *pvParameters)
{
	while (1)
	{
		/*semaphore check*/
		if (uxSemaphoreGetCount(eventsHandle))
		{
			if (events & SBP_DISCONNECT_EVT)
			{
				events &= ~SBP_DISCONNECT_EVT;
				/*do something */
			}
			else if (events & STAFF_EVT)
			{
				events &= ~STAFF_EVT;
				/*do something */
			}
			/*release semaphore*/
			xSemaphoreGive(eventsHandle);
		}
	}
}
/**
 * @brief     staff timer count every second
 * @param[in] xTimer				the pointer to the TimerHandle_t.
 */
static void nfStaffTimerCb(TimerHandle_t xTimer)
{
	/*run every second*/
	events |= STAFF_EVT; // arg or to events
	xSemaphoreTake(eventsHandle, 0);
}
/**
 * @brief     Parser timeout callback, flush fifo buffer
 * @param[in] xTimer				the pointer to the TimerHandle_t.
 */
static void nfParserTimerCb(TimerHandle_t xTimer)
{
	if (!IsFifoEmpty(&nfFifoBuffer))
	{
		FifoFlush(&nfFifoBuffer);
	}
	/*flush, send log message*/
}
/**
 * @brief   init timer
 */
static void nfTimerInit(void)
{
	nfStaffTimerHandle = xTimerCreate("nfStaffTimer" /* The timer name. */,
																		1000 / portTICK_PERIOD_MS /*const TickType_t xTimerPeriodInTicks*/,
																		pdTRUE /*const UBaseType_t uxAutoReload, pdFALSE for on shot, pdTRUE for period*/,
																		NULL /*void * const pvTimerID*/,
																		nfStaffTimerCb /*TimerCallbackFunction_t pxCallbackFunction*/);
	nfParserTimerHandle = xTimerCreate("nfParserTimer" /* The timer name. */,
																		 5000 / portTICK_PERIOD_MS /*const TickType_t xTimerPeriodInTicks*/,
																		 pdFALSE /*const UBaseType_t uxAutoReload, pdFALSE for on shot, pdTRUE for period*/,
																		 NULL /*void * const pvTimerID*/,
																		 nfParserTimerCb /*TimerCallbackFunction_t pxCallbackFunction*/);
	// xTimerStart(nfStaffTimerHandle, 0);
}

/**
 * @brief     main
 * @param[in] argc                     is the link handle.
 * @param[in] argv                     the role type of the local device.
 * @return                             #BT_STATUS_SUCCESS, the operation completed successfully.
 *                                     #BT_STATUS_CONNECTION_NOT_FOUND, the operation has failed, the link does not exist.
 */
int main(int argc, char const *argv[])
{
	//TODO: enter critical section
	/*systick時鐘為HCLK，中斷時間間隔1ms*/
	SysTick_Config(SystemCoreClock / 1000);
	
	eventsHandle = xSemaphoreCreateCounting(10, 0);
	if( !eventsHandle ) 
	{ 
		/*semaphore fail create*/ 
	} 
	nfTaskCreate();
	nfTimerInit();
	FifoInit(&nfFifoBuffer, nfUartFifoBuffer, MAX_AMZ_CMD_SIZE);	/*fifo init*/
	//TODO: exit critical section
	/* Start the scheduler. */
    vTaskStartScheduler();
	return 0;
}
