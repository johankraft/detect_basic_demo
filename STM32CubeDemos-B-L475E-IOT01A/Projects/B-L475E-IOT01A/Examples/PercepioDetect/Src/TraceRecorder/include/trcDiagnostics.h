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
 * @brief Public trace diagnostic APIs.
 */

#ifndef TRC_DIAGNOSTICS_H
#define TRC_DIAGNOSTICS_H

#if (TRC_USE_TRACEALYZER_RECORDER == 1)

#include <trcTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRC_DIAGNOSTICS_COUNT 5UL

typedef enum TraceDiagnosticsType
{
	TRC_DIAGNOSTICS_ENTRY_SYMBOL_LONGEST_LENGTH = 0x00UL,
	TRC_DIAGNOSTICS_ENTRY_SLOTS_NO_ROOM = 0x01UL,
	TRC_DIAGNOSTICS_BLOB_MAX_BYTES_TRUNCATED = 0x02UL,
	TRC_DIAGNOSTICS_STACK_MONITOR_NO_SLOTS = 0x03UL,
	TRC_DIAGNOSTICS_ASSERTS_TRIGGERED = 0x04UL,
} TraceDiagnosticsType_t;

typedef struct TraceDiagnostics /* Aligned */
{
	TraceBaseType_t metrics[TRC_DIAGNOSTICS_COUNT];
} TraceDiagnosticsData_t;

/**
 * @internal Initialize diagnostics
 *
 * @param[in] pxBuffer Diagnostics buffer
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsInitialize(TraceDiagnosticsData_t* pxBuffer);

/**
 * @brief Retrieve diagnostics value
 *
 * @param[in] xType Diagnostics type
 * @param[out] pxValue Pointer to value
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsGet(TraceDiagnosticsType_t xType, TraceBaseType_t* pxValue);

/**
 * @brief Set diagnostics value
 *
 * @param[in] xType Diagnostics type
 * @param[in] xValue Value
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsSet(TraceDiagnosticsType_t xType, TraceBaseType_t xValue);

/**
 * @brief Add to diagnostics value
 *
 * @param[in] xType Diagnostics type
 * @param[in] xValue Value
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsAdd(TraceDiagnosticsType_t xType, TraceBaseType_t xValue);

/**
 * @brief Increase diagnostics value
 *
 * @param[in] xType Diagnostics type
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsIncrease(TraceDiagnosticsType_t xType);

/**
 * @brief Decrease diagnostics value
 *
 * @param[in] xType Diagnostics type
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsDecrease(TraceDiagnosticsType_t xType);

/**
 * @brief Set a new diagnostics value if higher than previous value
 *
 * @param[in] xType Dagnostics type
 * @param[in] xValue Value
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsSetIfHigher(TraceDiagnosticsType_t xType, TraceBaseType_t xValue);

/**
 * @brief Set a new diagnostics value if lower than previous value
 *
 * @param[in] xType Dagnostics type
 * @param[in] xValue Value
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsSetIfLower(TraceDiagnosticsType_t xType, TraceBaseType_t xValue);

/**
 * @brief Check the diagnostics status
 *
 * @retval TRC_FAIL Failure
 * @retval TRC_SUCCESS Success
 */
traceResult xTraceDiagnosticsCheckStatus(void);

#ifdef __cplusplus
}
#endif

#endif

#endif
