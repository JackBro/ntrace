#include "test.h"

static BOOL ExpectBufferDepletion = FALSE;

/*----------------------------------------------------------------------
 *
 * Callbacks
 *
 */

static VOID __stdcall ProcedureEntry( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	UINT Index;
	PUCHAR Buffer;

	UNREFERENCED_PARAMETER( Context );

	TEST( JpfbtGetBuffer( 1024*1024 ) == NULL );

	Buffer = JpfbtGetBuffer( sizeof( PVOID ) );
	if ( ! ExpectBufferDepletion )
	{
		TEST( Buffer );
	}
	if ( Buffer )
	{
		memcpy( Buffer, Function, sizeof( PVOID ) );
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Proc == Function )
		{
			InterlockedIncrement( 
				&ProcSet->SampleProcs[ Index ].EntryThunkCallCount );
			break;
		}
	}

	OutputDebugString( L"--> ProcedureEntry\n" );

	// Garble volatiles
	__asm
	{
		mov eax, 0xDEAD001;
		mov ecx, 0xDEAD002;
		mov edx, 0xDEAD003;
	}
}

static VOID __stdcall ProcedureExit( 
	__in CONST PJPFBT_CONTEXT Context,
	__in PVOID Function
	)
{
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	UINT Index;

	PUCHAR Buffer;

	UNREFERENCED_PARAMETER( Context );

	TEST( JpfbtGetBuffer( 1024*1024 ) == NULL );

	Buffer = JpfbtGetBuffer( sizeof( PVOID ) );
	if ( ! ExpectBufferDepletion )
	{
		TEST( Buffer );
	}
	if ( Buffer )
	{
		memcpy( Buffer, Function, sizeof( PVOID ) );
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Proc == Function )
		{
			InterlockedIncrement( 
				&ProcSet->SampleProcs[ Index ].ExitThunkCallCount );
			break;
		}
	}

	OutputDebugString( L"<-- ProcedureExit\n" );

	// Garble volatiles
	__asm
	{
		mov eax, 0xDEAD004;
		mov ecx, 0xDEAD005;
		mov edx, 0xDEAD006;
	}
}

static VOID ProcessBuffer(
	__in SIZE_T BufferSize,
	__in_bcount(BufferSize) PUCHAR Buffer,
	__in DWORD ProcessId,
	__in DWORD ThreadId,
	__in_opt PVOID UserPointer
	)
{
	UNREFERENCED_PARAMETER( BufferSize );
	UNREFERENCED_PARAMETER( Buffer );
	UNREFERENCED_PARAMETER( ProcessId );
	UNREFERENCED_PARAMETER( ThreadId );
	UNREFERENCED_PARAMETER( UserPointer );
}


/*----------------------------------------------------------------------
 *
 * Patch/Unpatch helper procedures.
 *
 */

static BOOL PatchAll()
{
	JPFBT_PROCEDURE Temp = { NULL };
	
	UINT NoPatchProcCount = 0;
	PJPFBT_PROCEDURE NoPatchProcs;
	
	UINT PatchProcCount = 0;
	PJPFBT_PROCEDURE PatchProcs;

	PSAMPLE_PROC_SET ProcSet;

	UINT Index;
	NTSTATUS Status;

	//
	// Get sample procs.
	//
	ProcSet = GetSampleProcs();

	//
	// Prep patching
	//
	NoPatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );
	PatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );

	TEST( NoPatchProcs );
	TEST( PatchProcs );

	if ( ! NoPatchProcs || ! PatchProcs )
	{
		return 0xC0000001L;
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Patchable )
		{
			PatchProcs[ PatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
		else
		{
			NoPatchProcs[ NoPatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
	}

	TEST( NoPatchProcCount > 0 );
	TEST( PatchProcCount > 0 );

	//
	// Test nonpatchable.
	//
	TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		NoPatchProcCount, 
		NoPatchProcs, 
		&Temp ) );
	TEST( Temp.u.Procedure == NoPatchProcs[ 0 ].u.Procedure );

	for ( Index = 0; Index < NoPatchProcCount; Index++ )
	{
		TEST( STATUS_FBT_PROC_NOT_PATCHABLE == JpfbtInstrumentProcedure( 
			JpfbtAddInstrumentation, 
			1, 
			&NoPatchProcs[ Index ], 
			&Temp ) );
		TEST( Temp.u.Procedure == NoPatchProcs[ Index ].u.Procedure );
	}

	//
	// Patch.
	//
	Status = JpfbtInstrumentProcedure( 
		JpfbtAddInstrumentation, 
		PatchProcCount, 
		PatchProcs, 
		&Temp );
	TEST( ( 0 == Status && Temp.u.Procedure == NULL ) || 
		  STATUS_FBT_PROC_ALREADY_PATCHED == Status && Temp.u.Procedure != NULL );

	free( NoPatchProcs );
	free( PatchProcs );

	return Status == 0;
}

