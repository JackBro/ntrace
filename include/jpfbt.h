#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		JP Function Boundary Tracing Library.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#if defined(JPFBT_TARGET_KERNELMODE)
	#include <ntddk.h>
#elif defined(JPFBT_TARGET_USERMODE)
	#include <windows.h>
	#include <winnt.h>
	#include <ntsecapi.h>
#else
	#error Unknown mode (User/Kernel)
#endif

#ifdef _WIN64
	#define JPFBTCALLTYPE
#else
	#define JPFBTCALLTYPE __stdcall
#endif

#define EXCEPTION_FBT_NO_THUNKSTACK		 STATUS_FBT_NO_THUNKSTACK
#define EXCEPTION_FBT_AUPTR_IN_USE		 STATUS_FBT_AUPTR_IN_USE

#define JPFBT_MAX_BUFFER_SIZE	( 16 * 1024 * 1024 )

#define JPFBT_MIN_PROCEDURE_PADDING_REQUIRED	5

/*++
	Structure Description:
		Stripped-down context as used by jpfbt.
--*/
#ifdef _M_IX86 
typedef struct _JPFBT_CONTEXT {
    ULONG   Edi;				// 0x0
    ULONG   Esi;				// 0x4
    ULONG   Ebx;				// 0x8
    ULONG   Edx;				// 0xC
    ULONG   Ecx;				// 0x10
    ULONG   Eax;				// 0x14

    ULONG   Ebp;				// 0x18
    ULONG   Eip;				// 0x1C
    ULONG   EFlags;				// 0x20
    ULONG   Esp;				// 0x24
} JPFBT_CONTEXT, *PJPFBT_CONTEXT;
#else
#error Unsupported target architecture
#endif

/*++
	Structure Description:
		Execution statictics.
--*/
typedef struct _JPFBT_STATISTICS
{
	ULONG PatchCount;

	struct
	{
		ULONG Free;
		ULONG Dirty;
		ULONG Collected;
	} Buffers;

	struct
	{
		ULONG FreePreallocationPoolSize;
		ULONG FailedPreallocationPoolAllocations;
	} ThreadData;

	ULONG ReentrantThunkExecutionsDetected;
	ULONG EventsCaptured;
	ULONG ExceptionsUnwindings;
	ULONG ThreadTeardowns;
} JPFBT_STATISTICS, *PJPFBT_STATISTICS;

/*++
	Routine Description:
		Routine called at procedure event/exit.

		To obtain a buffer, call JpfbtGetBuffer();

	Parameters:
		Context	-   Context on call entry. Note that edx and eax
				    (both volatile) are 0 and do not reflect the
				    real values.
		Procedure - Procedure entered.
		UserPointer - Arbitrary user pointer passed to 
					  JpfbtInitialize.
--*/
typedef VOID ( JPFBTCALLTYPE * JPFBT_EVENT_ROUTINE ) (
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	);

/*++
	Routine Description:
		Routine called if exception unwinding occurs.

		To obtain a buffer, call JpfbtGetBuffer();

	Parameters:
		ExceptionCode - Code
		Procedure     - Procedure entered.
		UserPointer   - Arbitrary user pointer passed to 
					   JpfbtInitialize.
--*/
typedef VOID ( JPFBTCALLTYPE * JPFBT_EXCP_UNWIND_EVENT_ROUTINE ) (
	__in ULONG ExceptionCode,
	__in PVOID Function,
	__in_opt PVOID UserPointer
	);

/*++
	Routine Description:
		Process buffer containing information generated by 
		event routines.

	Parameters:
		BufferSize  - Size of buffer in bytes.
		Buffer	   
		ProcessId   - Id of process in which the data was generated
		ThreadId    - Id of thread in which the data was generated
		UserPointer - Arbitrary user pointer passed to 
					  JpfbtInitialize.
--*/
typedef VOID ( JPFBTCALLTYPE * JPFBT_PROCESS_BUFFER_ROUTINE ) (
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in ULONG ProcessId,
	__in ULONG ThreadId,
	__in_opt PVOID UserPointer
	);

