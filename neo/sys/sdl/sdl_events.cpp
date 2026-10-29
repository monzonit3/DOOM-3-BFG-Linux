/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.
Copyright (C) 2012 dhewg (dhewm3)
Copyright (C) 2012 Robert Beckebans

This file is part of the Doom 3 GPL Source Code ("Doom 3 Source Code").

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

#include "idlib/precompiled.h"

#include <SDL3/SDL.h>

#include "renderer/tr_local.h"
#include "sdl_local.h"
#include "posix/posix_public.h"

// DG: those are needed for moving/resizing windows
extern idCVar r_windowX;
extern idCVar r_windowY;
extern idCVar r_windowWidth;
extern idCVar r_windowHeight;
// DG end

const char* kbdNames[] =
{
	"english", "french", "german", "italian", "spanish", "turkish", "norwegian", NULL
};

idCVar in_kbd( "in_kbd", "english", CVAR_SYSTEM | CVAR_ARCHIVE | CVAR_NOCHEAT, "keyboard layout", kbdNames, idCmdSystem::ArgCompletion_String<kbdNames> );

struct kbd_poll_t
{
	int key;
	bool state;

	kbd_poll_t()
	{
	}

	kbd_poll_t( int k, bool s )
	{
		key = k;
		state = s;
	}
};

struct mouse_poll_t
{
	int action;
	int value;

	mouse_poll_t()
	{
	}

	mouse_poll_t( int a, int v )
	{
		action = a;
		value = v;
	}
};

static idList<kbd_poll_t> kbd_polls;
static idList<mouse_poll_t> mouse_polls;

// implemented further down, kept here so they can be called from the input init/shutdown
void Sys_InitJoystick();
void Sys_ShutdownJoystick();
void Sys_HandleJoystickEvent( const SDL_Event & ev );

/*
========================
SDL_KeyToDoom3Key
========================
*/
static int SDL_KeyToDoom3Key( SDL_Keycode key, bool& isChar )
{
	isChar = false;

	if( key >= SDLK_SPACE && key < SDLK_DELETE )
	{
		isChar = true;
	}

	switch( key )
	{
		case SDLK_ESCAPE:
			return K_ESCAPE;

		case SDLK_SPACE:
			return K_SPACE;

		case SDLK_0:
			return K_0;

		case SDLK_1:
			return K_1;

		case SDLK_2:
			return K_2;

		case SDLK_3:
			return K_3;

		case SDLK_4:
			return K_4;

		case SDLK_5:
			return K_5;

		case SDLK_6:
			return K_6;

		case SDLK_7:
			return K_7;

		case SDLK_8:
			return K_8;

		case SDLK_9:
			return K_9;

		case SDLK_UNDERSCORE:
			return K_UNDERLINE;

		case SDLK_MINUS:
			return K_MINUS;

		case SDLK_COMMA:
			return K_COMMA;

		case SDLK_COLON:
			return K_COLON;

		case SDLK_SEMICOLON:
			return K_SEMICOLON;

		case SDLK_PERIOD:
			return K_PERIOD;

		case SDLK_AT:
			return K_AT;

		case SDLK_EQUALS:
			return K_EQUALS;

		case SDLK_A:
			return K_A;

		case SDLK_B:
			return K_B;

		case SDLK_C:
			return K_C;

		case SDLK_D:
			return K_D;

		case SDLK_E:
			return K_E;

		case SDLK_F:
			return K_F;

		case SDLK_G:
			return K_G;

		case SDLK_H:
			return K_H;

		case SDLK_I:
			return K_I;

		case SDLK_J:
			return K_J;

		case SDLK_K:
			return K_K;

		case SDLK_L:
			return K_L;

		case SDLK_M:
			return K_M;

		case SDLK_N:
			return K_N;

		case SDLK_O:
			return K_O;

		case SDLK_P:
			return K_P;

		case SDLK_Q:
			return K_Q;

		case SDLK_R:
			return K_R;

		case SDLK_S:
			return K_S;

		case SDLK_T:
			return K_T;

		case SDLK_U:
			return K_U;

		case SDLK_V:
			return K_V;

		case SDLK_W:
			return K_W;

		case SDLK_X:
			return K_X;

		case SDLK_Y:
			return K_Y;

		case SDLK_Z:
			return K_Z;

		case SDLK_RETURN:
			return K_ENTER;

		case SDLK_BACKSPACE:
			return K_BACKSPACE;

		case SDLK_PAUSE:
			return K_PAUSE;

		case SDLK_TAB:
			return K_TAB;

		case SDLK_CAPSLOCK:
			return K_CAPSLOCK;

		case SDLK_SCROLLLOCK:
			return K_SCROLL;

		case SDLK_POWER:
			return K_POWER;

		case SDLK_UP:
			return K_UPARROW;

		case SDLK_DOWN:
			return K_DOWNARROW;

		case SDLK_LEFT:
			return K_LEFTARROW;

		case SDLK_RIGHT:
			return K_RIGHTARROW;

		case SDLK_LGUI:
			return K_LWIN;

		case SDLK_RGUI:
			return K_RWIN;

		case SDLK_LALT:
			return K_LALT;

		case SDLK_RALT:
			return K_RALT;

		case SDLK_RCTRL:
			return K_RCTRL;

		case SDLK_LCTRL:
			return K_LCTRL;

		case SDLK_RSHIFT:
			return K_RSHIFT;

		case SDLK_LSHIFT:
			return K_LSHIFT;

		case SDLK_INSERT:
			return K_INS;

		case SDLK_DELETE:
			return K_DEL;

		case SDLK_PAGEDOWN:
			return K_PGDN;

		case SDLK_PAGEUP:
			return K_PGUP;

		case SDLK_HOME:
			return K_HOME;

		case SDLK_END:
			return K_END;

		case SDLK_F1:
			return K_F1;

		case SDLK_F2:
			return K_F2;

		case SDLK_F3:
			return K_F3;

		case SDLK_F4:
			return K_F4;

		case SDLK_F5:
			return K_F5;

		case SDLK_F6:
			return K_F6;

		case SDLK_F7:
			return K_F7;

		case SDLK_F8:
			return K_F8;

		case SDLK_F9:
			return K_F9;

		case SDLK_F10:
			return K_F10;

		case SDLK_F11:
			return K_F11;

		case SDLK_F12:
			return K_F12;

		case SDLK_F13:
			return K_F13;

		case SDLK_F14:
			return K_F14;

		case SDLK_F15:
			return K_F15;

		case SDLK_KP_7:
			return K_KP_7;

		case SDLK_KP_8:
			return K_KP_8;

		case SDLK_KP_9:
			return K_KP_9;

		case SDLK_KP_4:
			return K_KP_4;

		case SDLK_KP_5:
			return K_KP_5;

		case SDLK_KP_6:
			return K_KP_6;

		case SDLK_KP_1:
			return K_KP_1;

		case SDLK_KP_2:
			return K_KP_2;

		case SDLK_KP_3:
			return K_KP_3;

		case SDLK_KP_ENTER:
			return K_KP_ENTER;

		case SDLK_KP_0:
			return K_KP_0;

		case SDLK_KP_PERIOD:
			return K_KP_DOT;

		case SDLK_KP_DIVIDE:
			return K_KP_SLASH;

		case SDLK_KP_MINUS:
			return K_KP_MINUS;

		case SDLK_KP_PLUS:
			return K_KP_PLUS;

		case SDLK_NUMLOCKCLEAR:
			return K_NUMLOCK;

		case SDLK_KP_MULTIPLY:
			return K_KP_STAR;

		case SDLK_KP_EQUALS:
			return K_KP_EQUALS;

		case SDLK_PRINTSCREEN:
			return K_PRINTSCREEN;

		case SDLK_MODE:
			return K_RALT;
	}

	return 0;
}

