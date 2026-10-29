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

#ifndef __X3DAUDIO_INCLUDED__
#define __X3DAUDIO_INCLUDED__

// FAudio ships F3DAudio, the same API under a different prefix
#include "F3DAudio.h"

#define X3DAUDIO_HANDLE			FAudio3DAudioHandle
#define X3DAudioInitialize		F3DAudioInitialize
#define X3DAudioCalculate		F3DAudioCalculate

#define X3DAUDIO_LISTENER		FAudio3DAudioListener
#define X3DAUDIO_EMITTER		FAudio3DAudioEmitter
#define X3DAUDIO_DSP_SETTINGS	FAudio3DAudioDSPSettings

#define X3DAUDIO_CALCULATE_MATRIX			F3DAUDIO_CALCULATE_MATRIX
#define X3DAUDIO_CALCULATE_DELAY			F3DAUDIO_CALCULATE_DELAY
#define X3DAUDIO_CALCULATE_LPF_DIRECT		F3DAUDIO_CALCULATE_LPF_DIRECT
#define X3DAUDIO_CALCULATE_LPF_REVERB		F3DAUDIO_CALCULATE_LPF_REVERB
#define X3DAUDIO_CALCULATE_REVERB			F3DAUDIO_CALCULATE_REVERB
#define X3DAUDIO_CALCULATE_DOPPLER			F3DAUDIO_CALCULATE_DOPPLER
#define X3DAUDIO_CALCULATE_EMITTER_ANGLE	F3DAUDIO_CALCULATE_EMITTER_ANGLE
#define X3DAUDIO_CALCULATE_ZEROCENTER		F3DAUDIO_CALCULATE_ZEROCENTER
#define X3DAUDIO_CALCULATE_REDIRECT_TO_LFE	F3DAUDIO_CALCULATE_REDIRECT_TO_LFE

#endif // __X3DAUDIO_INCLUDED__
