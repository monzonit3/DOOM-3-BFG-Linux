/*
===========================================================================

Doom 3 GPL Source Code
Copyright (C) 1999-2011 id Software LLC, a ZeniMax Media company.
Copyright (C) 2012 dhewg (dhewm3)
Copyright (C) 2012 Robert Beckebans
Copyright (C) 2013 Daniel Gibson

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

#include <SDL3/SDL.h>

#include "idlib/precompiled.h"

#include "renderer/tr_local.h"
#include "sdl_local.h"

#include <glad.h>

idCVar in_nograb( "in_nograb", "0", CVAR_SYSTEM | CVAR_NOCHEAT, "prevents input grabbing" );

// RB: FIXME this shit. We need the OpenGL alpha channel for advanced rendering effects
idCVar r_waylandcompat( "r_waylandcompat", "0", CVAR_SYSTEM | CVAR_NOCHEAT | CVAR_ARCHIVE, "wayland compatible framebuffer" );

// RB: only relevant if using SDL 2.0/3.0
idCVar r_useOpenGL32( "r_useOpenGL32", "1", CVAR_INTEGER, "0 = OpenGL 2.0, 1 = OpenGL 3.2 compatibility profile, 2 = OpenGL 3.2 core profile", 0, 2 );
// RB end

extern idCVar r_windowX;
extern idCVar r_windowY;
extern idCVar r_windowWidth;
extern idCVar r_windowHeight;

static bool			grabbed = false;
static SDL_Window*	window = NULL;
static SDL_GLContext context = NULL;

static bool			logicalScaling = false;
static GLuint		logicalScalingFBO = 0;
static GLuint		logicalScalingColor = 0;
static GLuint		logicalScalingDepth = 0;
static GLuint		logicalScalingResolveFBO = 0;
static GLuint		logicalScalingResolveColor = 0;
static GLuint		logicalScalingResolveDepth = 0;
static int			logicalScalingWidth = 0;
static int			logicalScalingHeight = 0;
static int			logicalScalingSamples = 0;

static GLuint		windowResolveFBO = 0;
static GLuint		windowResolveColor = 0;
static GLuint		windowResolveDepth = 0;
static int			windowResolveWidth = 0;
static int			windowResolveHeight = 0;

/*
===================
GetLogicalScalingViewport

Calculates the aspect-preserving destination rectangle for the scaled framebuffer.
===================
*/
static void GetLogicalScalingViewport( int physicalWidth, int physicalHeight, int &x, int &y, int &w, int &h )
{
	x = y = 0;
	w = physicalWidth;
	h = physicalHeight;

	if( physicalWidth <= 0 || physicalHeight <= 0 || logicalScalingWidth <= 0 || logicalScalingHeight <= 0 )
	{
		return;
	}

	const float wantAspect = ( float )logicalScalingWidth / ( float )logicalScalingHeight;
	const float realAspect = ( float )physicalWidth / ( float )physicalHeight;

	if( wantAspect > realAspect )
	{
		// letterbox
		w = physicalWidth;
		h = ( int )( logicalScalingHeight * ( ( float )physicalWidth / ( float )logicalScalingWidth ) );
		y = ( physicalHeight - h ) / 2;
	}
	else if( wantAspect < realAspect )
	{
		// pillarbox
		h = physicalHeight;
		w = ( int )( logicalScalingWidth * ( ( float )physicalHeight / ( float )logicalScalingHeight ) );
		x = ( physicalWidth - w ) / 2;
	}

	if( w < 1 )
	{
		w = 1;
	}
	if( h < 1 )
	{
		h = 1;
	}
}

/*
===================
GLimp_DestroyLogicalScalingFBO
===================
*/
static void GLimp_DestroyLogicalScalingFBO()
{
	if( logicalScalingResolveDepth )
	{
		glDeleteRenderbuffers( 1, &logicalScalingResolveDepth );
		logicalScalingResolveDepth = 0;
	}
	if( logicalScalingResolveColor )
	{
		glDeleteRenderbuffers( 1, &logicalScalingResolveColor );
		logicalScalingResolveColor = 0;
	}
	if( logicalScalingResolveFBO )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glDeleteFramebuffers( 1, &logicalScalingResolveFBO );
		logicalScalingResolveFBO = 0;
	}
	if( logicalScalingDepth )
	{
		glDeleteRenderbuffers( 1, &logicalScalingDepth );
		logicalScalingDepth = 0;
	}
	if( logicalScalingColor )
	{
		glDeleteRenderbuffers( 1, &logicalScalingColor );
		logicalScalingColor = 0;
	}
	if( logicalScalingFBO )
	{
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glDeleteFramebuffers( 1, &logicalScalingFBO );
		logicalScalingFBO = 0;
	}

	logicalScaling = false;
	glConfig.logicalScaling = false;
	logicalScalingWidth = 0;
	logicalScalingHeight = 0;
	logicalScalingSamples = 0;
}

/*
===================
GLimp_DestroyWindowResolveFBO

Frees the single-sample target used to resolve the multisampled default
framebuffer before glCopyTexImage2D / glReadPixels are called in windowed mode.
===================
*/
static void GLimp_DestroyWindowResolveFBO()
{
	if( windowResolveDepth )
	{
		glDeleteRenderbuffers( 1, &windowResolveDepth );
		windowResolveDepth = 0;
	}
	if( windowResolveColor )
	{
		glDeleteRenderbuffers( 1, &windowResolveColor );
		windowResolveColor = 0;
	}
	if( windowResolveFBO )
	{
		glDeleteFramebuffers( 1, &windowResolveFBO );
		windowResolveFBO = 0;
	}
	windowResolveWidth = 0;
	windowResolveHeight = 0;
}