/*
================
Sys_GetKeyName

Returns the name SDL uses for the physical key the engine key number refers to,
which follows the currently selected keyboard layout.
================
*/
const char * Sys_GetKeyName( int keyNum )
{
	// the engine's key numbers are DirectInput scan codes, so the SDL scancode for a
	// key number has to be looked up by translating every scancode back
	static keyNum_t scancodeToKeyNum[SDL_SCANCODE_COUNT] = { K_NONE };
	static bool tableBuilt = false;

	if ( !tableBuilt ) {
		for( int s = 0; s < SDL_SCANCODE_COUNT; s++ )
		{
			bool isChar = false;
			scancodeToKeyNum[s] = ( keyNum_t )SDL_KeyToDoom3Key( SDL_GetKeyFromScancode( ( SDL_Scancode )s, SDL_KMOD_NONE, false ), isChar );
		}
		tableBuilt = true;
	}

	for( int s = 0; s < SDL_SCANCODE_COUNT; s++ )
	{
		if( scancodeToKeyNum[s] != ( keyNum_t )keyNum )
		{
			continue;
		}

		const char* name = SDL_GetScancodeName( ( SDL_Scancode )s );
		if( name != NULL && name[0] != '\0' )
		{
			return name;
		}
	}

	return NULL;
}

static void PushConsoleEvent( const char* s )
{
	char* b;
	size_t len;

	len = strlen( s ) + 1;
	b = ( char* )Mem_Alloc( len, TAG_EVENTS );
	strcpy( b, s );

	SDL_Event event;

	event.type = SDL_EVENT_USER;
	event.user.code = SE_CONSOLE;
	event.user.data1 = ( void* )len;
	event.user.data2 = b;

	SDL_PushEvent( &event );
}