static VOID UnpatchAll()
{
	UINT PatchProcCount = 0;
	PJPFBT_PROCEDURE PatchProcs;

	PSAMPLE_PROC_SET ProcSet;
	UINT Index;

	//
	// Get sample procs.
	//
	ProcSet = GetSampleProcs();

	PatchProcs = malloc( sizeof( JPFBT_PROCEDURE ) * ProcSet->SampleProcCount );

	TEST( PatchProcs );
	if ( ! PatchProcs )
	{
		return;
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Patchable )
		{
			PatchProcs[ PatchProcCount++ ].u.Procedure = ProcSet->SampleProcs[ Index ].Proc;
		}
		else
		{
			JPFBT_PROCEDURE Temp = { NULL };
			JPFBT_PROCEDURE InvalidUnpatchProc;
			InvalidUnpatchProc.u.Procedure = ProcSet->SampleProcs[ Index ].Proc;

			TEST( 0xC000000DL == JpfbtInstrumentProcedure( 
				JpfbtRemoveInstrumentation, 
				1, 
				&InvalidUnpatchProc, 
				&Temp ) );
			TEST( Temp.u.Procedure == ProcSet->SampleProcs[ Index ].Proc );
		}
	}

	TEST( PatchProcCount > 0 );

	TEST_SUCCESS( JpfbtInstrumentProcedure( 
		JpfbtRemoveInstrumentation, 
		PatchProcCount, 
		PatchProcs, 
		NULL ) );

	free( PatchProcs );
}

/*----------------------------------------------------------------------
 *
 * Test case.
 *
 */

VOID PatchAndTestAllProcsSinglethreaded()
{
	UINT Index;
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();

	ExpectBufferDepletion = TRUE;

	TEST_SUCCESS( JpfbtInitialize( 
		2,								// deliberately too small
		8,
		JPFBT_FLAG_AUTOCOLLECT,
		ProcedureEntry, 
		ProcedureExit,
		ProcessBuffer,
		NULL) );
	
	TEST( PatchAll() );

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].DriverProcedure();
	}

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		LONG ExpecedCount = 
			( ProcSet->SampleProcs[ Index ].Patchable ? 1 : 0 ) *
				ProcSet->SampleProcs[ Index ].CallMultiplier;

		TEST( *ProcSet->SampleProcs[ Index ].CallCount == 1 * 
			ProcSet->SampleProcs[ Index ].CallMultiplier );

		TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount == ExpecedCount );
		TEST( ProcSet->SampleProcs[ Index ].ExitThunkCallCount == ExpecedCount );
	}

	UnpatchAll();

	TEST_SUCCESS( JpfbtUninitialize() );
}

/*----------------------------------------------------------------------
 *
 * Test case.
 *
 */
static volatile LONG DriversCalled = 0;
static volatile LONG CallProcsThreadsActive = 0;

