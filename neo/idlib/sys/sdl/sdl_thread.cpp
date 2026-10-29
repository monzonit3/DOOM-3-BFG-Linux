/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "precompiled.h"

#include <SDL3/SDL.h>

/*
================================================================================================

	All threading primitives are implemented on top of the SDL3 platform layer
	instead of raw pthreads, so the engine only ever talks to SDL.

================================================================================================
*/

// SDL_CreateThread only passes a single void* to the entry point, so the real
// function pointer and its argument travel together in a small heap block.
static int RunThread( void* data )
{
	uintptr_t* parms = ( uintptr_t* )data;
	xthread_t function = ( xthread_t )parms[0];
	void* arg = ( void* )parms[1];

	int result = ( int )function( arg );

	Mem_Free( parms );
	return result;
}

/*
================
Sys_CreateThread
================
*/
uintptr_t Sys_CreateThread( xthread_t function, void* parms, xthreadPriority priority, const char* name, core_t core, int stackSize, bool suspended )
{
	// SDL_CreateThread only takes a single void* argument, so the function pointer
	// and its argument are handed over in a small heap allocated block.
	uintptr_t* args = ( uintptr_t* )Mem_Alloc( sizeof( uintptr_t ) * 2, TAG_IDLIB );
	args[0] = ( uintptr_t )function;
	args[1] = ( uintptr_t )parms;

	SDL_Thread* thread = SDL_CreateThread( RunThread, ( name && name[0] ) ? name : "Thread", args );
	if( thread == NULL )
	{
		Mem_Free( args );
		return 0;
	}

	// SDL3 only lets a thread change its own priority, so the requested priority is
	// applied from within the thread through a pending value is intentionally not done
	// here; thread names are assigned at creation time above.
	( void )priority;
	( void )core;
	( void )stackSize;
	( void )suspended;

	return ( uintptr_t )thread;
}

/*
================
Sys_GetCurrentThreadID
================
*/
uintptr_t Sys_GetCurrentThreadID()
{
	return ( uintptr_t )SDL_GetCurrentThreadID();
}

/*
================
Sys_DestroyThread
================
*/
void Sys_DestroyThread( uintptr_t threadHandle )
{
	if( threadHandle == 0 )
	{
		return;
	}

	SDL_Thread* thread = ( SDL_Thread* )threadHandle;
	SDL_DetachThread( thread );
}

/*
================
Sys_SetCurrentThreadName
================
*/
void Sys_SetCurrentThreadName( const char* name )
{
	// SDL3 assigns thread names at SDL_CreateThread time and has no setter for the
	// running thread, so there is nothing to do here.
	( void )name;
}

/*
================================================================================================

	Mutexes

================================================================================================
*/

void Sys_MutexCreate( mutexHandle_t& handle )
{
	handle = SDL_CreateMutex();
}

void Sys_MutexDestroy( mutexHandle_t& handle )
{
	if( handle )
	{
		SDL_DestroyMutex( handle );
		handle = NULL;
	}
}

bool Sys_MutexLock( mutexHandle_t& handle, bool blocking )
{
	if( handle == NULL )
	{
		return false;
	}

	if( blocking )
	{
		SDL_LockMutex( handle );
		return true;
	}

	return SDL_TryLockMutex( handle );
}

void Sys_MutexUnlock( mutexHandle_t& handle )
{
	if( handle )
	{
		SDL_UnlockMutex( handle );
	}
}

/*
================================================================================================

	Signals (SDL condition variables)

================================================================================================
*/

idSysSignal::idSysSignal( bool manualReset )
{
	mutex = SDL_CreateMutex();
	cond = SDL_CreateCondition();
	signaled = false;
	signalCounter = 0;
	waiting = false;
	this->manualReset = manualReset;
}

idSysSignal::~idSysSignal()
{
	if( cond )
	{
		SDL_DestroyCondition( cond );
	}
	if( mutex )
	{
		SDL_DestroyMutex( mutex );
	}
}

void idSysSignal::Raise()
{
	SDL_LockMutex( mutex );

	if( manualReset )
	{
		signaled = true;
	}
	else
	{
		signalCounter++;
	}

	if( waiting )
	{
		SDL_SignalCondition( cond );
	}

	SDL_UnlockMutex( mutex );
}

void idSysSignal::Clear()
{
	SDL_LockMutex( mutex );
	signaled = false;
	signalCounter = 0;
	SDL_UnlockMutex( mutex );
}

bool idSysSignal::Wait( int timeout )
{
	bool result = false;

	SDL_LockMutex( mutex );

	if( manualReset )
	{
		if( !signaled )
		{
			waiting = true;
			if( timeout == WAIT_INFINITE )
			{
				while( !signaled )
				{
					SDL_WaitCondition( cond, mutex );
				}
			}
			else
			{
				SDL_WaitConditionTimeout( cond, mutex, timeout );
			}
			waiting = false;
		}
		result = signaled;
	}
	else
	{
		if( signalCounter == 0 )
		{
			waiting = true;
			if( timeout == WAIT_INFINITE )
			{
				while( signalCounter == 0 )
				{
					SDL_WaitCondition( cond, mutex );
				}
			}
			else
			{
				SDL_WaitConditionTimeout( cond, mutex, timeout );
			}
			waiting = false;
		}

		if( signalCounter > 0 )
		{
			signalCounter--;
			result = true;
		}
	}

	SDL_UnlockMutex( mutex );

	return result;
}

/*
================================================================================================

	Atomics

================================================================================================
*/

interlockedInt_t Sys_InterlockedIncrement( interlockedInt_t& value )
{
	return SDL_AddAtomicInt( ( SDL_AtomicInt* )&value, 1 ) + 1;
}

interlockedInt_t Sys_InterlockedDecrement( interlockedInt_t& value )
{
	return SDL_AddAtomicInt( ( SDL_AtomicInt* )&value, -1 ) - 1;
}

interlockedInt_t Sys_InterlockedAdd( interlockedInt_t& value, interlockedInt_t i )
{
	return SDL_AddAtomicInt( ( SDL_AtomicInt* )&value, i ) + i;
}

interlockedInt_t Sys_InterlockedSub( interlockedInt_t& value, interlockedInt_t i )
{
	return SDL_AddAtomicInt( ( SDL_AtomicInt* )&value, -i ) - i;
}

interlockedInt_t Sys_InterlockedExchange( interlockedInt_t& value, interlockedInt_t exchange )
{
	return SDL_SetAtomicInt( ( SDL_AtomicInt* )&value, exchange );
}

interlockedInt_t Sys_InterlockedCompareExchange( interlockedInt_t& value, interlockedInt_t comparand, interlockedInt_t exchange )
{
	SDL_CompareAndSwapAtomicInt( ( SDL_AtomicInt* )&value, comparand, exchange );
	return comparand;
}

void* Sys_InterlockedExchangePointer( void*& ptr, void* exchange )
{
	return SDL_SetAtomicPointer( &ptr, exchange );
}

void* Sys_InterlockedCompareExchangePointer( void*& ptr, void* comparand, void* exchange )
{
	SDL_CompareAndSwapAtomicPointer( &ptr, comparand, exchange );
	return comparand;
}

/*
================================================================================================

	Misc

================================================================================================
*/

void Sys_Yield()
{
	SDL_Delay( 0 );
}
