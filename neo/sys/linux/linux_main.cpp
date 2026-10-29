/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.
Copyright (C) 2012 Robert Beckebans

This file is part of the Doom 3 GPL Source Code (?Doom 3 Source Code?).

Doom 3 Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/
#include "../../idlib/precompiled.h"
#include "../posix/posix_public.h"
#include "../sys_local.h"
//#include "local.h"

#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// DG: needed for Sys_ReLaunch()
#include <dirent.h>

// needed for the callstack helpers
#include <execinfo.h>

static const char** cmdargv = NULL;
static int cmdargc = 0;
// DG end

#ifdef ID_MCHECK
#include <mcheck.h>
#endif

static idStr	basepath;
static idStr	savepath;


/*
 ==============
 Sys_DefaultSavePath
 ==============
 */
const char* Sys_DefaultSavePath()
{
	// SDL_GetPrefPath returns a platform appropriate writable directory that is
	// guaranteed to end with a path separator
	char* prefPath = SDL_GetPrefPath( "id Software", "DOOM3 BFG" );
	if( prefPath != NULL )
	{
		savepath = prefPath;
		SDL_free( prefPath );
		// strip the trailing separator, the engine appends its own
		savepath.StripTrailing( '/' );
		savepath.StripTrailing( '\\' );
	}
	else
	{
		// ~ is only expanded by a shell, so build the fallback from SDL's
		// real home directory instead of handing a literal ~ to the kernel.
		const char* home = SDL_GetUserFolder( SDL_FOLDER_HOME );
		if( home != NULL )
		{
			savepath = home;
			savepath.StripTrailing( '/' );
			savepath.StripTrailing( '\\' );
			savepath += "/.doom3bfg";
		}
		else
		{
			savepath = Posix_Cwd();
			savepath += "/.doom3bfg";
		}
	}
	
	return savepath.c_str();
}
/*
==============
Sys_EXEPath
==============
*/
const char* Sys_EXEPath()
{
	static idStr exePath;

	// /proc/self/exe gives the full path of the running executable, which is
	// what callers expect (SDL_GetBasePath() only returns its directory).
	char path[ 4096 ];
	ssize_t len = readlink( "/proc/self/exe", path, sizeof( path ) - 1 );
	if( len > 0 )
	{
		path[ len ] = '\0';
		exePath = path;
		return exePath.c_str();
	}

	// fallback: use the directory returned by SDL (cannot include the name)
	char* basePath = ( char* )SDL_GetBasePath();
	if( basePath != NULL )
	{
		exePath = basePath;
		SDL_free( basePath );
		if( exePath.Length() )
		{
			exePath.StripTrailing( '/' );
		}
	}

	return exePath.c_str();
}

/*
================
Sys_DefaultBasePath

Get the default base path
- binary image path
- current directory
- hardcoded
Try to be intelligent: if there is no BASE_GAMEDIR, try the next path
================
*/
const char* Sys_DefaultBasePath()
{
	idStr testbase;
	basepath = Sys_EXEPath();
	if( basepath.Length() )
	{
		basepath.StripFilename();
		testbase = basepath;
		testbase += "/";
		testbase += BASE_GAMEDIR;
		if( Sys_IsFolder( testbase.c_str() ) == FOLDER_YES )
		{
			return basepath.c_str();
		}
		else
		{
			common->Printf( "no '%s' directory in exe path %s, skipping\n", BASE_GAMEDIR, basepath.c_str() );
		}
	}
	if( basepath != Posix_Cwd() )
	{
		basepath = Posix_Cwd();
		testbase = basepath;
		testbase += "/";
		testbase += BASE_GAMEDIR;
		if( Sys_IsFolder( testbase.c_str() ) == FOLDER_YES )
		{
			return basepath.c_str();
		}
		else
		{
			common->Printf( "no '%s' directory in cwd path %s, skipping\n", BASE_GAMEDIR, basepath.c_str() );
		}
	}

	// last resort: the directory a system wide install puts the game data in
	static const char* default_base_path = "/usr/local/share/doom3bfg";
	common->Printf( "WARNING: using hardcoded default base path '%s'\n", default_base_path );
	return default_base_path;
}



/*
===============
Sys_Shutdown
===============
*/
void Sys_Shutdown()
{
	basepath.Clear();
	savepath.Clear();
	Posix_Shutdown();
}

/*
===============
Sys_GetProcessorId
===============
*/
cpuid_t Sys_GetProcessorId()
{
	return CPUID_GENERIC;
}

/*
===============
Sys_GetProcessorString
===============
*/
const char* Sys_GetProcessorString()
{
	return "generic";
}

