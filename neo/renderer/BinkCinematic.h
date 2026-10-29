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

#ifndef __BINK_CINEMATIC_H__
#define __BINK_CINEMATIC_H__

/*
===============================================================================

	Bink video playback.

	The retail game played the .bik videos with the proprietary RAD Bink SDK, which
	was not part of the GPL source release, so idCinematic::Alloc() only returned a
	stub and every videoMap material stayed black.  The standalone Bink decoder in
	libs/bink is used instead: frames are decoded on demand and uploaded to three
	luminance images (Y, Cr, Cb), which is exactly what the bink render programs
	sample.

===============================================================================
*/

class idCinematic;

// allocates the Bink implementation of idCinematic, see BinkCinematic.cpp
idCinematic *	R_AllocBinkCinematic();

#endif // __BINK_CINEMATIC_H__