/*
=================
FilterGamepadEvents

Gamepad state is polled directly through SDL_GetGamepadButton/Axis, so the
per-frame axis/touchpad/sensor event stream is redundant and can flood the
main event queue. Keep buttons and hotplug events; drop the high-frequency ones.
=================
*/
static bool SDLCALL FilterGamepadEvents( void *userdata, SDL_Event *event )
{
	switch( event->type )
	{
		case SDL_EVENT_GAMEPAD_AXIS_MOTION:
		case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
		case SDL_EVENT_GAMEPAD_BUTTON_UP:
		case SDL_EVENT_GAMEPAD_REMAPPED:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_DOWN:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_MOTION:
		case SDL_EVENT_GAMEPAD_TOUCHPAD_UP:
		case SDL_EVENT_GAMEPAD_SENSOR_UPDATE:
		case SDL_EVENT_GAMEPAD_UPDATE_COMPLETE:
		// The engine uses the SDL gamepad API; raw joystick motion events are redundant too.
		case SDL_EVENT_JOYSTICK_AXIS_MOTION:
		case SDL_EVENT_JOYSTICK_BALL_MOTION:
		case SDL_EVENT_JOYSTICK_HAT_MOTION:
		case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
		case SDL_EVENT_JOYSTICK_BUTTON_UP:
		case SDL_EVENT_JOYSTICK_BATTERY_UPDATED:
		case SDL_EVENT_JOYSTICK_UPDATE_COMPLETE:
			return false;
		default:
			return true;
	}
}
/*
=================
GamepadButtonToKey
=================
*/
static int GamepadButtonToKey( SDL_GamepadButton button )
{
	switch( button )
	{
		case SDL_GAMEPAD_BUTTON_SOUTH:          return K_JOY1;
		case SDL_GAMEPAD_BUTTON_EAST:           return K_JOY2;
		case SDL_GAMEPAD_BUTTON_WEST:           return K_JOY3;
		case SDL_GAMEPAD_BUTTON_NORTH:          return K_JOY4;
		case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:  return K_JOY5;
		case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return K_JOY6;
		case SDL_GAMEPAD_BUTTON_LEFT_STICK:     return K_JOY7;
		case SDL_GAMEPAD_BUTTON_RIGHT_STICK:    return K_JOY8;
		case SDL_GAMEPAD_BUTTON_START:          return K_JOY9;
		case SDL_GAMEPAD_BUTTON_BACK:           return K_JOY10;
		case SDL_GAMEPAD_BUTTON_GUIDE:          return K_JOY11;
		case SDL_GAMEPAD_BUTTON_DPAD_UP:        return K_JOY_DPAD_UP;
		case SDL_GAMEPAD_BUTTON_DPAD_DOWN:      return K_JOY_DPAD_DOWN;
		case SDL_GAMEPAD_BUTTON_DPAD_LEFT:      return K_JOY_DPAD_LEFT;
		case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:     return K_JOY_DPAD_RIGHT;
		default:                                return K_NONE;
	}
}

/*
=================
GamepadAxisToDoomAxis
=================
*/
static int GamepadAxisToDoomAxis( SDL_GamepadAxis axis )
{
	switch( axis )
	{
		case SDL_GAMEPAD_AXIS_LEFTX:         return AXIS_LEFT_X;
		case SDL_GAMEPAD_AXIS_LEFTY:         return AXIS_LEFT_Y;
		case SDL_GAMEPAD_AXIS_RIGHTX:        return AXIS_RIGHT_X;
		case SDL_GAMEPAD_AXIS_RIGHTY:        return AXIS_RIGHT_Y;
		case SDL_GAMEPAD_AXIS_LEFT_TRIGGER:  return AXIS_LEFT_TRIG;
		case SDL_GAMEPAD_AXIS_RIGHT_TRIGGER: return AXIS_RIGHT_TRIG;
		default:                             return -1;
	}
}

static idList<sysEvent_t> gamepadMenuEvents;

static void QueueGamepadMenuEvents();

/*
=================
GamepadMenuActive
=================
*/
static bool GamepadMenuActive()
{
	return game && ( game->Shell_IsActive() || game->IsPDAOpen() );
}

/*
=================
Sys_InitInput
=================
*/
void Sys_InitInput()
{
	kbd_polls.SetGranularity( 64 );
	mouse_polls.SetGranularity( 64 );

	SDL_SetEventFilter( FilterGamepadEvents, NULL );

	Sys_InitJoystick();

	in_kbd.SetModified();
}

/*
=================
Sys_ShutdownInput
=================
*/
void Sys_ShutdownInput()
{
	Sys_ShutdownJoystick();

	kbd_polls.Clear();
	mouse_polls.Clear();
}

/*
===========
Sys_InitScanTable
===========
*/
// Windows has its own version due to the tools
void Sys_InitScanTable()
{
}

/*
===============
Sys_GetConsoleKey
===============
*/
unsigned char Sys_GetConsoleKey( bool shifted )
{
	static unsigned char keys[2] = { '`', '~' };

	if( in_kbd.IsModified() )
	{
		idStr lang = in_kbd.GetString();

		if( lang.Length() )
		{
			if( !lang.Icmp( "french" ) )
			{
				keys[0] = '<';
				keys[1] = '>';
			}
			else if( !lang.Icmp( "german" ) )
			{
				keys[0] = '^';
				keys[1] = 176; // degree
			}
			else if( !lang.Icmp( "italian" ) )
			{
				keys[0] = '\\';
				keys[1] = '|';
			}
			else if( !lang.Icmp( "spanish" ) )
			{
				keys[0] = 186;
				keys[1] = 170;
			}
			else if( !lang.Icmp( "turkish" ) )
			{
				keys[0] = '\"';
				keys[1] = 233;
			}
			else if( !lang.Icmp( "norwegian" ) )
			{
				keys[0] = 124;
				keys[1] = 167;
			}
		}

		in_kbd.ClearModified();
	}

	return shifted ? keys[1] : keys[0];
}

/*
===============
Sys_MapCharForKey
===============
*/
unsigned char Sys_MapCharForKey( int key )
{
	return key & 0xff;
}

/*
===============
Sys_GrabMouseCursor
===============
*/
void Sys_GrabMouseCursor( bool grabIt )
{
	int flags;

	if( grabIt )
	{
		flags = GRAB_ENABLE | GRAB_SETSTATE;
	}
	else
	{
		flags = GRAB_SETSTATE;
	}

	GLimp_GrabInput( flags );
}

