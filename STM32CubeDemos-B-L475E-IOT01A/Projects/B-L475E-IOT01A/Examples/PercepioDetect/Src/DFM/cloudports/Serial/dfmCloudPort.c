/*
 * Percepio DFM v2.1.0
 * Copyright 2023 Percepio AB
 * www.percepio.com
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * DFM serial port Cloud port
 */

#include <stddef.h>
#include <dfmCloudPort.h>
#include <dfmCloudPortConfig.h>
#include <dfm.h>
#include <string.h>
#include <stdio.h>

#if (defined(DFM_CFG_ENABLED) && ((DFM_CFG_ENABLED) >= 1))

/* Prototype for the print function */
extern void vMainUARTPrintString( char * pcString );

#define DFM_PRINT_SERIAL_DATA(msg) printf(msg)

	
static DfmCloudPortData_t *pxCloudPortData = (void*)0;

static uint32_t prvPrintDataAsHex(uint8_t* data, int size);
static DfmResult_t prvSerialPortUploadEntry(DfmEntryHandle_t xEntryHandle);

static uint32_t prvPrintDataAsHex(uint8_t* data, int size)
{
	uint32_t checksum = 0;
	int i;
	char buf[10];

    for (i = 0; i < size; i++)
    {
    	uint8_t byte = data[i];
    	checksum += byte;
        snprintf(buf, sizeof(buf), " %02X", (unsigned int)byte);

        if (i % 20 == 0)
        {
            DFM_CFG_LOCK_SERIAL();
            DFM_PRINT_ALERT_DATA(("[[ DATA:"));
        }

        DFM_PRINT_ALERT_DATA(buf);

        if ( (i+1) % 20 == 0)
        {
            DFM_PRINT_ALERT_DATA((" ]]\n"));
            DFM_CFG_UNLOCK_SERIAL();
            
            for(volatile int i=0; i<1000000; i++);
        }
    }

    if (i % 20 != 0)
    {
        DFM_PRINT_ALERT_DATA((" ]]\n"));
        DFM_CFG_UNLOCK_SERIAL();
    }

    return checksum;
}


#define ITM_PORT 2

#define itm_write_32(__data) \
{\
		while (ITM->PORT[ITM_PORT].u32 == 0) { /* Do nothing */ }	/* Block until room in ITM FIFO - This stream port is always in "blocking mode", since intended for high-speed ITM! */ \
		ITM->PORT[ITM_PORT].u32 = __data;							/* Write the data */ \
}

/* This is assumed to execute from within the recorder, with interrupts disabled */
traceResult prvTraceItmWrite(void* ptrData, uint32_t size, int32_t* ptrBytesWritten)
{
	uint32_t* ptr32 = (uint32_t*)ptrData;

	TRC_ASSERT(size % 4 == 0);
	TRC_ASSERT(ptrBytesWritten != 0);

	*ptrBytesWritten = 0;
	
	if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) &&					/* Trace enabled? */ \
		(ITM->TCR & ITM_TCR_ITMENA_Msk) &&									/* ITM enabled? */ \
		(ITM->TER & (1UL << (ITM_PORT))))				/* ITM port enabled? */
	{
		while (*ptrBytesWritten < (int32_t)size)
		{
			itm_write_32(*ptr32);
			ptr32++;
			*ptrBytesWritten += 4;
		}
	}

	return TRC_SUCCESS;
}

static DfmResult_t prvSerialPortUploadEntry(DfmEntryHandle_t xEntryHandle)
{
	uint32_t datalen;
        int32_t bytesWritten;

	if (pxCloudPortData == (void*)0)
	{
		return DFM_FAIL;
	}

	if (xEntryHandle == 0)
	{
		return DFM_FAIL;
	}

	if (xDfmEntryGetSize(xEntryHandle, &datalen) == DFM_FAIL)
	{
		return DFM_FAIL;
	}

	if (datalen > 0xFFFF)
	{
		return DFM_FAIL;
	}

        prvTraceItmWrite((uint8_t*)xEntryHandle, datalen, &bytesWritten);
        
        printf("DFM wrote %d bytes\n\r", bytesWritten);
        fflush(stdout);
        
        if (bytesWritten != datalen)
        {
          return DFM_FAIL;
        }
        
	// (void) prvPrintDataAsHex((uint8_t*)xEntryHandle, datalen);

	return DFM_SUCCESS;
}


static DfmResult_t __prvSerialPortUploadEntry(DfmEntryHandle_t xEntryHandle)
{
	uint32_t datalen;

	if (pxCloudPortData == (void*)0)
	{
		return DFM_FAIL;
	}

	if (xEntryHandle == 0)
	{
		return DFM_FAIL;
	}

	if (xDfmEntryGetSize(xEntryHandle, &datalen) == DFM_FAIL)
	{
		return DFM_FAIL;
	}

	if (datalen > 0xFFFF)
	{
		return DFM_FAIL;
	}

	DFM_CFG_LOCK_SERIAL();
	DFM_PRINT_SERIAL_DATA("\n[[ DevAlert Data Begins ]]\n");
	DFM_CFG_UNLOCK_SERIAL();

	(void) prvPrintDataAsHex((uint8_t*)xEntryHandle, datalen);

    // Checksum not provided (0) since not updated for the new Receiver script (uses a different checksum algorithm). If 0, checksum is ignore.
	snprintf(pxCloudPortData->buf, sizeof(pxCloudPortData->buf), "[[ DevAlert Data Ended. Checksum: %d ]]\n\r", (unsigned int)0);

	DFM_CFG_LOCK_SERIAL();
	DFM_PRINT_SERIAL_DATA(pxCloudPortData->buf);
	DFM_CFG_UNLOCK_SERIAL();

        //fflush(stdout);          
                
	return DFM_SUCCESS;
}

DfmResult_t xDfmCloudPortInitialize(DfmCloudPortData_t* pxBuffer)
{
	pxCloudPortData = pxBuffer;

        xTraceHardwarePortInitCortexM();
        
	return DFM_SUCCESS;
}

DfmResult_t xDfmCloudPortSendAlert(DfmEntryHandle_t xEntryHandle)
{
	return prvSerialPortUploadEntry(xEntryHandle);
}

DfmResult_t xDfmCloudPortSendPayloadChunk(DfmEntryHandle_t xEntryHandle)
{
	return prvSerialPortUploadEntry(xEntryHandle);
}

#endif