/*
===================
GLimp_CreateWindowResolveFBO
===================
*/
static bool GLimp_CreateWindowResolveFBO( int width, int height )
{
	GLimp_DestroyWindowResolveFBO();

	if( width <= 0 || height <= 0 || !glGenFramebuffers || !glGenRenderbuffers || !glRenderbufferStorage )
	{
		return false;
	}

	while( glGetError() != GL_NO_ERROR )
	{
	}

	glGenFramebuffers( 1, &windowResolveFBO );
	glGenRenderbuffers( 1, &windowResolveColor );
	glGenRenderbuffers( 1, &windowResolveDepth );

	glBindFramebuffer( GL_FRAMEBUFFER, windowResolveFBO );

	glBindRenderbuffer( GL_RENDERBUFFER, windowResolveColor );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, windowResolveColor );

	glBindRenderbuffer( GL_RENDERBUFFER, windowResolveDepth );
	glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, windowResolveDepth );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, windowResolveDepth );

	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	const GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	const GLenum error = glGetError();

	glBindFramebuffer( GL_FRAMEBUFFER, 0 );

	if( status != GL_FRAMEBUFFER_COMPLETE || error != GL_NO_ERROR )
	{
		GLimp_DestroyWindowResolveFBO();
		return false;
	}

	windowResolveWidth = width;
	windowResolveHeight = height;
	return true;
}

/*
===================
GLimp_CreateLogicalScalingFBO

Renders at the requested resolution and scales the result to the window in GLimp_SwapBuffers().
===================
*/
static bool GLimp_CreateLogicalScalingFBO( int width, int height, int samples )
{
	GLimp_DestroyLogicalScalingFBO();

	if( width <= 0 || height <= 0 )
	{
		return false;
	}

	if( !glGenFramebuffers || !glBindFramebuffer || !glBlitFramebuffer || !glGenRenderbuffers || !glRenderbufferStorage )
	{
		common->Warning( "Framebuffer objects are not available, using the native window resolution" );
		return false;
	}

	if( samples > 1 )
	{
		if( !glRenderbufferStorageMultisample )
		{
			samples = 0;
		}
		else
		{
			GLint maxSamples = 0;
			glGetIntegerv( GL_MAX_SAMPLES, &maxSamples );
			if( maxSamples < 2 )
			{
				samples = 0;
			}
			else if( samples > maxSamples )
			{
				samples = maxSamples;
			}
		}
	}
	else
	{
		samples = 0;
	}

	while( glGetError() != GL_NO_ERROR )
	{
	}

	glGenFramebuffers( 1, &logicalScalingFBO );
	glGenRenderbuffers( 1, &logicalScalingColor );
	glGenRenderbuffers( 1, &logicalScalingDepth );

	glBindFramebuffer( GL_FRAMEBUFFER, logicalScalingFBO );

	glBindRenderbuffer( GL_RENDERBUFFER, logicalScalingColor );
	if( samples > 1 )
	{
		glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, GL_RGBA8, width, height );
	}
	else
	{
		glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, width, height );
	}
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, logicalScalingColor );

	glBindRenderbuffer( GL_RENDERBUFFER, logicalScalingDepth );
	if( samples > 1 )
	{
		glRenderbufferStorageMultisample( GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height );
	}
	else
	{
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height );
	}
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, logicalScalingDepth );
	glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, logicalScalingDepth );

	glBindRenderbuffer( GL_RENDERBUFFER, 0 );

	GLenum status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
	GLenum error = glGetError();

	if( samples > 1 && ( status != GL_FRAMEBUFFER_COMPLETE || error != GL_NO_ERROR ) )
	{
		// the driver refused the multisampled attachments, retry without MSAA
		GLimp_DestroyLogicalScalingFBO();
		return GLimp_CreateLogicalScalingFBO( width, height, 0 );
	}

	if( status != GL_FRAMEBUFFER_COMPLETE || error != GL_NO_ERROR )
	{
		common->Warning( "Couldn't create %dx%d scaling framebuffer (status 0x%x, error 0x%x), using the native window resolution",
						 width, height, status, error );
		GLimp_DestroyLogicalScalingFBO();
		return false;
	}

	if( samples > 1 )
	{
		// A multisampled framebuffer cannot be scaled by glBlitFramebuffer, so resolve
		// it into a single-sample framebuffer at the same size first.
		glGenFramebuffers( 1, &logicalScalingResolveFBO );
		glGenRenderbuffers( 1, &logicalScalingResolveColor );
		glGenRenderbuffers( 1, &logicalScalingResolveDepth );

		glBindFramebuffer( GL_FRAMEBUFFER, logicalScalingResolveFBO );
		glBindRenderbuffer( GL_RENDERBUFFER, logicalScalingResolveColor );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_RGBA8, width, height );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, logicalScalingResolveColor );

		glBindRenderbuffer( GL_RENDERBUFFER, logicalScalingResolveDepth );
		glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, logicalScalingResolveDepth );
		glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, logicalScalingResolveDepth );

		glBindRenderbuffer( GL_RENDERBUFFER, 0 );

		status = glCheckFramebufferStatus( GL_FRAMEBUFFER );
		error = glGetError();

		if( status != GL_FRAMEBUFFER_COMPLETE || error != GL_NO_ERROR )
		{
			common->Warning( "Couldn't create %dx%d MSAA resolve framebuffer (status 0x%x, error 0x%x), disabling antialiasing",
							 width, height, status, error );
			GLimp_DestroyLogicalScalingFBO();
			return GLimp_CreateLogicalScalingFBO( width, height, 0 );
		}

		glBindFramebuffer( GL_FRAMEBUFFER, logicalScalingFBO );
	}

	logicalScaling = true;
	glConfig.logicalScaling = true;
	logicalScalingWidth = width;
	logicalScalingHeight = height;
	logicalScalingSamples = samples;

	glViewport( 0, 0, width, height );
	glScissor( 0, 0, width, height );

	return true;
}