//
// Auto-colect buffers.
//
#define JPFBT_FLAG_AUTOCOLLECT				1

//
// Disable lazy allocation of thread data structures (KM only).
//
#define JPFBT_FLAG_DISABLE_LAZY_ALLOCATION	2

//
// Disable eager notification of buffer collector (KM only).
//
#define JPFBT_FLAG_DISABLE_EAGER_BUFFER_COLLECTION	4

/*++
	Routine Description:
		Initialize library. Initialization and Unininitialization
		are not threadsafe.

		Callable at PASSIVE_LEVEL.

	Parameters
		BufferCount - total number of buffers. Should be at least
					  2 times the total number of threads.
		BufferSize  - size of each buffer. Must be a multiple of 
					  MEMORY_ALLOCATION_ALIGNMENT.
		ThreadDataPreallocations - Number of ThreadData structures to
					  be preallocated tp facilitate high-IRQL
					  allocations. (Kernel mode only).
		Flags		- JPFBT_FLAG_AUTOCOLLECT:
						call ProcessBufferRoutine whenever buffers
						are to be collected. JpfbtProcessBuffer
						must not be called.
					  0:
						no autocollection. ProcessBufferRoutine is 
						only called during shutdown to flush buffers.
						JpfbtProcessBuffer must be called 
						repeatedly.
					  JPFBT_FLAG_INTERCEPT_EXCEPTIONS
					  JPFBT_FLAG_DISABLE_LAZY_ALLOCATION
					  JPFBT_FLAG_DISABLE_EAGER_BUFFER_COLLECTION
		EntryEvRt.  - Routine called on entry of hooked function.
		ExitEvRt.   - Routine called on exit of hooked function.
		ExcepEvRt.  - Routine called when a traced routine is unwound after
					  an exception has been thrown. Must be NULL iff
					  JPFBT_FLAG_INTERCEPT_EXCEPTIONS not set.
		ProcessBufR.- Routine called for dirty buffer collection.
		UserPointer - Arbitrary user pointer passed to 
					  ProcessBufferRoutine.

	Return Value:
		STATUS_SUCCESS on success.
		(any NTSTATUS) on failure.
--*/

#if defined(JPFBT_TARGET_USERMODE)
NTSTATUS JPFBTCALLTYPE JpfbtInitialize(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG Flags,
	__in JPFBT_EVENT_ROUTINE  EntryEventRoutine,
	__in JPFBT_EVENT_ROUTINE  ExitEventRoutine,
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in_opt PVOID UserPointer
	);
#endif

NTSTATUS JpfbtInitializeEx(
	__in ULONG BufferCount,
	__in ULONG BufferSize,
	__in ULONG ThreadDataPreallocations,
	__in ULONG Flags,
	__in JPFBT_EVENT_ROUTINE EntryEventRoutine,
	__in JPFBT_EVENT_ROUTINE ExitEventRoutine,
	__in_opt JPFBT_EXCP_UNWIND_EVENT_ROUTINE ExceptionEventRoutine,
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in_opt PVOID UserPointer
	);

/*++
	Routine Description:
		Unininitialize library. Initialization and Unininitialization
		are not threadsafe.

		This routine may not be called from within one of the callbacks.

		Callable at PASSIVE_LEVEL.

	Parameters
		None

	Return Value:
		STATUS_SUCCESS on success.
		(any NTSTATUS) oon failure.
--*/
NTSTATUS JPFBTCALLTYPE JpfbtUninitialize();