/*
================
Sys_GetEvent
================
*/
sysEvent_t Sys_GetEvent()
{
	SDL_Event ev;
	sysEvent_t res = { };
	int key;

	static const sysEvent_t res_none = { SE_NONE, 0, 0, 0, NULL };

	QueueGamepadMenuEvents();

	if ( gamepadMenuEvents.Num() > 0 ) {
		const sysEvent_t event = gamepadMenuEvents[0];
		gamepadMenuEvents.RemoveIndex( 0 );
		return event;
	}

	static char* s = NULL;
	static size_t s_pos = 0;

	if( s )
	{
		res.evType = SE_CHAR;
		res.evValue = s[s_pos];

		s_pos++;
		if( !s[s_pos] )
		{
			free( s );
			s = NULL;
			s_pos = 0;
		}

		return res;
	}

	// fake a "mousewheel not pressed anymore" event
	// so scrolling in menus stops after one step
	static int mwheelRel = 0;
	if( mwheelRel )
	{
		res.evType = SE_KEY;
		res.evValue = mwheelRel;
		res.evValue2 = 0; // "not pressed anymore"
		mwheelRel = 0;
		return res;
	}

	static byte c = 0;

	if( c )
	{
		res.evType = SE_CHAR;
		res.evValue = c;

		c = 0;

		return res;
	}

	if( SDL_PollEvent( &ev ) )
	{
		switch( ev.type )
		{
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
			{
				// unset modifier, in case alt-tab was used to leave window and ALT is still set
				// as that can cause fullscreen-toggling when pressing enter...
				SDL_Keymod currentmod = SDL_GetModState();
				SDL_Keymod newmod = SDL_KMOD_NONE;
				if( currentmod & SDL_KMOD_CAPS ) // preserve capslock
				{
					newmod |= SDL_KMOD_CAPS;
				}

				SDL_SetModState( newmod );

				// un-pause the game when focus is gained, that also re-grabs the input
				cvarSystem->SetCVarBool( "com_pause", false );
				break;
			}

			case SDL_EVENT_WINDOW_FOCUS_LOST:
				// pause the game when focus is lost, that also un-grabs the input
				cvarSystem->SetCVarBool( "com_pause", true );
				break;

			// window coordinates are logical points; the framebuffer size is in pixels
			case SDL_EVENT_WINDOW_RESIZED:
			{
				r_windowWidth.SetInteger( ev.window.data1 );
				r_windowHeight.SetInteger( ev.window.data2 );
				break;
			}

			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			{
				// in desktop fullscreen the renderer keeps its logical resolution and scales in GL,
				// so don't replace it with the window's (desktop) pixel size
				if( !GLimp_IsLogicalScaling() )
				{
					glConfig.nativeScreenWidth = ev.window.data1;
					glConfig.nativeScreenHeight = ev.window.data2;
				}
				break;
			}

			case SDL_EVENT_WINDOW_MOVED:
			{
				int x = ev.window.data1;
				int y = ev.window.data2;
				r_windowX.SetInteger( x );
				r_windowY.SetInteger( y );
				break;
			}

			case SDL_EVENT_WINDOW_MOUSE_LEAVE:
				res.evType = SE_MOUSE_LEAVE;
				return res;

			case SDL_EVENT_KEY_DOWN:
				if( ev.key.key == SDLK_RETURN && ( ev.key.mod & SDL_KMOD_ALT ) > 0 )
				{
					// go to fullscreen on current display at its highest available videomode,
					// instead of always first display
					int fullscreen = 0;
					if( !renderSystem->IsFullScreen() )
					{
						const int displayNum = GLimp_GetCurrentDisplayIndex();
						idList<vidMode_t> modeList;
						if( R_GetModeListForDisplay( displayNum, modeList ) && modeList.Num() > 0 )
						{
							// r_fullScreen is 1 based, the mode list is sorted from lowest to highest resolution
							fullscreen = displayNum + 1;
							cvarSystem->SetCVarInteger( "r_vidMode", modeList.Num() - 1 );
						}
						else
						{
							// no mode list: fall back to "fullscreen on current window"
							fullscreen = -2;
						}
					}
					cvarSystem->SetCVarInteger( "r_fullscreen", fullscreen );
					PushConsoleEvent( "vid_restart" );
					return res_none;
				}

				// ctrl-g to un-grab mouse
				if( ev.key.key == SDLK_G && ( ev.key.mod & SDL_KMOD_CTRL ) > 0 )
				{
					bool grab = cvarSystem->GetCVarBool( "in_nograb" );
					grab = !grab;
					cvarSystem->SetCVarBool( "in_nograb", grab );
					return res_none;
				}

				// fall through
			case SDL_EVENT_KEY_UP:
			{
				bool isChar;

				// special case for SDL_SCANCODE_GRAVE - the console key under Esc
				if( ev.key.scancode == SDL_SCANCODE_GRAVE )
				{
					key = K_GRAVE;
					c = K_BACKSPACE; // bad hack to get empty console inputline..
				}
				else
				{
					key = SDL_KeyToDoom3Key( ev.key.key, isChar );

					if( key == 0 )
					{
						// check if its an unmapped console key
						unsigned char uc = Sys_MapCharForKey( ( int )ev.key.key );
						if( uc == Sys_GetConsoleKey( false ) || uc == Sys_GetConsoleKey( true ) )
						{
							key = K_GRAVE;
							c = K_BACKSPACE; // bad hack to get empty console inputline..
						}
						else
						{
							if( ev.type == SDL_EVENT_KEY_DOWN )
							{
								common->Warning( "unmapped SDL key %d (0x%x) scancode %d", ev.key.key, ev.key.key, ev.key.scancode );
							}
							return res_none;
						}
					}
				}

				res.evType = SE_KEY;
				res.evValue = key;
				res.evValue2 = ev.key.down ? 1 : 0;

				kbd_polls.Append( kbd_poll_t( key, ev.key.down ) );

				if( key == K_BACKSPACE && ev.key.down )
				{
					c = key;
				}

				return res;
			}

			case SDL_EVENT_TEXT_INPUT:
				if( ev.text.text && *ev.text.text )
				{
					if( !ev.text.text[1] )
					{
						c = *ev.text.text;
					}
					else
					{
						s = strdup( ev.text.text );
					}
				}

				return res_none;

			case SDL_EVENT_MOUSE_MOTION:
				// return event with absolute mouse-coordinates when a SWF menu is active
				// to fix cursor problems in windowed/high-DPI mode
				if( game && ( game->Shell_IsActive() || game->IsPDAOpen() ) )
				{
					int mouseX = ( int )ev.motion.x;
					int mouseY = ( int )ev.motion.y;
					GLimp_AdjustMouseCoordinates( mouseX, mouseY );

					res.evType = SE_MOUSE_ABSOLUTE;
					res.evValue = mouseX;
					res.evValue2 = mouseY;
				}
				else
				{
					res.evType = SE_MOUSE;
					res.evValue = ev.motion.xrel;
					res.evValue2 = ev.motion.yrel;
				}

				mouse_polls.Append( mouse_poll_t( M_DELTAX, ev.motion.xrel ) );
				mouse_polls.Append( mouse_poll_t( M_DELTAY, ev.motion.yrel ) );

				return res;

			case SDL_EVENT_MOUSE_WHEEL:
				res.evType = SE_KEY;

				if( ev.wheel.y > 0 )
				{
					res.evValue = K_MWHEELUP;
					mouse_polls.Append( mouse_poll_t( M_DELTAZ, 1 ) );
				}
				else
				{
					res.evValue = K_MWHEELDOWN;
					mouse_polls.Append( mouse_poll_t( M_DELTAZ, -1 ) );
				}

				// remember mousewheel direction to issue a "not pressed anymore" event
				mwheelRel = res.evValue;

				res.evValue2 = 1;

				return res;

			case SDL_EVENT_MOUSE_BUTTON_DOWN:
			case SDL_EVENT_MOUSE_BUTTON_UP:
				res.evType = SE_KEY;

				switch( ev.button.button )
				{
					case SDL_BUTTON_LEFT:
						res.evValue = K_MOUSE1;
						mouse_polls.Append( mouse_poll_t( M_ACTION1, ev.button.down ? 1 : 0 ) );
						break;
					case SDL_BUTTON_MIDDLE:
						res.evValue = K_MOUSE3;
						mouse_polls.Append( mouse_poll_t( M_ACTION3, ev.button.down ? 1 : 0 ) );
						break;
					case SDL_BUTTON_RIGHT:
						res.evValue = K_MOUSE2;
						mouse_polls.Append( mouse_poll_t( M_ACTION2, ev.button.down ? 1 : 0 ) );
						break;
				}

				res.evValue2 = ev.button.down ? 1 : 0;

				return res;

			case SDL_EVENT_QUIT:
				PushConsoleEvent( "quit" );
				return res_none;

			case SDL_EVENT_USER:
				switch( ev.user.code )
				{
					case SE_CONSOLE:
						res.evType = SE_CONSOLE;
						res.evPtrLength = ( intptr_t )ev.user.data1;
						res.evPtr = ev.user.data2;
						return res;
					default:
						common->Warning( "unknown user event %u", ev.user.code );
						return res_none;
				}

			// joystick/gamepad input is polled through the dedicated Sys_PollJoystickInputEvents() API
			case SDL_EVENT_GAMEPAD_ADDED:
			case SDL_EVENT_GAMEPAD_REMOVED:
				Sys_HandleJoystickEvent( ev );
				return res_none;

			case SDL_EVENT_GAMEPAD_AXIS_MOTION:
			case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
			case SDL_EVENT_GAMEPAD_BUTTON_UP:
			case SDL_EVENT_GAMEPAD_REMAPPED:
				return res_none;

			default:
				return res_none;
		}
	}

	return res_none;
}