/*
===================
GLimp_IsLogicalScaling
===================
*/
bool GLimp_IsLogicalScaling()
{
	return logicalScaling;
}

/*
===================
GLimp_ResolveFramebuffer

A multisampled rendertarget cannot be read by glCopyTexImage2D or glReadPixels,
so resolve it into the single-sample target and bind that as the read framebuffer.
The render target stays bound for writing, so rendering can continue afterwards.
===================
*/
void GLimp_ResolveFramebuffer( bool resolveDepth )
{
	GLbitfield mask = GL_COLOR_BUFFER_BIT;
	if( resolveDepth )
	{
		mask |= GL_DEPTH_BUFFER_BIT;
	}

	if( logicalScaling )
	{
		if( logicalScalingSamples <= 1 || !logicalScalingResolveFBO )
		{
			return;
		}

		glBindFramebuffer( GL_READ_FRAMEBUFFER, logicalScalingFBO );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, logicalScalingResolveFBO );
		glDrawBuffer( GL_COLOR_ATTACHMENT0 );

		glBlitFramebuffer( 0, 0, logicalScalingWidth, logicalScalingHeight,
						   0, 0, logicalScalingWidth, logicalScalingHeight,
						   mask, GL_NEAREST );

		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, logicalScalingFBO );
		glBindFramebuffer( GL_READ_FRAMEBUFFER, logicalScalingResolveFBO );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );
		return;
	}

	// windowed: the default framebuffer may be multisampled itself
	GLint samples = 0;
	glGetIntegerv( GL_SAMPLES, &samples );
	if( samples <= 1 || !window )
	{
		return;
	}

	int width = 0;
	int height = 0;
	if( !SDL_GetWindowSizeInPixels( window, &width, &height ) || width <= 0 || height <= 0 )
	{
		return;
	}

	if( !windowResolveFBO || width != windowResolveWidth || height != windowResolveHeight )
	{
		if( !GLimp_CreateWindowResolveFBO( width, height ) )
		{
			return;
		}
	}

	glBindFramebuffer( GL_READ_FRAMEBUFFER, 0 );
	glReadBuffer( GL_BACK );
	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, windowResolveFBO );
	glDrawBuffer( GL_COLOR_ATTACHMENT0 );

	glBlitFramebuffer( 0, 0, width, height,
					   0, 0, width, height,
					   mask, GL_NEAREST );

	glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
	glBindFramebuffer( GL_READ_FRAMEBUFFER, windowResolveFBO );
	glReadBuffer( GL_COLOR_ATTACHMENT0 );
}

/*
===================
GLimp_AdjustMouseCoordinates

Maps SDL window-space mouse coordinates into the renderer's framebuffer coordinate space.
On high-DPI displays the window size is in points while the framebuffer is in pixels.
===================
*/
void GLimp_AdjustMouseCoordinates( int &x, int &y )
{
	if( !window )
	{
		return;
	}

	int windowWidth = 0;
	int windowHeight = 0;
	if( !SDL_GetWindowSize( window, &windowWidth, &windowHeight ) || windowWidth <= 0 || windowHeight <= 0 )
	{
		return;
	}

	int drawableWidth = windowWidth;
	int drawableHeight = windowHeight;
	int pixelWidth = 0;
	int pixelHeight = 0;
	if( SDL_GetWindowSizeInPixels( window, &pixelWidth, &pixelHeight ) && pixelWidth > 0 && pixelHeight > 0 )
	{
		drawableWidth = pixelWidth;
		drawableHeight = pixelHeight;
	}

	// SDL mouse coordinates are in window coordinates, which are logical points on high-DPI displays.
	const float pixelX = x * ( float )drawableWidth / ( float )windowWidth;
	const float pixelY = y * ( float )drawableHeight / ( float )windowHeight;

	if( !logicalScaling )
	{
		x = ( int )pixelX;
		y = ( int )pixelY;
		return;
	}

	int viewportX, viewportY, viewportWidth, viewportHeight;
	GetLogicalScalingViewport( drawableWidth, drawableHeight, viewportX, viewportY, viewportWidth, viewportHeight );

	const float scaleX = ( float )logicalScalingWidth / ( float )viewportWidth;
	const float scaleY = ( float )logicalScalingHeight / ( float )viewportHeight;

	int adjustedX = ( int )( ( pixelX - viewportX ) * scaleX );
	int adjustedY = ( int )( ( pixelY - viewportY ) * scaleY );

	if( adjustedX < 0 )
	{
		adjustedX = 0;
	}
	else if( adjustedX > logicalScalingWidth )
	{
		adjustedX = logicalScalingWidth;
	}
	if( adjustedY < 0 )
	{
		adjustedY = 0;
	}
	else if( adjustedY > logicalScalingHeight )
	{
		adjustedY = logicalScalingHeight;
	}

	x = adjustedX;
	y = adjustedY;
}