/*
===============
Sys_FPU_EnableExceptions
===============
*/
//void Sys_FPU_EnableExceptions( int exceptions )
//{
//}

/*
===============
Sys_FPE_handler
===============
*/
void Sys_FPE_handler( int signum, siginfo_t* info, void* context )
{
	assert( signum == SIGFPE );
	Sys_Printf( "FPE\n" );
}

/*
===============
Sys_GetClockticks
===============
*/
double Sys_GetClockTicks()
{
	return ( double )SDL_GetTicksNS();
}

/*
===============
Sys_ClockTicksPerSecond
===============
*/
double Sys_ClockTicksPerSecond()
{
	// Sys_GetClockTicks returns SDL's nanosecond timer on every platform.
	return 1000000000.0;
}

/*
========================
Sys_CPUCount

numLogicalCPUCores	- the number of logical CPU per core
numPhysicalCPUCores	- the total number of cores per package
numCPUPackages		- the total number of packages (physical processors)
========================
*/
void Sys_CPUCount( int& numLogicalCPUCores, int& numPhysicalCPUCores, int& numCPUPackages )
{
	numLogicalCPUCores = SDL_GetNumLogicalCPUCores();
	if( numLogicalCPUCores < 1 )
	{
		numLogicalCPUCores = 1;
	}

	// SDL does not distinguish physical cores from SMT threads.
	numPhysicalCPUCores = numLogicalCPUCores;
	numCPUPackages = 1;
}

/*
================
Sys_GetSystemRam
returns in megabytes
================
*/
int Sys_GetSystemRam()
{
	int mb = SDL_GetSystemRAM();
	if( mb <= 0 )
	{
		common->Printf( "GetSystemRam: SDL_GetSystemRAM failed\n" );
		return 512;
	}
	// round to the nearest 16Mb
	mb = ( mb + 8 ) & ~15;
	return mb;
}



/*
==================
Sys_DoStartProcess
if we don't fork, this function never returns
the no-fork lets you keep the terminal when you're about to spawn an installer

if the command contains spaces, system() is used. Otherwise the more straightforward execl ( system() blows though )
==================
*/
void Sys_DoStartProcess( const char* exeName, bool dofork )
{
	bool use_system = false;
	if( strchr( exeName, ' ' ) )
	{
		use_system = true;
	}
	else
	{
		// set exec rights when it's about a single file to execute
		struct stat buf;
		if( stat( exeName, &buf ) == -1 )
		{
			printf( "stat %s failed: %s\n", exeName, strerror( errno ) );
		}
		else
		{
			if( chmod( exeName, buf.st_mode | S_IXUSR ) == -1 )
			{
				printf( "cmod +x %s failed: %s\n", exeName, strerror( errno ) );
			}
		}
	}
	if( dofork )
	{
		switch( fork() )
		{
			case -1:
				// main thread
				break;
			case 0:
				if( use_system )
				{
					printf( "system %s\n", exeName );
					system( exeName );
					_exit( 0 );
				}
				else
				{
					printf( "execl %s\n", exeName );
					execl( exeName, exeName, NULL );
					printf( "execl failed: %s\n", strerror( errno ) );
					_exit( -1 );
				}
				break;
		}
	}
	else
	{
		if( use_system )
		{
			printf( "system %s\n", exeName );
			system( exeName );
			sleep( 1 );	// on some systems I've seen that starting the new process and exiting this one should not be too close
		}
		else
		{
			printf( "execl %s\n", exeName );
			execl( exeName, exeName, NULL );
			printf( "execl failed: %s\n", strerror( errno ) );
		}
		// terminate
		_exit( 0 );
	}
}

/*
=================
Sys_OpenURL
=================
*/
void idSysLocal::OpenURL( const char* url, bool quit )
{
	static bool	quit_spamguard = false;
	
	if( quit_spamguard )
	{
		common->DPrintf( "Sys_OpenURL: already in a doexit sequence, ignoring %s\n", url );
		return;
	}
	
	common->Printf( "Open URL: %s\n", url );
	
	// let SDL figure out how to hand this to the desktop (xdg-open on Wayland/X11 etc.)
	if( !SDL_OpenURL( url ) )
	{
		common->Printf( "OpenURL '%s' failed: %s\n", url, SDL_GetError() );
	}
	
	// if we are going to quit, only accept a single URL before quitting
	if( quit )
	{
		quit_spamguard = true;
		cmdSystem->BufferCommandText( CMD_EXEC_APPEND, "quit\n" );
	}
}

/*
 ==================
 Sys_DoPreferences
 ==================
 */