/*
================
Sys_ClearEvents
================
*/
void Sys_ClearEvents()
{
	SDL_Event ev;

	while( SDL_PollEvent( &ev ) )
		;

	kbd_polls.SetNum( 0 );
	mouse_polls.SetNum( 0 );
}

/*
================
Sys_GenerateEvents
================
*/
void Sys_GenerateEvents()
{
	char* s = Posix_ConsoleInput();

	if( s )
	{
		PushConsoleEvent( s );
	}

	SDL_PumpEvents();
}

/*
================
Sys_PollKeyboardInputEvents
================
*/
int Sys_PollKeyboardInputEvents()
{
	return kbd_polls.Num();
}

/*
================
Sys_ReturnKeyboardInputEvent
================
*/
int Sys_ReturnKeyboardInputEvent( const int n, int& key, bool& state )
{
	if( n >= kbd_polls.Num() )
	{
		return 0;
	}

	key = kbd_polls[n].key;
	state = kbd_polls[n].state;
	return 1;
}

/*
================
Sys_EndKeyboardInputEvents
================
*/
void Sys_EndKeyboardInputEvents()
{
	kbd_polls.SetNum( 0 );
}

/*
================
Sys_PollMouseInputEvents
================
*/
int Sys_PollMouseInputEvents( int mouseEvents[MAX_MOUSE_EVENTS][2] )
{
	int numEvents = mouse_polls.Num();

	if( numEvents > MAX_MOUSE_EVENTS )
	{
		numEvents = MAX_MOUSE_EVENTS;
	}

	for( int i = 0; i < numEvents; i++ )
	{
		const mouse_poll_t& mp = mouse_polls[i];

		mouseEvents[i][0] = mp.action;
		mouseEvents[i][1] = mp.value;
	}

	mouse_polls.SetNum( 0 );

	return numEvents;
}