/*
===================
GLimp_PreInit

 R_GetModeListForDisplay is called before GLimp_Init(), but SDL needs SDL_Init() first.
 So do that in GLimp_PreInit()
 Calling that function more than once doesn't make a difference
===================
*/
void GLimp_PreInit()
{
	if( !SDL_WasInit( SDL_INIT_VIDEO ) )
	{
		if( !SDL_Init( SDL_INIT_VIDEO ) )
		{
			common->Error( "Error while initializing SDL: %s", SDL_GetError() );
		}
	}
	if( !SDL_WasInit(  SDL_INIT_GAMEPAD ) )
	{
		SDL_AddGamepadMappingsFromFile("gamecontrollerdb.txt");
		if( !SDL_Init( SDL_INIT_GAMEPAD ) )
		{
		}
	}
}

/*
===================
ClampWindowedParms

Clamps a requested window rectangle to the usable display area. Window
coordinates are logical points, not framebuffer pixels.
===================
*/
static void ClampWindowedParms( glimpParms_t &parms )
{
	SDL_Point point = { parms.x, parms.y };
	SDL_DisplayID display = SDL_GetDisplayForPoint( &point );
	if( display == 0 )
	{
		display = SDL_GetPrimaryDisplay();
	}

	SDL_Rect bounds;
	if( display == 0 || !SDL_GetDisplayUsableBounds( display, &bounds ) )
	{
		return;
	}

	if( parms.width > bounds.w )
	{
		parms.width = bounds.w;
	}
	if( parms.height > bounds.h )
	{
		parms.height = bounds.h;
	}
	if( parms.width < 1 )
	{
		parms.width = 1;
	}
	if( parms.height < 1 )
	{
		parms.height = 1;
	}

	if( parms.x < bounds.x )
	{
		parms.x = bounds.x;
	}
	if( parms.y < bounds.y )
	{
		parms.y = bounds.y;
	}
	if( parms.x + parms.width > bounds.x + bounds.w )
	{
		parms.x = bounds.x + bounds.w - parms.width;
	}
	if( parms.y + parms.height > bounds.y + bounds.h )
	{
		parms.y = bounds.y + bounds.h - parms.height;
	}

	r_windowX.SetInteger( parms.x );
	r_windowY.SetInteger( parms.y );
	r_windowWidth.SetInteger( parms.width );
	r_windowHeight.SetInteger( parms.height );
}