void Sys_DoPreferences() { }

#if 0
/*
================
Sys_FPU_SetDAZ
================
*/
void Sys_FPU_SetDAZ( bool enable )
{
	/*
	DWORD dwData;
	
	_asm {
		movzx	ecx, byte ptr enable
		and		ecx, 1
		shl		ecx, 6
		STMXCSR	dword ptr dwData
		mov		eax, dwData
		and		eax, ~(1<<6)	// clear DAX bit
		or		eax, ecx		// set the DAZ bit
		mov		dwData, eax
		LDMXCSR	dword ptr dwData
	}
	*/
}

/*
================
Sys_FPU_SetFTZ
================
*/
void Sys_FPU_SetFTZ( bool enable )
{
	/*
	DWORD dwData;
	
	_asm {
		movzx	ecx, byte ptr enable
		and		ecx, 1
		shl		ecx, 15
		STMXCSR	dword ptr dwData
		mov		eax, dwData
		and		eax, ~(1<<15)	// clear FTZ bit
		or		eax, ecx		// set the FTZ bit
		mov		dwData, eax
		LDMXCSR	dword ptr dwData
	}
	*/
}
#endif

/*
===============
mem consistency stuff
===============
*/

#ifdef ID_MCHECK

const char* mcheckstrings[] =
{
	"MCHECK_DISABLED",
	"MCHECK_OK",
	"MCHECK_FREE",	// block freed twice
	"MCHECK_HEAD",	// memory before the block was clobbered
	"MCHECK_TAIL"	// memory after the block was clobbered
};

void abrt_func( mcheck_status status )
{
	Sys_Printf( "memory consistency failure: %s\n", mcheckstrings[ status + 1 ] );
	Posix_SetExit( EXIT_FAILURE );
	common->Quit();
}

#endif


/*
========================
Sys_GetCmdLine
========================
*/
const char* Sys_GetCmdLine()
{
	// DG: don't use this, use cmdargv and cmdargc instead!
	static idStr cmdLine;

	cmdLine.Clear();
	for( int i = 1; i < cmdargc; ++i )
	{
		if( i > 1 )
		{
			cmdLine.Append( ' ' );
		}

		const char* arg = cmdargv[i];
		const bool needsQuotes = ( arg[0] == '\0' || strchr( arg, ' ' ) != NULL );
		if( needsQuotes )
		{
			cmdLine.Append( '"' );
		}
		cmdLine.Append( arg );
		if( needsQuotes )
		{
			cmdLine.Append( '"' );
		}
	}

	return cmdLine.c_str();
}

/*
========================
Sys_GetCallStack
========================
*/
void Sys_GetCallStack( address_t *callStack, const int callStackSize )
{
	int numFrames = backtrace( ( void ** )callStack, callStackSize );
	for( int i = numFrames; i < callStackSize; i++ )
	{
		callStack[i] = 0;
	}
}

/*
========================
Sys_GetCallStackStr
========================
*/
const char *Sys_GetCallStackStr( const address_t *callStack, const int callStackSize )
{
	static char text[8192];
	text[0] = '\0';

	char **symbols = backtrace_symbols( ( void * const * )callStack, callStackSize );
	if( symbols != NULL )
	{
		int len = 0;
		for( int i = 0; i < callStackSize && len < ( int )sizeof( text ) - 1; i++ )
		{
			int ret = idStr::snPrintf( text + len, sizeof( text ) - len, "%s\n", symbols[i] );
			if( ret < 0 )
			{
				break;
			}
			len += ret;
		}
		free( symbols );
	}

	return text;
}

/*
========================
Sys_GetCallStackCurStr
========================
*/
const char *Sys_GetCallStackCurStr( int depth )
{
	address_t callStack[64];

	if( depth <= 0 || depth > 64 )
	{
		depth = 64;
	}

	Sys_GetCallStack( callStack, depth );

	return Sys_GetCallStackStr( callStack, depth );
}

/*
========================
Sys_GetCallStackCurAddressStr
========================
*/
const char *Sys_GetCallStackCurAddressStr( int depth )
{
	static char text[2048];
	address_t callStack[64];

	if( depth <= 0 || depth > 64 )
	{
		depth = 64;
	}

	Sys_GetCallStack( callStack, depth );

	int len = 0;
	for( int i = 0; i < depth && len < ( int )sizeof( text ) - 1; i++ )
	{
		int ret = idStr::snPrintf( text + len, sizeof( text ) - len, "0x%lx\n", ( unsigned long )callStack[i] );
		if( ret < 0 )
		{
			break;
		}
		len += ret;
	}

	return text;
}

