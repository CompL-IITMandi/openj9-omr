/*******************************************************************************
 * Copyright IBM Corp. and others 2014
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/

#ifndef omrprofiler_h
#define omrprofiler_h

#include "omr.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* This file defines the interface used by omrglue/ code to access OMR profiling-related functions */

#if defined(_MSC_VER)
#pragma warning(disable : 4200)
#endif /* defined(_MSC_VER) */
typedef struct OMR_MethodDictionaryEntry {
	const void *key;
	const char *propertyValues[];
} OMR_MethodDictionaryEntry;

/* ---------------- OMR_MethodDictionary.cpp ---------------- */

/**
 * @brief Allocate and initialize the VM's method dictionary.
 *
 * Does nothing if the VM's method dictionary is already allocated.
 *
 * @param[in] vm The OMR VM.
 * @param[in] numProperties Number of properties per method.
 * @param[in] propertyNames Names of method properties.
 * @return An OMR error code.
 * @retval OMR_ERROR_NONE Success.
 * @retval OMR_ERROR_OUT_OF_NATIVE_MEMORY Unable to allocate native memory for the method dictionary.
 * @retval OMR_ERROR_FAILED_TO_ALLOCATE_MONITOR Unable to allocate the method dictionary's lock.
 * @retval OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD Unable to attach to the omrthread library.
 */
omr_error_t omr_ras_initMethodDictionary(OMR_VM *vm, size_t numProperties, const char * const *propertyNames);

/**
 * @brief Deallocate the VM's method dictionary.
 *
 * @param[in] vm The OMR VM.
 */
void omr_ras_cleanupMethodDictionary(OMR_VM *vm);

/**
 * @brief Insert a method into the method dictionary.
 *
 * If the method's key is already in the dictionary, then the old entry is overwritten by the new entry.
 *
 * There is no error if the method dictionary is not enabled.
 *
 * @param[in] vm The OMR VM.
 * @param[in] entry The method entry to insert.
 * @return An OMR error code.
 * @retval OMR_ERROR NONE Success.
 * @retval OMR_ERROR_OUT_OF_NATIVE_MEMORY Unable to allocate native memory for the new entry.
 * @retval OMR_ERROR_FAILED_TO_ATTACH_NATIVE_THREAD Unable to attach to the omrthread library,
 * which is needed to lock the method dictionary.
 * @retval OMR_ERROR_INTERNAL An unexpected internal error occurred.
 */
omr_error_t omr_ras_insertMethodDictionary(OMR_VM *vm, OMR_MethodDictionaryEntry *entry);

/* ---------------- OMR_Profiler.cpp ---------------- */
/**
 * @brief Trace the current thread's top-most stack frame.
 *
 * Wrapper for tracepoint macro.
 * This allows omrglue code to be compiled without access to tracegen-generated
 * header files.
 *
 * @pre The current thread must be attached to the OMR VM.
 *
 * @param[in] omrVMThread The current OMR VM thread. Must not be NULL.
 * @param[in] methodKey The stack frame's method's key. It corresponds to the key of
 *                      the method's entry in the method dictionary.
 */
void omr_ras_sampleStackTraceStart(OMR_VMThread *omrVMThread, const void *methodKey);

/**
 * @brief Trace a frame of the current thread's stack, other than the top-most frame.
 *
 * Wrapper for tracepoint macro.
 * This allows omrglue code to be compiled without access to tracegen-generated
 * header files.
 *
 * @pre The current thread must be attached to the OMR VM.
 *
 * @param[in] omrVMThread The current OMR VM thread. Must not be NULL.
 * @param[in] methodKey The stack frame's method's key. It corresponds to the key of
 *                      the method's entry in the method dictionary.
 */
void omr_ras_sampleStackTraceContinue(OMR_VMThread *omrVMThread, const void *methodKey);

/**
 * @brief Test whether stack sampling is enabled.
 *
 * Wrapper for tracepoint macro.
 * This allows omrglue code to be compiled without access to tracegen-generated
 * header files.
 *
 * Stack stampling is enabled if either of the sampling tracepoints is enabled,
 * regardless of whether the method dictionary is enabled.
 *
 * @return TRUE if stack sampling is enabled. FALSE if stack sampling is disabled.
 */
BOOLEAN omr_ras_sampleStackEnabled(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* omrprofiler_h */