/*
===================
GLimp_Init
===================
*/
bool GLimp_Init( glimpParms_t parms )
{
	common->Printf( "Initializing OpenGL subsystem\n" );

	GLimp_PreInit();

	// high pixel density is mandatory on Wayland; the drawable size is queried in pixels below
	SDL_WindowFlags flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |  SDL_WINDOW_HIGH_PIXEL_DENSITY;

	if( parms.fullScreen )
	{
		flags |= SDL_WINDOW_FULLSCREEN;
	}
	else
	{
		ClampWindowedParms( parms );
	}

	int colorbits = 24;
	int depthbits = 24;
	int stencilbits = 8;

	for( int i = 0; i < 16; i++ )
	{
		// 0 - default
		// 1 - minus colorbits
		// 2 - minus depthbits
		// 3 - minus stencil
		if( ( i % 4 ) == 0 && i )
		{
			// one pass, reduce
			switch( i / 4 )
			{
				case 2 :
					if( colorbits == 24 )
					{
						colorbits = 16;
					}
					break;
				case 1 :
					if( depthbits == 24 )
					{
						depthbits = 16;
					}
					else if( depthbits == 16 )
					{
						depthbits = 8;
					}
				case 3 :
					if( stencilbits == 24 )
					{
						stencilbits = 16;
					}
					else if( stencilbits == 16 )
					{
						stencilbits = 8;
					}
			}
		}

		int tcolorbits = colorbits;
		int tdepthbits = depthbits;
		int tstencilbits = stencilbits;

		if( ( i % 4 ) == 3 )
		{
			// reduce colorbits
			if( tcolorbits == 24 )
			{
				tcolorbits = 16;
			}
		}

		if( ( i % 4 ) == 2 )
		{
			// reduce depthbits
			if( tdepthbits == 24 )
			{
				tdepthbits = 16;
			}
			else if( tdepthbits == 16 )
			{
				tdepthbits = 8;
			}
		}

		if( ( i % 4 ) == 1 )
		{
			// reduce stencilbits
			if( tstencilbits == 24 )
			{
				tstencilbits = 16;
			}
			else if( tstencilbits == 16 )
			{
				tstencilbits = 8;
			}
			else
			{
				tstencilbits = 0;
			}
		}

		int channelcolorbits = 4;
		if( tcolorbits == 24 )
		{
			channelcolorbits = 8;
		}

		SDL_GL_SetAttribute( SDL_GL_RED_SIZE, channelcolorbits );
		SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, channelcolorbits );
		SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, channelcolorbits );
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, tdepthbits );
		SDL_GL_SetAttribute( SDL_GL_STENCIL_SIZE, tstencilbits );

		if( r_waylandcompat.GetBool() )
		{
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, 0 );
		}
		else
		{
			SDL_GL_SetAttribute( SDL_GL_ALPHA_SIZE, channelcolorbits );
		}

		SDL_GL_SetAttribute( SDL_GL_STEREO, parms.stereo ? 1 : 0 );

		// The desktop-fullscreen path renders into an offscreen scaling FBO, which is
		// multisampled separately (and resolved before blitting), so keep window
		// multisampling off for fullscreen.
		const int windowMultiSamples = ( parms.fullScreen != 0 ) ? 0 : parms.multiSamples;
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, windowMultiSamples ? 1 : 0 );
		SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, windowMultiSamples );

		// RB begin
		if( r_useOpenGL32.GetInteger() > 0 )
		{
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 2 );

			if( r_debugContext.GetBool() )
			{
				SDL_GL_SetAttribute( SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG );
			}
		}

		if( r_useOpenGL32.GetInteger() > 1 )
		{
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE );
		}
		else if( r_useOpenGL32.GetInteger() > 0 )
		{
			// the default 3.2 context must be the compatibility profile, the shaders rely on
			// legacy built-ins like gl_Color / gl_FragColor and tex2D()
			SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
		}
		// RB end

		window = SDL_CreateWindow( GAME_NAME, parms.width, parms.height, flags );
		if( !window )
		{
			common->DPrintf( "Couldn't set GL mode %d/%d/%d: %s",
							 channelcolorbits, tdepthbits, tstencilbits, SDL_GetError() );
			continue;
		}

		if( parms.fullScreen == 0 )
		{
			SDL_SetWindowPosition( window, parms.x, parms.y );
		}

		// use desktop (borderless) fullscreen and scale in GL instead of an exclusive mode change
		SDL_SetWindowFullscreenMode( window, NULL );

		context = SDL_GL_CreateContext( window );
		if( !context )
		{
			common->Warning( "Couldn't create OpenGL context: %s", SDL_GetError() );
			SDL_DestroyWindow( window );
			window = NULL;
			continue;
		}

		if( !SDL_GL_SetSwapInterval( r_swapInterval.GetInteger() ) )
		{
			common->Warning( "SDL_GL_SetSwapInterval not supported" );
		}

		// high-DPI/Wayland: the framebuffer size in pixels is what the GL viewport must use
		SDL_GetWindowSizeInPixels( window, &glConfig.nativeScreenWidth, &glConfig.nativeScreenHeight );

		// SDL3 does not expose a reliable physical display width. Use a typical
		// desktop monitor width so stereo separation stays finite; users can
		// override it with r_forceScreenWidthCentimeters.
		glConfig.physicalScreenWidthInCentimeters = 50.0f;

		glConfig.isFullscreen = ( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN ) == SDL_WINDOW_FULLSCREEN;

		common->Printf( "Using %d color bits, %d depth, %d stencil display\n",
						channelcolorbits, tdepthbits, tstencilbits );

		glConfig.colorBits = tcolorbits;
		glConfig.depthBits = tdepthbits;
		glConfig.stencilBits = tstencilbits;

		// RB begin
		glConfig.displayFrequency = 60;
		glConfig.isStereoPixelFormat = parms.stereo;
		glConfig.multisamples = windowMultiSamples;

		glConfig.pixelAspect = 1.0f;
		// RB end

		break;
	}

	if( !window )
	{
		common->Printf( "No usable GL mode found: %s", SDL_GetError() );
		return false;
	}

	// load all OpenGL entry points through glad
	if( !gladLoadGLLoader( ( GLADloadproc )SDL_GL_GetProcAddress ) )
	{
		common->Error( "Couldn't initialize the OpenGL function loader (glad)!" );
	}

	// Ask the actual GL context, not just the requested SDL attribute.
	GLint actualStereo = 0;
	glGetIntegerv( GL_STEREO, &actualStereo );
	glConfig.stereoPixelFormatAvailable = ( actualStereo == GL_TRUE );
	glConfig.isStereoPixelFormat = glConfig.stereoPixelFormatAvailable;

	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSizeInPixels( window, &drawableWidth, &drawableHeight );

	if( parms.fullScreen != 0 )
	{
		// desktop fullscreen: keep the window at the desktop resolution and render into a
		// framebuffer at the requested video mode, then scale it to the window when swapping
		if( GLimp_CreateLogicalScalingFBO( parms.width, parms.height, parms.multiSamples ) )
		{
			glConfig.nativeScreenWidth = parms.width;
			glConfig.nativeScreenHeight = parms.height;
			glConfig.multisamples = logicalScalingSamples;
		}
		else
		{
			glConfig.nativeScreenWidth = drawableWidth > 0 ? drawableWidth : parms.width;
			glConfig.nativeScreenHeight = drawableHeight > 0 ? drawableHeight : parms.height;
		}
	}
	else
	{
		GLimp_DestroyLogicalScalingFBO();
		glConfig.nativeScreenWidth = drawableWidth > 0 ? drawableWidth : parms.width;
		glConfig.nativeScreenHeight = drawableHeight > 0 ? drawableHeight : parms.height;
	}

	SDL_HideCursor();

	return true;
}

