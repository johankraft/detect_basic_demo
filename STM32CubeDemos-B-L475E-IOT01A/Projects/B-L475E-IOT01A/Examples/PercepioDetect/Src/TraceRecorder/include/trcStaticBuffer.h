/*
* Percepio Trace Recorder for Tracealyzer v989.878.767
* Copyright 2025 Percepio AB
* www.percepio.com
*
* SPDX-License-Identifier: Apache-2.0
*/

/**
 * @file 
 * 
 * @brief Public trace static buffer APIs.
 */

#ifndef TRC_STATIC_BUFFER_H
#define TRC_STATIC_BUFFER_H

#if (TRC_USE_TRACEALYZER_RECORDER == 1)

#include <trcTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup trace_static_buffer_apis Trace Static Buffer APIs
 * @ingroup trace_recorder_apis
 * @{
 */

/* A buffer type that is maximum size */
typedef uint8_t TraceStaticBuffer_t[TRC_MAX_BLOB_SIZE];

/**
 * @internal Trace Core Static Buffer Core Structure
 */
typedef struct TraceCoreStaticBufferCore	/* Aligned */
{
	TraceStaticBuffer_t dummyEvents[(TRC_CFG_MAX_ISR_NESTING) + 1]; /**< */
} TraceCoreStaticBuffer_t;

/**
 * @internal Trace Static Buffer Table Structure
 */
typedef struct TraceStaticBufferTable	/* Aligned */
{
	TraceCoreStaticBuffer_t coreDummyEvents[TRC_CFG_CORE_COUNT]; /**< Temporary buffers used for event or blob creation. */
} TraceStaticBufferTable_t;

extern TraceStaticBufferTable_t* pxTraceStaticBufferTable;

/**
 * @internal Initialize trace static buffer. 
 * 
 * @param[in] pxBuffer Pointer to memory that will be used by the
 * trace static buffer.
 * 
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceStaticBufferInitialize(TraceStaticBufferTable_t* pxBuffer);

#if ((TRC_CFG_USE_TRACE_ASSERT) == 1)

/**
 * @brief Gets trace static buffer.
 * 
 * @param[out] ppvBuffer Buffer.
 * 
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceStaticBufferGet(void **ppvBuffer);

#else /* ((TRC_CFG_USE_TRACE_ASSERT) == 1) */

/**
 * @brief Gets trace static buffer.
 * 
 * @param[out] ppvBuffer Buffer.
 * 
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
#define xTraceStaticBufferGet(ppvBuffer) (*(ppvBuffer) = (void*)&pxTraceStaticBufferTable->coreDummyEvents[TRC_CFG_GET_CURRENT_CORE()].dummyEvents[xTraceISRGetCurrentNestingReturned() + 1], TRC_SUCCESS)

#endif /* ((TRC_CFG_USE_TRACE_ASSERT) == 1) */

/** @} */

#ifdef __cplusplus
}
#endif

#endif

#endif