//=====================================================================================
//	Joystick / Gamepad Input Handling
//
//	Uses the SDL3 Gamepad API. Rumble uses SDL_RumbleGamepad() (the Haptic subsystem
//	is intentionally not used).
//=====================================================================================

static const int MAX_GAMEPADS = MAX_INPUT_DEVICES;

static SDL_Gamepad* gamepads[MAX_GAMEPADS] = { NULL };
static SDL_JoystickID gamepadIds[MAX_GAMEPADS] = { 0 };
static int lastRumbleLow[MAX_GAMEPADS] = { 0 };
static int lastRumbleHigh[MAX_GAMEPADS] = { 0 };
static int nextRumbleRefresh[MAX_GAMEPADS] = { 0 };

struct joy_poll_t
{
	int device;
	int action;
	int value;
};

static idList<joy_poll_t> joy_polls;

static int FindGamepadSlot( SDL_JoystickID id )
{
	for( int i = 0; i < MAX_GAMEPADS; i++ )
	{
		if( gamepads[i] && gamepadIds[i] == id )
		{
			return i;
		}
	}
	return -1;
}

static void OpenGamepad( SDL_JoystickID id )
{
	if( FindGamepadSlot( id ) >= 0 )
	{
		return;
	}

	for( int i = 0; i < MAX_GAMEPADS; i++ )
	{
		if( !gamepads[i] )
		{
			SDL_Gamepad* gp = SDL_OpenGamepad( id );
			if( gp )
			{
				gamepads[i] = gp;
				gamepadIds[i] = id;
				lastRumbleLow[i] = 0;
				lastRumbleHigh[i] = 0;
				nextRumbleRefresh[i] = 0;
				common->DPrintf( "Opened gamepad %d: %s\n", i, SDL_GetGamepadName( gp ) );
			}
			return;
		}
	}

	common->DPrintf( "Too many gamepads, ignoring gamepad id %u\n", ( unsigned )id );
}

static void CloseGamepad( SDL_JoystickID id )
{
	int slot = FindGamepadSlot( id );
	if( slot >= 0 )
	{
		SDL_CloseGamepad( gamepads[slot] );
		gamepads[slot] = NULL;
		gamepadIds[slot] = 0;
	}
}

void Sys_InitJoystick()
{
	int numJoysticks = 0;
	SDL_JoystickID* joysticks = SDL_GetGamepads( &numJoysticks );
	if( joysticks )
	{
		for( int i = 0; i < numJoysticks; i++ )
		{
			OpenGamepad( joysticks[i] );
		}
		SDL_free( joysticks );
	}
}

void Sys_ShutdownJoystick()
{
	for( int i = 0; i < MAX_GAMEPADS; i++ )
	{
		if( gamepads[i] )
		{
			SDL_CloseGamepad( gamepads[i] );
			gamepads[i] = NULL;
			gamepadIds[i] = 0;
		}
	}
	joy_polls.Clear();
}

/*
===============
Sys_SetRumble
===============
*/
void Sys_SetRumble( int device, int low, int hi )
{
	if( device < 0 || device >= MAX_GAMEPADS || !gamepads[device] )
	{
		return;
	}

	// SDL_RumbleGamepad expects 0..0xFFFF; the engine passes 0..255-ish values
	Uint16 lowFreq = ( Uint16 )idMath::ClampInt( 0, 0xFFFF, low * 256 );
	Uint16 highFreq = ( Uint16 )idMath::ClampInt( 0, 0xFFFF, hi * 256 );
	const int now = Sys_Milliseconds();
	const bool changed = lowFreq != lastRumbleLow[device] || highFreq != lastRumbleHigh[device];

	// ProcessGameReturn calls this every frame. Avoid rewriting the same FF effect
	// constantly; only refresh active rumble periodically.
	if ( !changed && ( lowFreq == 0 && highFreq == 0 ) ) {
		return;
	}
	if ( !changed && now < nextRumbleRefresh[device] ) {
		return;
	}

	lastRumbleLow[device] = lowFreq;
	lastRumbleHigh[device] = highFreq;
	nextRumbleRefresh[device] = now + 100;

	SDL_RumbleGamepad( gamepads[device], lowFreq, highFreq, ( lowFreq || highFreq ) ? 250 : 0 );
}