/*
===================
GLimp_GetCurrentDisplayIndex

Returns the 0 based index of the display the window is currently on.
===================
*/
int GLimp_GetCurrentDisplayIndex()
{
	SDL_DisplayID displayId = SDL_GetDisplayForWindow( window );
	if( displayId == 0 )
	{
		displayId = SDL_GetPrimaryDisplay();
	}

	int numDisplays = 0;
	SDL_DisplayID* displays = SDL_GetDisplays( &numDisplays );
	if( displays == NULL )
	{
		return 0;
	}

	int displayNum = 0;
	for( int i = 0; i < numDisplays; i++ )
	{
		if( displays[ i ] == displayId )
		{
			displayNum = i;
			break;
		}
	}
	SDL_free( displays );

	return displayNum;
}

/*
===================
 Helper functions for GLimp_SetScreenParms()
===================
*/

// makes sure the window will be full-screened on the right display and returns the SDL display id
static SDL_DisplayID ScreenParmsHandleDisplayIndex( glimpParms_t parms )
{
	SDL_DisplayID displayId;

	if( parms.fullScreen > 0 )
	{
		int numDisplays = 0;
		SDL_DisplayID* displays = SDL_GetDisplays( &numDisplays );
		if( parms.fullScreen > numDisplays || displays == NULL )
		{
			common->Warning( "Can't set fullscreen mode to display number %i, because SDL only knows about %i displays!",
							 parms.fullScreen, numDisplays );
			if( displays )
			{
				SDL_free( displays );
			}
			return 0;
		}
		displayId = displays[ parms.fullScreen - 1 ];
		SDL_free( displays );
	}
	else // -2 == use current display
	{
		displayId = SDL_GetDisplayForWindow( window );
		if( displayId == 0 )
		{
			displayId = SDL_GetPrimaryDisplay();
		}
	}

	if( parms.fullScreen != glConfig.isFullscreen )
	{
		// we have to switch to another display
		if( glConfig.isFullscreen )
		{
			// if we're already in fullscreen mode but want to switch to another monitor
			// we have to go to windowed mode first to move the window.. SDL-oddity.
			SDL_SetWindowFullscreen( window, false );
		}
		// select display ; SDL_WINDOWPOS_UNDEFINED_DISPLAY() doesn't work.
		int x = SDL_WINDOWPOS_CENTERED_DISPLAY( displayId );
		// move window to the center of selected display
		SDL_SetWindowPosition( window, x, x );
	}
	return displayId;
}

static bool SetScreenParmsFullscreen( glimpParms_t parms )
{
	SDL_DisplayID displayId = ScreenParmsHandleDisplayIndex( parms );
	if( displayId == 0 )
	{
		return false;
	}

	// desktop (borderless) fullscreen: passing NULL keeps the current desktop mode.
	// The requested resolution is applied by scaling the render target in GL, not by
	// switching the display to an exclusive video mode.
	SDL_SetWindowFullscreenMode( window, NULL );

	if( !SDL_SetWindowFullscreen( window, true ) )
	{
		common->Warning( "Couldn't switch to fullscreen mode, reason: %s!", SDL_GetError() );
		return false;
	}
	return true;
}

static bool SetScreenParmsWindowed( glimpParms_t parms )
{
	ClampWindowedParms( parms );
	SDL_SetWindowSize( window, parms.width, parms.height );
	SDL_SetWindowPosition( window, parms.x, parms.y );

	// if we're currently in fullscreen mode, we need to disable that
	if( SDL_GetWindowFlags( window ) & SDL_WINDOW_FULLSCREEN )
	{
		if( !SDL_SetWindowFullscreen( window, false ) )
		{
			common->Warning( "Couldn't switch to windowed mode, reason: %s!", SDL_GetError() );
			return false;
		}
	}
	return true;
}

/*
===================
GLimp_SetScreenParms
===================
*/
bool GLimp_SetScreenParms( glimpParms_t parms )
{
	if( parms.fullScreen > 0 || parms.fullScreen == -2 )
	{
		if( !SetScreenParmsFullscreen( parms ) )
		{
			return false;
		}
	}
	else if( parms.fullScreen == 0 ) // windowed mode
	{
		if( !SetScreenParmsWindowed( parms ) )
		{
			return false;
		}
	}
	else
	{
		common->Warning( "GLimp_SetScreenParms: fullScreen -1 (borderless window for multiple displays) currently unsupported!" );
		return false;
	}

	SDL_GL_SetAttribute( SDL_GL_STEREO, parms.stereo ? 1 : 0 );

	const int windowMultiSamples = ( parms.fullScreen != 0 ) ? 0 : parms.multiSamples;
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, windowMultiSamples ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, windowMultiSamples );

	glConfig.isFullscreen = parms.fullScreen;
	glConfig.isStereoPixelFormat = parms.stereo;
	glConfig.displayFrequency = parms.displayHz;
	glConfig.multisamples = windowMultiSamples;

	int drawableWidth = 0;
	int drawableHeight = 0;
	SDL_GetWindowSizeInPixels( window, &drawableWidth, &drawableHeight );

	if( parms.fullScreen != 0 )
	{
		if( GLimp_CreateLogicalScalingFBO( parms.width, parms.height, parms.multiSamples ) )
		{
			glConfig.nativeScreenWidth = parms.width;
			glConfig.nativeScreenHeight = parms.height;
			glConfig.multisamples = logicalScalingSamples;
		}
		else
		{
			glConfig.nativeScreenWidth = drawableWidth > 0 ? drawableWidth : parms.width;
			glConfig.nativeScreenHeight = drawableHeight > 0 ? drawableHeight : parms.height;
		}
	}
	else
	{
		GLimp_DestroyLogicalScalingFBO();
		glConfig.nativeScreenWidth = drawableWidth > 0 ? drawableWidth : parms.width;
		glConfig.nativeScreenHeight = drawableHeight > 0 ? drawableHeight : parms.height;
	}

	return true;
}