static DWORD CALLBACK CallProcsThreadProc( __in PVOID PvIterations )
{
	PUINT TotalIterations = ( PUINT ) PvIterations;
	UINT Iteration = 0;
	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	UINT Index;

	InterlockedIncrement( &CallProcsThreadsActive );

	while ( Iteration++ < *TotalIterations * 100 )
	{
		//
		// Call all procs.
		//
		for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
		{
			ProcSet->SampleProcs[ Index ].DriverProcedure();
			Sleep( 0 );
			InterlockedIncrement( &DriversCalled );
		}
	}

	InterlockedDecrement( &CallProcsThreadsActive );

	return 0;
}

static DWORD CALLBACK PatchUnpatchThreadProc( __in PVOID PvIterations )
{
	PUINT TotalIterations = ( PUINT ) PvIterations;
	UINT Iteration = 0;

	while ( Iteration++ < *TotalIterations )
	{
		BOOL Patched = PatchAll();
		
		//
		// Give other threads some time.
		//
		LONG DriversCalledBefore = DriversCalled;
		Sleep( 1000 );

		if ( Patched )
		{
			TEST( ( CallProcsThreadsActive > 0 ) == ( DriversCalledBefore < DriversCalled ) );
	
			OutputDebugString( L"Patching succeeded - now unpatching\n" );
			UnpatchAll();
		}
		else
		{
			OutputDebugString( L"Patching failed\n" );
		}
	}

	return 0;
}


VOID PatchAndTestAllProcsMultithreaded()
{
	#define CALLER_THREAD_COUNT 10
	#define PATCHUNPATCH_THREAD_COUNT 10

	UINT Iterations = 20;

	PSAMPLE_PROC_SET ProcSet = GetSampleProcs();
	UINT Index;

	HANDLE Threads[ CALLER_THREAD_COUNT + PATCHUNPATCH_THREAD_COUNT ];
	UINT NextThreadIndex = 0;

	TEST_SUCCESS( JpfbtInitialize( 
		15*5, 
		16,
		JPFBT_FLAG_AUTOCOLLECT,
		ProcedureEntry, 
		ProcedureExit,
		ProcessBuffer,
		NULL ) );

	//
	// Clear counters.
	//
	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		ProcSet->SampleProcs[ Index ].EntryThunkCallCount = 0;
		ProcSet->SampleProcs[ Index ].ExitThunkCallCount = 0;
		*ProcSet->SampleProcs[ Index ].CallCount = 0;
	}

	for ( Index = 0; Index < CALLER_THREAD_COUNT; Index++ )
	{
		HANDLE Thread = CreateThread(
			NULL,
			0,
			CallProcsThreadProc,
			&Iterations,
			0,
			NULL );
		TEST( Thread );
		Threads[ NextThreadIndex++ ] = Thread;
	}

	for ( Index = 0; Index < PATCHUNPATCH_THREAD_COUNT; Index++ )
	{
		HANDLE Thread = CreateThread(
			NULL,
			0,
			PatchUnpatchThreadProc,
			&Iterations,
			0,
			NULL );
		TEST( Thread );
		Threads[ NextThreadIndex++ ] = Thread;
	}

	WaitForMultipleObjects( _countof( Threads ), Threads, TRUE, INFINITE );

	for ( Index = 0; Index < ProcSet->SampleProcCount; Index++ )
	{
		if ( ProcSet->SampleProcs[ Index ].Patchable )
		{
			TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount > 0 );
			TEST( ProcSet->SampleProcs[ Index ].ExitThunkCallCount > 0 );
		
			TEST( ProcSet->SampleProcs[ Index ].EntryThunkCallCount ==
				  ProcSet->SampleProcs[ Index ].ExitThunkCallCount );
		}

		TEST( *ProcSet->SampleProcs[ Index ].CallCount > 0 );
	}

	TEST_SUCCESS( JpfbtUninitialize() );
}