/*
===============
Sys_PollJoystickInputEvents
===============
*/
int Sys_PollJoystickInputEvents( int deviceNum )
{
	// Make sure the gamepad state is current even if the event loop is not pumping.
	SDL_UpdateGamepads();

	joy_polls.Clear();

	int baseDevice = ( deviceNum >= 0 && deviceNum < MAX_GAMEPADS && gamepads[deviceNum] ) ? deviceNum : -1;
	if ( baseDevice < 0 )
	{
		for ( int i = 0; i < MAX_GAMEPADS; i++ )
		{
			if ( gamepads[i] )
			{
				baseDevice = i;
				break;
			}
		}
	}

	// If no pad is open yet, periodically rescan so hotplug works even if the
	// SDL added event was missed.
	if ( baseDevice < 0 )
	{
		static int lastRescan = 0;
		const int now = Sys_Milliseconds();
		if ( now - lastRescan > 1000 )
		{
			lastRescan = now;
			Sys_InitJoystick();
			for ( int i = 0; i < MAX_GAMEPADS; i++ )
			{
				if ( gamepads[i] )
				{
					baseDevice = i;
					break;
				}
			}
		}
	}

	if ( baseDevice < 0 )
	{
		return 0;
	}

	// Aggregate all open pads. Any connected controller can drive player 0.
	static const struct { SDL_GamepadButton button; int action; } buttonMap[] =
	{
		{ SDL_GAMEPAD_BUTTON_SOUTH,          J_ACTION1 },
		{ SDL_GAMEPAD_BUTTON_EAST,           J_ACTION2 },
		{ SDL_GAMEPAD_BUTTON_WEST,           J_ACTION3 },
		{ SDL_GAMEPAD_BUTTON_NORTH,          J_ACTION4 },
		{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  J_ACTION5 },
		{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, J_ACTION6 },
		{ SDL_GAMEPAD_BUTTON_LEFT_STICK,     J_ACTION7 },
		{ SDL_GAMEPAD_BUTTON_RIGHT_STICK,    J_ACTION8 },
		{ SDL_GAMEPAD_BUTTON_START,          J_ACTION9 },
		{ SDL_GAMEPAD_BUTTON_BACK,           J_ACTION10 },
		{ SDL_GAMEPAD_BUTTON_GUIDE,          J_ACTION11 },
		{ SDL_GAMEPAD_BUTTON_DPAD_UP,        J_DPAD_UP },
		{ SDL_GAMEPAD_BUTTON_DPAD_DOWN,      J_DPAD_DOWN },
		{ SDL_GAMEPAD_BUTTON_DPAD_LEFT,      J_DPAD_LEFT },
		{ SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     J_DPAD_RIGHT },
	};

	for ( unsigned i = 0; i < sizeof( buttonMap ) / sizeof( buttonMap[0] ); i++ )
	{
		int value = 0;
		for ( int j = 0; j < MAX_GAMEPADS; j++ )
		{
			if ( gamepads[j] && SDL_GetGamepadButton( gamepads[j], buttonMap[i].button ) )
			{
				value = 1;
				break;
			}
		}
		joy_polls.Append( joy_poll_t{ baseDevice, buttonMap[i].action, value } );
	}

	// axes (-32768..32767)
	static const struct { SDL_GamepadAxis axis; int action; } axisMap[] =
	{
		{ SDL_GAMEPAD_AXIS_LEFTX,        J_AXIS_LEFT_X },
		{ SDL_GAMEPAD_AXIS_LEFTY,        J_AXIS_LEFT_Y },
		{ SDL_GAMEPAD_AXIS_RIGHTX,       J_AXIS_RIGHT_X },
		{ SDL_GAMEPAD_AXIS_RIGHTY,       J_AXIS_RIGHT_Y },
		{ SDL_GAMEPAD_AXIS_LEFT_TRIGGER, J_AXIS_LEFT_TRIG },
		{ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, J_AXIS_RIGHT_TRIG },
	};

	for ( unsigned i = 0; i < sizeof( axisMap ) / sizeof( axisMap[0] ); i++ )
	{
		Sint16 value = 0;
		int best = 0;
		for ( int j = 0; j < MAX_GAMEPADS; j++ )
		{
			if ( !gamepads[j] )
			{
				continue;
			}
			Sint16 v = SDL_GetGamepadAxis( gamepads[j], axisMap[i].axis );
			int magnitude = v < 0 ? -( int )v : ( int )v;
			if ( magnitude > best )
			{
				best = magnitude;
				value = v;
			}
		}
		joy_polls.Append( joy_poll_t{ baseDevice, axisMap[i].action, value } );
	}

	return joy_polls.Num();
}