/*
===================
GLimp_Shutdown
===================
*/
void GLimp_Shutdown()
{
	common->Printf( "Shutting down OpenGL subsystem\n" );

	Sys_ShutdownJoystick();

	GLimp_DestroyLogicalScalingFBO();
	GLimp_DestroyWindowResolveFBO();

	if( context )
	{
		SDL_GL_DestroyContext( context );
		context = NULL;
	}

	if( window )
	{
		SDL_DestroyWindow( window );
		window = NULL;
	}
}

/*
===================
GLimp_SwapBuffers
===================
*/
void GLimp_SwapBuffers()
{
	if( !window )
	{
		return;
	}

	if( logicalScaling )
	{
		int drawableWidth = 0;
		int drawableHeight = 0;
		if( !SDL_GetWindowSizeInPixels( window, &drawableWidth, &drawableHeight ) || drawableWidth <= 0 || drawableHeight <= 0 )
		{
			SDL_GetWindowSize( window, &drawableWidth, &drawableHeight );
		}

		int viewportX, viewportY, viewportWidth, viewportHeight;
		GetLogicalScalingViewport( drawableWidth, drawableHeight, viewportX, viewportY, viewportWidth, viewportHeight );

		const GLboolean scissorEnabled = glIsEnabled( GL_SCISSOR_TEST );
		if( scissorEnabled )
		{
			glDisable( GL_SCISSOR_TEST );
		}

		GLboolean colorMask[4];
		GLfloat clearColor[4];
		glGetBooleanv( GL_COLOR_WRITEMASK, colorMask );
		glGetFloatv( GL_COLOR_CLEAR_VALUE, clearColor );

		// clear the window first so the letterbox/pillarbox bars stay black
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
		glDrawBuffer( GL_BACK );
		glReadBuffer( GL_BACK );
		glColorMask( GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE );
		glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );
		glClear( GL_COLOR_BUFFER_BIT );

		GLuint readFBO = logicalScalingFBO;
		if( logicalScalingSamples > 1 )
		{
			// resolve the multisampled target at its native size first, then scale the
			// single-sample result to the window (resolve blits cannot be scaled)
			glBindFramebuffer( GL_READ_FRAMEBUFFER, logicalScalingFBO );
			glReadBuffer( GL_COLOR_ATTACHMENT0 );
			glBindFramebuffer( GL_DRAW_FRAMEBUFFER, logicalScalingResolveFBO );
			glDrawBuffer( GL_COLOR_ATTACHMENT0 );
			glBlitFramebuffer( 0, 0, logicalScalingWidth, logicalScalingHeight,
							   0, 0, logicalScalingWidth, logicalScalingHeight,
							   GL_COLOR_BUFFER_BIT, GL_NEAREST );
			readFBO = logicalScalingResolveFBO;
		}

		glBindFramebuffer( GL_READ_FRAMEBUFFER, readFBO );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );
		glBindFramebuffer( GL_DRAW_FRAMEBUFFER, 0 );
		glDrawBuffer( GL_BACK );
		glBlitFramebuffer( 0, 0, logicalScalingWidth, logicalScalingHeight,
						   viewportX, viewportY, viewportX + viewportWidth, viewportY + viewportHeight,
						   GL_COLOR_BUFFER_BIT, GL_LINEAR );

		glBindFramebuffer( GL_FRAMEBUFFER, logicalScalingFBO );
		glDrawBuffer( GL_COLOR_ATTACHMENT0 );
		glReadBuffer( GL_COLOR_ATTACHMENT0 );

		glClearColor( clearColor[0], clearColor[1], clearColor[2], clearColor[3] );
		glColorMask( colorMask[0], colorMask[1], colorMask[2], colorMask[3] );

		if( scissorEnabled )
		{
			glEnable( GL_SCISSOR_TEST );
		}
	}

	SDL_GL_SwapWindow( window );
}

/*
=================
GLimp_SetGamma
=================
*/
void GLimp_SetGamma( unsigned short red[256], unsigned short green[256], unsigned short blue[256] )
{
	// SDL3 removed the hardware gamma ramp API, so gamma is applied in the shader path instead.
	if( !window )
	{
		common->Warning( "GLimp_SetGamma called without window" );
	}
}

/*
===================
GLimp_ExtensionPointer
===================
*/
GLExtension_t GLimp_ExtensionPointer( const char* name )
{
	assert( SDL_WasInit( SDL_INIT_VIDEO ) );

	return ( GLExtension_t )SDL_GL_GetProcAddress( name );
}