/*++
	Routine Description:
		Process the next buffers by calling the specified callback
		routine. After return from the callback routine, the 
		buffer memory is considered free and must not be touched 
		again.
		If no buffer is currently ready for being processed, the 
		routine blocks until a buffer becomes available.

		This routine must only be called if JPFBT_FLAG_AUTOCOLLECT
		has *not* been specified on the call to JpfbtInitialize.

		Routine is threadsafe.

	Parameters:
		ProcessBufferRoutine - callback. Called synchronously.
		Timeout				 - timeout to block or 0.
		UserPointer - Arbitrary user pointer passed to 
					  ProcessBufferRoutine.

	Return Value:
		STATUS_SUCCESS on success.
		STATUS_TIMEOUT if timeout elapsed and no buffer was processed.
		(any NTSTATUS) oon failure.
--*/
NTSTATUS JpfbtProcessBuffers(
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in ULONG Timeout,
	__in_opt PVOID UserPointer
	);

typedef enum _JPFBT_INSTRUMENTATION_ACTION
{
	JpfbtAddInstrumentation		= 0,
	JpfbtRemoveInstrumentation	= 1
} JPFBT_INSTRUMENTATION_ACTION;

typedef struct _JPFBT_PROCEDURE
{
	union
	{
		PVOID Procedure;
		ULONG_PTR ProcedureVa;
	} u;
} JPFBT_PROCEDURE, *PJPFBT_PROCEDURE;

/*++
	Routine Description:
		Instrument one or more procedures. Instrumentation requires 
		a 5 byte (/functionpadmin:5) padding and a hotpatchable prolog.

		Routine is threadsafe.

	Parameters:
		ProcedureCount  - # of procedures to instrument.
		Procedures	    - Procedures to instrument.
						  The caller has to ensure that the array
						  is free of duplicates. Duplicate entries
						  lead to undefined behaviour.
		FailedProcedure - Procedure that made the instrumentation fail.

	Return Value:
		STATUS_SUCCESS on success. FailedProcedure is set to NULL.
		STATUS_FBT_PROC_NOT_PATCHABLE if at least one procedure does not 
			fulfill criteria. FailedProcedure is set.
		STATUS_FBT_PROC_ALREADY_PATCHED if procedure has already been
			patched. FailedProcedure is set.
--*/
NTSTATUS JPFBTCALLTYPE JpfbtInstrumentProcedure(
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in ULONG ProcedureCount,
	__in_ecount(ProcedureCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	);

/*++
	Routine Description:
		Remove instrumentation from all routines that have been
		instrumented. Should be called before uninitializing.
--*/
NTSTATUS JpfbtRemoveInstrumentationAllProcedures();

/*++
	Routine Description:
		Get buffer. For use from within event routines.

		Threadsafe.
		
	Parameters:
		Size in bytes the buffer must provide.

	Return Value:
		Buffer or NULL if no left space available.
--*/
PUCHAR JPFBTCALLTYPE JpfbtGetBuffer(
	__in ULONG RequiredSize 
	);

/*++
	Routine Description:
		Should be called by an application whenever a thread that
		may have been affected by instrumentated routines is about 
		to exit. It is safe to call this routine from DllMain
		or a PS Thread Notify Routine.

		Callable at IRQL <= APC_LEVEL.

	Parameters:
		Thread		- Kernel Mode: Pointer to the affected ETHREAD.
					  User Mode: NULL. The calling thread must be the
					  one that is about to be terminated.
--*/
VOID JPFBTCALLTYPE JpfbtCleanupThread(
	__in_opt PVOID Thread
	);

/*++
	Routine Description:
		Check if procedure is suitable for being instrumented.

	Parameters:
		Procedure			Procedure ro check.
		Instrumentable		Result of check.
--*/
NTSTATUS JpfbtCheckProcedureInstrumentability(
	__in JPFBT_PROCEDURE Procedure,
	__out PBOOLEAN Instrumentable
	);

/*++
	Routine Description:
		Query statistics.
--*/
NTSTATUS JpfbtQueryStatistics(
	__out PJPFBT_STATISTICS Statistics
	);