/*
========================
Sys_ShutdownSymbols
========================
*/
void Sys_ShutdownSymbols()
{
}

/*
========================
Sys_Launch
========================
*/
void Sys_Launch( const char * path, idCmdArgs & args, void * launchData, unsigned int launchDataSize )
{
	idList<const char *> argv;
	argv.Append( Sys_EXEPath() );
	for( int i = 0; i < args.Argc(); i++ )
	{
		argv.Append( args.Argv( i ) );
	}
	argv.Append( NULL );

	pid_t pid = fork();
	if( pid < 0 )
	{
		idLib::Warning( "Sys_Launch(): Couldn't fork(), reason: %s", strerror( errno ) );
		return;
	}

	if( pid == 0 )
	{
		execv( path, ( char ** )argv.Ptr() );
		idLib::Warning( "Sys_Launch(): execv() failed, reason: %s", strerror( errno ) );
		_exit( 1 );
	}

	cmdSystem->AppendCommandText( "quit\n" );
}

/*
========================
Sys_ReLaunch
========================
*/
void Sys_ReLaunch( void * launchData, unsigned int launchDataSize )
{
	// DG: implementing this... basic old fork() exec() (+ setsid()) routine..
	// NOTE: this function used to have parameters: the commandline arguments, but as one string..
	//       for Linux/Unix we want one char* per argument so we'll just add the friggin'
	//       " +set com_skipIntroVideos 1" to the other commandline arguments in this function.
	
	int ret = fork();
	if( ret < 0 )
		idLib::Error( "Sys_ReLaunch(): Couldn't fork(), reason: %s ", strerror( errno ) );
		
	if( ret == 0 )
	{
		// child process
		
		// get our own session so we don't depend on the (soon to be killed)
		// parent process anymore - else we'll freeze
		pid_t sId = setsid();
		if( sId == ( pid_t ) - 1 )
		{
			idLib::Error( "Sys_ReLaunch(): setsid() failed! Reason: %s ", strerror( errno ) );
		}
		
		// close all FDs (except for stdin/out/err) so we don't leak FDs
		DIR* devfd = opendir( "/dev/fd" );
		if( devfd != NULL )
		{
			struct dirent entry;
			struct dirent* result = NULL;
			while( readdir_r( devfd, &entry, &result ) == 0 && result != NULL )
			{
				const char* filename = result->d_name;
				char* endptr = NULL;
				long int fd = strtol( filename, &endptr, 0 );
				if( endptr != filename && fd > STDERR_FILENO )
					close( fd );
			}
			closedir( devfd );
		}
		else
		{
			idLib::Warning( "Sys_ReLaunch(): Couldn't open /dev/fd/ - will leak file descriptors. Reason: %s", strerror( errno ) );
		}
		
		const char* exepath = Sys_EXEPath();

		// argv[0] must be the executable name, followed by the original
		// command line arguments, then "+set com_skipIntroVideos 1" and NULL.
		int argc = cmdargc + 4;
		const char** argv = ( const char** )calloc( argc, sizeof( char* ) );
		if( argv == NULL )
		{
			_exit( 1 );
		}

		int i = 0;
		argv[i++] = exepath;
		for( int a = 1; a < cmdargc; ++a )
			argv[i++] = cmdargv[a];
			
		// add +set com_skipIntroVideos 1
		argv[i++] = "+set";
		argv[i++] = "com_skipIntroVideos";
		argv[i++] = "1";
		// execv expects NULL terminated array
		argv[i] = NULL;
		
		errno = 0;
		execv( exepath, ( char** )argv );
		// we only get here if execv() fails, else the executable is restarted
		idLib::Error( "Sys_ReLaunch(): WTF exec() failed! Reason: %s ", strerror( errno ) );
		
	}
	else
	{
		// original process
		// just do a clean shutdown
		cmdSystem->AppendCommandText( "quit\n" );
	}
	// DG end
}

/*
===============
main
===============
*/
int main( int argc, const char** argv )
{
	// DG: needed for Sys_ReLaunch()
	cmdargc = argc;
	cmdargv = argv;
	// DG end
#ifdef ID_MCHECK
	// must have -lmcheck linkage
	mcheck( abrt_func );
	Sys_Printf( "memory consistency checking enabled\n" );
#endif
	
	Posix_EarlyInit( );
	
	if( argc > 1 )
	{
		common->Init( argc - 1, &argv[1], NULL );
	}
	else
	{
		common->Init( 0, NULL, NULL );
	}
	
	Posix_LateInit( );
	
	while( 1 )
	{
		common->Frame();
	}
}