void GLimp_GrabInput( int flags )
{
	bool grab = flags & GRAB_ENABLE;

	if( grab && ( flags & GRAB_REENABLE ) )
	{
		grab = false;
	}

	if( flags & GRAB_SETSTATE )
	{
		grabbed = grab;
	}

	if( in_nograb.GetBool() )
	{
		grab = false;
	}

	if( !window )
	{
		common->Warning( "GLimp_GrabInput called without window" );
		return;
	}

	SDL_SetWindowRelativeMouseMode( window, ( flags & GRAB_ENABLE ) ? true : false );
	SDL_SetWindowMouseGrab( window, grab ? true : false );
}

/*
=================
GLimp_EnableLogging
=================
*/
void GLimp_EnableLogging( bool enable )
{
}

/*
=================
SMP render thread handoff - single threaded on Linux, so these are trivial
=================
*/
bool GLimp_SpawnRenderThread( void ( *function )() )
{
	return false;
}
void* GLimp_BackEndSleep()
{
	return NULL;
}
void GLimp_FrontEndSleep()
{
}
void GLimp_WakeBackEnd( void* data )
{
}
void GLimp_ActivateContext()
{
}
void GLimp_DeactivateContext()
{
}

/*
====================
DumpAllDisplayDevices
====================
*/
void DumpAllDisplayDevices()
{
	common->DPrintf( "TODO: DumpAllDisplayDevices\n" );
}

class idSort_VidMode : public idSort_Quick< vidMode_t, idSort_VidMode >
{
public:
	int Compare( const vidMode_t& a, const vidMode_t& b ) const
	{
		int wd = a.width - b.width;
		int hd = a.height - b.height;
		int fd = a.displayHz - b.displayHz;
		return ( hd != 0 ) ? hd : ( wd != 0 ) ? wd : fd;
	}
};

// vidMode_t is a plain aggregate, build one without needing a constructor
static vidMode_t MakeVidMode( int width, int height, int displayHz ) {
	vidMode_t mode;
	mode.width = width;
	mode.height = height;
	mode.displayHz = displayHz;
	return mode;
}

// RB: resolutions supported by XreaL
static void FillStaticVidModes( idList<vidMode_t>& modeList )
{
	modeList.AddUnique( MakeVidMode(  320,   240, 60 ) );
	modeList.AddUnique( MakeVidMode(  400,   300, 60 ) );
	modeList.AddUnique( MakeVidMode(  512,   384, 60 ) );
	modeList.AddUnique( MakeVidMode(  640,   480, 60 ) );
	modeList.AddUnique( MakeVidMode(  800,   600, 60 ) );
	modeList.AddUnique( MakeVidMode(  960,   720, 60 ) );
	modeList.AddUnique( MakeVidMode(  1024,  768, 60 ) );
	modeList.AddUnique( MakeVidMode(  1152,  864, 60 ) );
	modeList.AddUnique( MakeVidMode(  1280,  720, 60 ) );
	modeList.AddUnique( MakeVidMode(  1280,  768, 60 ) );
	modeList.AddUnique( MakeVidMode(  1280,  800, 60 ) );
	modeList.AddUnique( MakeVidMode(  1280, 1024, 60 ) );
	modeList.AddUnique( MakeVidMode(  1360,  768, 60 ) );
	modeList.AddUnique( MakeVidMode(  1440,  900, 60 ) );
	modeList.AddUnique( MakeVidMode(  1680, 1050, 60 ) );
	modeList.AddUnique( MakeVidMode(  1600, 1200, 60 ) );
	modeList.AddUnique( MakeVidMode(  1920, 1080, 60 ) );
	modeList.AddUnique( MakeVidMode(  1920, 1200, 60 ) );
	modeList.AddUnique( MakeVidMode(  2048, 1536, 60 ) );
	modeList.AddUnique( MakeVidMode(  2560, 1600, 60 ) );

	modeList.SortWithTemplate( idSort_VidMode() );
}

/*
====================
R_GetModeListForDisplay
====================
*/
bool R_GetModeListForDisplay( const int requestedDisplayNum, idList<vidMode_t>& modeList )
{
	assert( requestedDisplayNum >= 0 );

	// this may be called before GLimp_Init(), but SDL needs to be initialized to query the displays
	GLimp_PreInit();

	modeList.Clear();

	int numDisplays = 0;
	SDL_DisplayID* displays = SDL_GetDisplays( &numDisplays );

	if( requestedDisplayNum >= numDisplays || displays == NULL )
	{
		if( displays )
		{
			SDL_free( displays );
		}
		return false;
	}

	SDL_DisplayID displayId = displays[ requestedDisplayNum ];
	SDL_free( displays );

	int numModes = 0;
	SDL_DisplayMode** modes = SDL_GetFullscreenDisplayModes( displayId, &numModes );

	if( modes && numModes > 0 )
	{
		for( int i = 0; i < numModes; i++ )
		{
			vidMode_t mode;
			mode.width = modes[i]->w;
			mode.height = modes[i]->h;
			mode.displayHz = modes[i]->refresh_rate ? modes[i]->refresh_rate : 60;
			modeList.AddUnique( mode );
		}
		SDL_free( modes );

		if( modeList.Num() < 1 )
		{
			common->Warning( "Couldn't get a single video mode for display %i, using default ones..!\n", requestedDisplayNum );
			FillStaticVidModes( modeList );
		}

		// sort with lowest resolution first
		modeList.SortWithTemplate( idSort_VidMode() );
	}
	else
	{
		common->Warning( "Can't get display modes for display %i, using default modes...\n", requestedDisplayNum );
		FillStaticVidModes( modeList );
	}

	return true;
}