/*
===============
QueueGamepadMenuEvents

Queues gamepad buttons (always) and menu-navigation axis events. This is
independent of the gameplay in_useJoystick option, matching the Windows
input sampling thread.
===============
*/
static void QueueGamepadMenuEvents()
{
	static Uint64 lastCheck = 0;
	const Uint64 now = SDL_GetTicksNS();
	if ( now - lastCheck < 4000000ULL ) {
		return;
	}
	lastCheck = now;

	SDL_UpdateGamepads();

	int baseDevice = -1;
	for ( int i = 0; i < MAX_GAMEPADS; i++ ) {
		if ( gamepads[i] ) {
			baseDevice = i;
			break;
		}
	}
	if ( baseDevice < 0 ) {
		return;
	}

	static const struct { SDL_GamepadButton button; int action; } buttonMap[] = {
		{ SDL_GAMEPAD_BUTTON_SOUTH,          J_ACTION1 },
		{ SDL_GAMEPAD_BUTTON_EAST,           J_ACTION2 },
		{ SDL_GAMEPAD_BUTTON_WEST,           J_ACTION3 },
		{ SDL_GAMEPAD_BUTTON_NORTH,          J_ACTION4 },
		{ SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,  J_ACTION5 },
		{ SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, J_ACTION6 },
		{ SDL_GAMEPAD_BUTTON_LEFT_STICK,     J_ACTION7 },
		{ SDL_GAMEPAD_BUTTON_RIGHT_STICK,    J_ACTION8 },
		{ SDL_GAMEPAD_BUTTON_START,          J_ACTION9 },
		{ SDL_GAMEPAD_BUTTON_BACK,           J_ACTION10 },
		{ SDL_GAMEPAD_BUTTON_GUIDE,          J_ACTION11 },
		{ SDL_GAMEPAD_BUTTON_DPAD_UP,        J_DPAD_UP },
		{ SDL_GAMEPAD_BUTTON_DPAD_DOWN,      J_DPAD_DOWN },
		{ SDL_GAMEPAD_BUTTON_DPAD_LEFT,      J_DPAD_LEFT },
		{ SDL_GAMEPAD_BUTTON_DPAD_RIGHT,     J_DPAD_RIGHT },
	};

	static int lastButton[MAX_JOY_EVENT] = { 0 };
	for ( unsigned i = 0; i < sizeof( buttonMap ) / sizeof( buttonMap[0] ); i++ ) {
		const int action = buttonMap[i].action;
		int value = 0;
		for ( int j = 0; j < MAX_GAMEPADS; j++ ) {
			if ( gamepads[j] && SDL_GetGamepadButton( gamepads[j], buttonMap[i].button ) ) {
				value = 1;
				break;
			}
		}
		if ( value != lastButton[action] ) {
			lastButton[action] = value;
			const int key = GamepadButtonToKey( buttonMap[i].button );
			if ( key != K_NONE ) {
				gamepadMenuEvents.Append( sysEvent_t{ SE_KEY, key, value ? 1 : 0, 0, NULL, baseDevice } );
			}
		}
	}

	if ( !GamepadMenuActive() ) {
		return;
	}

	static const struct { SDL_GamepadAxis axis; int action; } axisMap[] = {
		{ SDL_GAMEPAD_AXIS_LEFTX,        J_AXIS_LEFT_X },
		{ SDL_GAMEPAD_AXIS_LEFTY,        J_AXIS_LEFT_Y },
		{ SDL_GAMEPAD_AXIS_RIGHTX,       J_AXIS_RIGHT_X },
		{ SDL_GAMEPAD_AXIS_RIGHTY,       J_AXIS_RIGHT_Y },
		{ SDL_GAMEPAD_AXIS_LEFT_TRIGGER, J_AXIS_LEFT_TRIG },
		{ SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, J_AXIS_RIGHT_TRIG },
	};

	static Sint16 lastAxis[MAX_JOYSTICK_AXIS] = { 0 };
	static int lastDir[6][2] = { { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };
	static const int dirNegKey[6] = { K_JOY_STICK1_LEFT, K_JOY_STICK1_UP, K_JOY_STICK2_LEFT, K_JOY_STICK2_UP, K_NONE, K_NONE };
	static const int dirPosKey[6] = { K_JOY_STICK1_RIGHT, K_JOY_STICK1_DOWN, K_JOY_STICK2_RIGHT, K_JOY_STICK2_DOWN, K_JOY_TRIGGER1, K_JOY_TRIGGER2 };

	for ( unsigned i = 0; i < sizeof( axisMap ) / sizeof( axisMap[0] ); i++ ) {
		Sint16 value = 0;
		int best = 0;
		for ( int j = 0; j < MAX_GAMEPADS; j++ ) {
			if ( !gamepads[j] ) {
				continue;
			}
			Sint16 v = SDL_GetGamepadAxis( gamepads[j], axisMap[i].axis );
			int magnitude = v < 0 ? -( int )v : ( int )v;
			if ( magnitude > best ) {
				best = magnitude;
				value = v;
			}
		}
		const int axis = axisMap[i].action - J_AXIS_MIN;
		if ( axis < 0 || axis >= MAX_JOYSTICK_AXIS ) {
			continue;
		}

		const bool neg = value < -16384;
		const bool pos = value > 16384;
		for ( int dir = 0; dir < 2; dir++ ) {
			const bool state = ( dir == 0 ) ? neg : pos;
			if ( state != ( lastDir[axis][dir] != 0 ) ) {
				lastDir[axis][dir] = state ? 1 : 0;
				const int key = ( dir == 0 ) ? dirNegKey[axis] : dirPosKey[axis];
				if ( key != K_NONE ) {
					gamepadMenuEvents.Append( sysEvent_t{ SE_KEY, key, state ? 1 : 0, 0, NULL, baseDevice } );
				}
			}
		}

		if ( value != lastAxis[axis] ) {
			lastAxis[axis] = value;
			gamepadMenuEvents.Append( sysEvent_t{ SE_JOYSTICK, axis, ( value * 16 ) / 16384, 0, NULL, baseDevice } );
		}
	}
}

/*
===============
Sys_ReturnJoystickInputEvent
===============
*/
int Sys_ReturnJoystickInputEvent( const int n, int& action, int& value )
{
	if( n >= joy_polls.Num() )
	{
		return 0;
	}

	action = joy_polls[n].action;
	value = joy_polls[n].value;
	return 1;
}

/*
===============
Sys_EndJoystickInputEvents
===============
*/
void Sys_EndJoystickInputEvents()
{
	joy_polls.SetNum( 0 );
}

// handle hotplug events from the main event pump
void Sys_HandleJoystickEvent( const SDL_Event& ev );
void Sys_HandleJoystickEvent( const SDL_Event& ev )
{
	switch( ev.type )
	{
		case SDL_EVENT_GAMEPAD_ADDED:
			OpenGamepad( ev.gdevice.which );
			break;
		case SDL_EVENT_GAMEPAD_REMOVED:
			CloseGamepad( ev.gdevice.which );
			break;
	}
}
