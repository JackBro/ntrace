#pragma once

/*----------------------------------------------------------------------
 * Purpose:
 *		JP Function Boundary Tracing Library.
 *
 * Copyright:
 *		Johannes Passing (johannes.passing@googlemail.com)
 */

#ifdef _NT_TARGET_VERSION
#include <wdm.h>
#define JPFBT_KERNELMODE
#elif _WIN32
#include <windows.h>
#include <winnt.h>
#include <ntsecapi.h>
#define JPFBT_USERMODE
#else
#error Unknown mode - neither _NT_TARGET_VERSION nor _WIN32 defined
#endif

#ifdef _WIN64
#define JPFBTCALLTYPE
#else
#define JPFBTCALLTYPE __stdcall
#endif

#ifndef _MAKE_NTSTATUS
#define _MAKE_NTSTATUS( Sev, Cust, Fac, Code ) \
    ( ( NTSTATUS ) (	\
		( ( unsigned long ) ( Sev  & 0x03 ) << 30 ) | \
		( ( unsigned long ) ( Cust & 0x01 ) << 29 ) | \
		( ( unsigned long ) ( Fac  & 0x3F ) << 16 ) | \
		( ( unsigned long ) ( Code ) ) ) )
#endif

//
// Errors
//
#define STATUS_FBT_NO_THUNKSTACK		 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x1 )
#define STATUS_FBT_PROC_NOT_PATCHABLE	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x2 )
#define STATUS_FBT_PROC_ALREADY_PATCHED	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x3 )
#define STATUS_FBT_PROC_TOO_FAR			 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x4 )
#define STATUS_FBT_INIT_FAILURE			 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x5 )
#define STATUS_FBT_THR_SUSPEND_FAILURE	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x6 )
#define STATUS_FBT_THR_CTXUPD_FAILURE	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x7 )
#define STATUS_FBT_STILL_PATCHED		 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x8 )
#define STATUS_FBT_NOT_INITIALIZED		 _MAKE_NTSTATUS( 3, 1, 0xFB, 0x9 )
#define STATUS_FBT_ALREADY_INITIALIZED	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0xA )
#define STATUS_FBT_UNUSABLE_TLS_SLOT	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0xB )
#define STATUS_FBT_AUPTR_IN_USE		 	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0xC )
#define STATUS_FBT_NOT_PATCHED		 	 _MAKE_NTSTATUS( 3, 1, 0xFB, 0xD )


#define EXCEPTION_FBT_NO_THUNKSTACK		 STATUS_FBT_NO_THUNKSTACK
#define EXCEPTION_FBT_AUPTR_IN_USE		 STATUS_FBT_AUPTR_IN_USE


//
// If defined, debug code is inserted into the trampoline.
//
// Note: The debug code scraps eax!
//
#define TRAMPOLINE_TESTMODE 1

//
// Stripped-down context as used by jpfbt.
//
#ifdef _M_IX86 
typedef struct _JPFBT_CONTEXT {
    DWORD   Edi;				// 0x0
    DWORD   Esi;				// 0x4
    DWORD   Ebx;				// 0x8
    DWORD   Edx;				// 0xC
    DWORD   Ecx;				// 0x10
    DWORD   Eax;				// 0x14

    DWORD   Ebp;				// 0x18
    DWORD   Eip;				// 0x1C
    DWORD   EFlags;				// 0x20
    DWORD   Esp;				// 0x24
} JPFBT_CONTEXT, *PJPFBT_CONTEXT;
#else
#error Unsupported target architecture
#endif

/*++
	Routine Description:
		Routine called at procedure event/exit.

		To obtain a buffer, call JpfbtGetBuffer();

	Parameters:
		Context	-   Context on call entry. Note that edx and eax
				    (both volatile) are 0 and do not reflect the
				    real values.
		Procedure - Procedure entered.
--*/
typedef VOID ( JPFBTCALLTYPE * JPFBT_EVENT_ROUTINE ) (
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
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
	__in DWORD ProcessId,
	__in DWORD ThreadId,
	__in_opt PVOID UserPointer
	);

#define JPFBT_FLAG_AUTOCOLLECT	1

/*++
	Routine Description:
		Initialize library. Initialization and Unininitialization
		are not threadsafe.

	Parameters
		BufferCount - total number of buffers. Should be at least
					  2 times the total number of threads.
		BufferSize  - size of each buffer. Must be a multiple of 
					  MEMORY_ALLOCATION_ALIGNMENT.
		Flags		- JPFBT_FLAG_AUTOCOLLECT:
						call ProcessBufferRoutine whenever buffers
						are to be collected. JpfbtProcessBuffer
						must not be called.
					  0:
						no autocollection. ProcessBufferRoutine is 
						only called during shutdown to flush buffers.
						JpfbtProcessBuffer must be called 
						repeatedly.
		EntryEvRt.  - Routine called on entry of hooked function.
		ExitEvRt.   - Routine called on exit of hooked function.
		ProcessBufR.- Routine called for dirty buffer collection.
		UserPointer - Arbitrary user pointer passed to 
					  ProcessBufferRoutine.

	Return Value:
		STATUS_SUCCESS on success.
		(any NTSTATUS) on failure.
--*/
NTSTATUS JPFBTCALLTYPE JpfbtInitialize(
	__in UINT BufferCount,
	__in UINT BufferSize,
	__in DWORD Flags,
	__in JPFBT_EVENT_ROUTINE  EntryEventRoutine,
	__in JPFBT_EVENT_ROUTINE  ExitEventRoutine,
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in_opt PVOID UserPointer
	);

/*++
	Routine Description:
		Unininitialize library. Initialization and Unininitialization
		are not threadsafe.

		This routine may not be called from within one of the callbacks.

	Parameters
		None

	Return Value:
		STATUS_SUCCESS on success.
		(any NTSTATUS) oon failure.
--*/
NTSTATUS JPFBTCALLTYPE JpfbtUninitialize();

/*++
	Routine Description:
		Process the next buffer by calling the specified callback
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
NTSTATUS JpfbtProcessBuffer(
	__in JPFBT_PROCESS_BUFFER_ROUTINE ProcessBufferRoutine,
	__in DWORD Timeout,
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
		DWORD_PTR ProcedureVa;
	} u;
} JPFBT_PROCEDURE, *PJPFBT_PROCEDURE;

/*++
	Routine Description:
		Instrument one or more procedures. Instrumentation requires 
		either 5 byte (/functionpadmin:5) or 10 byte (/functionpadmin:10) 
		padding and a hotpatchable prolog.

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
			fulfil criteria. FailedProcedure is set.
		STATUS_FBT_PROC_ALREADY_PATCHED if procedure has already been
			patched. FailedProcedure is set.
--*/
NTSTATUS JPFBTCALLTYPE JpfbtInstrumentProcedure(
	__in JPFBT_INSTRUMENTATION_ACTION Action,
	__in UINT ProcedureCount,
	__in_ecount(InstrCount) CONST PJPFBT_PROCEDURE Procedures,
	__out_opt PJPFBT_PROCEDURE FailedProcedure
	);

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
	__in UINT RequiredSize 
	);