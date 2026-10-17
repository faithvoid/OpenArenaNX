// SPDX-License-Identifier: GPL-2.0-or-later
// Exit-crash diagnostics: linker --wrap hooks that trace shutdown to sdmc:/OpenArenaNX_exit.log.

#include <switch.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>

#include "../qcommon/q_shared.h"

#define EXITLOG_PATH "sdmc:/OpenArenaNX_exit.log"

static qboolean logStarted = qfalse;
// Set while Com_Frame runs, so an exit request seen then came from SDL's event pump.
static qboolean inFrame = qfalse;

static void ExitLog( const char *fmt, ... ) __attribute__ ((format (printf, 1, 2)));

static void ExitLog_AtExit( void )
{
	ExitLog( "atexit handlers running" );
}

static void ExitLog_Append( const char *text )
{
	FILE *f;

	if ( !logStarted ) {
		logStarted = qtrue;
		f = fopen( EXITLOG_PATH, "w" );
		if ( f ) {
			fprintf( f, "OpenArenaNX exit log, applet type %d\n", (int)appletGetAppletType( ) );
			fclose( f );
		}
		atexit( ExitLog_AtExit );
	}

	// Reopen per write so each line reaches the SD card before a crash.
	f = fopen( EXITLOG_PATH, "a" );
	if ( !f )
		return;
	fputs( text, f );
	fclose( f );
}

static void ExitLog( const char *fmt, ... )
{
	char msg[512];
	char line[640];
	va_list argptr;

	va_start( argptr, fmt );
	vsnprintf( msg, sizeof( msg ), fmt, argptr );
	va_end( argptr );

	snprintf( line, sizeof( line ), "[exitlog %llu ms] %s\n",
		(unsigned long long)( armTicksToNs( armGetSystemTick( ) ) / 1000000ULL ), msg );
	ExitLog_Append( line );
}

#define WRAP_VOID( name ) \
	void __real_##name( void ); \
	void __wrap_##name( void ) \
	{ \
		ExitLog( "-> " #name ); \
		__real_##name( ); \
		ExitLog( "<- " #name ); \
	}

WRAP_VOID( CL_cURL_Shutdown )
WRAP_VOID( S_DisableSounds )
WRAP_VOID( CL_ShutdownCGame )
WRAP_VOID( CL_ShutdownUI )
WRAP_VOID( GLimp_Shutdown )
WRAP_VOID( IN_Shutdown )
WRAP_VOID( Hunk_Clear )
WRAP_VOID( CM_ClearMap )
WRAP_VOID( S_Shutdown )
WRAP_VOID( S_CodecShutdown )
WRAP_VOID( CL_ShutdownInput )
WRAP_VOID( Con_Shutdown )
WRAP_VOID( CON_Shutdown )
WRAP_VOID( SDL_Quit )
WRAP_VOID( NET_Shutdown )
WRAP_VOID( Sys_PlatformExit )
WRAP_VOID( socketExit )

void __real_Sys_Print( const char *msg );
void __wrap_Sys_Print( const char *msg )
{
	ExitLog_Append( msg );
	__real_Sys_Print( msg );
}

void __real_Com_Frame( void );
void __wrap_Com_Frame( void )
{
	inFrame = qtrue;
	__real_Com_Frame( );
	inFrame = qfalse;
}

bool __real_appletMainLoop( void );
bool __wrap_appletMainLoop( void )
{
	bool ret = __real_appletMainLoop( );

	if ( !ret )
		ExitLog( "appletMainLoop() returned false, read by %s",
			inFrame ? "SDL event pump (inside Com_Frame)" : "main loop (sys_main_nx.c)" );
	return ret;
}

void __real_Cbuf_ExecuteText( int exec_when, const char *text );
void __wrap_Cbuf_ExecuteText( int exec_when, const char *text )
{
	if ( text && !Q_stricmpn( text, "quit", 4 ) )
		ExitLog( "Cbuf_ExecuteText(%d, \"%.*s\")", exec_when, (int)strcspn( text, "\r\n" ), text );
	__real_Cbuf_ExecuteText( exec_when, text );
}

void __real_Sys_Quit( void ) __attribute__ ((noreturn));
void __wrap_Sys_Quit( void )
{
	ExitLog( "-> Sys_Quit" );
	__real_Sys_Quit( );
}

void __real_SV_Shutdown( char *finalmsg );
void __wrap_SV_Shutdown( char *finalmsg )
{
	ExitLog( "-> SV_Shutdown(\"%s\")", finalmsg );
	__real_SV_Shutdown( finalmsg );
	ExitLog( "<- SV_Shutdown" );
}

void __real_CL_Shutdown( char *finalmsg, qboolean disconnect, qboolean quit );
void __wrap_CL_Shutdown( char *finalmsg, qboolean disconnect, qboolean quit )
{
	ExitLog( "-> CL_Shutdown(\"%s\", %d, %d)", finalmsg, disconnect, quit );
	__real_CL_Shutdown( finalmsg, disconnect, quit );
	ExitLog( "<- CL_Shutdown" );
}

void __real_FS_Shutdown( qboolean closemfp );
void __wrap_FS_Shutdown( qboolean closemfp )
{
	ExitLog( "-> FS_Shutdown(%d)", closemfp );
	__real_FS_Shutdown( closemfp );
	ExitLog( "<- FS_Shutdown" );
}

void __real_SDL_QuitSubSystem( Uint32 flags );
void __wrap_SDL_QuitSubSystem( Uint32 flags )
{
	ExitLog( "-> SDL_QuitSubSystem(0x%x)", (unsigned int)flags );
	__real_SDL_QuitSubSystem( flags );
	ExitLog( "<- SDL_QuitSubSystem(0x%x)", (unsigned int)flags );
}

Result __real_hidStopSixAxisSensor( HidSixAxisSensorHandle handle );
Result __wrap_hidStopSixAxisSensor( HidSixAxisSensorHandle handle )
{
	Result rc;

	ExitLog( "-> hidStopSixAxisSensor" );
	rc = __real_hidStopSixAxisSensor( handle );
	ExitLog( "<- hidStopSixAxisSensor rc=0x%x", (unsigned int)rc );
	return rc;
}

// ALC types are opaque here; the AL headers are not needed for pointer pass-through.
void __real_alcDestroyContext( void *context );
void __wrap_alcDestroyContext( void *context )
{
	ExitLog( "-> alcDestroyContext(%p)", context );
	__real_alcDestroyContext( context );
	ExitLog( "<- alcDestroyContext" );
}

char __real_alcCloseDevice( void *device );
char __wrap_alcCloseDevice( void *device )
{
	char ret;

	ExitLog( "-> alcCloseDevice(%p)", device );
	ret = __real_alcCloseDevice( device );
	ExitLog( "<- alcCloseDevice ret=%d", ret );
	return ret;
}

Result __real_appletUnlockExit( void );
Result __wrap_appletUnlockExit( void )
{
	Result rc;

	ExitLog( "-> appletUnlockExit" );
	rc = __real_appletUnlockExit( );
	ExitLog( "<- appletUnlockExit rc=0x%x", (unsigned int)rc );
	return rc;
}

void __real_exit( int code ) __attribute__ ((noreturn));
void __wrap_exit( int code )
{
	ExitLog( "-> exit(%d)", code );
	__real_exit( code );
}
