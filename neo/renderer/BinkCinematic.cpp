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

#pragma hdrstop
#include "../idlib/precompiled.h"

#include "tr_local.h"
#include "Cinematic.h"
#include "BinkCinematic.h"

// the standalone Bink decoder in libs/bink
#include "bink_api.h"

#include <xaudio2.h>

/*
===============================================================================

	idBinkCinematic

	Decodes .bik movies on demand.  The renderer binds the three returned images
	as the Y, Cr and Cb planes and the bink render programs convert them to RGB.

===============================================================================
*/

class idBinkCinematic : public idCinematic {
public:
							idBinkCinematic();
	virtual					~idBinkCinematic();

	virtual bool			InitFromFile( const char *qpath, bool looping );
	virtual int				AnimationLength();
	virtual cinData_t		ImageForTime( int milliseconds );
	virtual void			Close();
	virtual void			ResetTime( int time );
	virtual int				GetStartTime();
	virtual float			GetFrameRate() const;

private:
	void					EnsureImages();
	void					UploadFrame( const BinkFrameResult &frame );

	bool					StartAudio();
	void					SubmitAudio( const BinkFrameResult &frame );
	void					StopAudio();

	BinkFile *				file;
	bool					loop;

	int						width;
	int						height;
	int						fpsNum;
	int						fpsDen;
	int						numFrames;

	int64					shownFrame;			// frame currently in the images, -1 if none
	int						startTime;
	int						status;

	idImage *				imageY;
	idImage *				imageCr;
	idImage *				imageCb;

	int						imageID;

	int						audioTrack;
	int						audioChannels;
	int						audioSampleRate;
	IXAudio2SourceVoice *	audioVoice;
	idList< int16_t * >		audioBuffers;
	bool					audioStarted;
};

/*
==============
idBinkCinematic::idBinkCinematic
==============
*/
idBinkCinematic::idBinkCinematic() {
	file = NULL;
	loop = false;

	width = 0;
	height = 0;
	fpsNum = 0;
	fpsDen = 1;
	numFrames = 0;

	shownFrame = -1;
	startTime = -1;
	status = FMV_IDLE;

	static int nextImageID = 0;
	imageID = nextImageID++;

	imageY = NULL;
	imageCr = NULL;
	imageCb = NULL;

	audioTrack = -1;
	audioChannels = 0;
	audioSampleRate = 0;
	audioVoice = NULL;
	audioStarted = false;
}

/*
==============
idBinkCinematic::~idBinkCinematic
==============
*/
idBinkCinematic::~idBinkCinematic() {
	Close();
}

/*
==============
idBinkCinematic::InitFromFile
==============
*/
bool idBinkCinematic::InitFromFile( const char *qpath, bool looping ) {
	Close();

	if ( qpath == NULL || qpath[0] == '\0' ) {
		return false;
	}

	loop = looping;
	status = FMV_PLAY;

	idStr path = qpath;
	path.BackSlashesToSlashes();
	while ( path.Length() > 0 && path[ 0 ] == '/' ) {
		path.StripLeading( '/' );
	}

	// the .bik files are never inside the resource files, so a plain file read is all we need
	void *	buffer = NULL;
	int		length = fileSystem->ReadFile( path.c_str(), &buffer );

	if ( length <= 0 ) {
		// the retail release re-encoded the old RoQ movies as Bink files, but the
		// materials and scripts still refer to them by their .RoQ names
		idStr binkPath = path;
		binkPath.SetFileExtension( "bik" );
		if ( binkPath.Icmp( path ) != 0 ) {
			length = fileSystem->ReadFile( binkPath.c_str(), &buffer );
			if ( length > 0 ) {
				path = binkPath;
			}
		}
	}

	if ( length <= 0 || buffer == NULL ) {
		common->Warning( "idBinkCinematic: couldn't read '%s'", qpath );
		return false;
	}

	const int err = bink_open_memory( &file, ( const uint8_t * )buffer, length );
	Mem_Free( buffer );

	if ( err < 0 || file == NULL ) {
		common->Warning( "idBinkCinematic: '%s' is not a valid Bink file (%i)", qpath, err );
		file = NULL;
		return false;
	}

	const BinkStreamInfo * videoStream = bink_stream( file, 0 );
	if ( videoStream == NULL || videoStream->type != BINK_STREAM_VIDEO || !bink_has_video_decoder( file ) ) {
		common->Warning( "idBinkCinematic: '%s' has no video stream", qpath );
		bink_close( &file );
		return false;
	}

	width = videoStream->width;
	height = videoStream->height;
	fpsNum = ( videoStream->fps_num > 0 ) ? videoStream->fps_num : 30;
	fpsDen = ( videoStream->fps_den > 0 ) ? videoStream->fps_den : 1;
	numFrames = videoStream->nb_frames;

	audioTrack = -1;
	for ( int i = 1; i < bink_stream_count( file ); i++ ) {
		const BinkStreamInfo * audioStream = bink_stream( file, i );
		if ( audioStream != NULL && audioStream->type == BINK_STREAM_AUDIO && audioStream->channels > 0 && audioStream->sample_rate > 0 ) {
			audioTrack = i - 1;
			audioChannels = audioStream->channels;
			audioSampleRate = audioStream->sample_rate;
			break;
		}
	}

	shownFrame = -1;
	startTime = -1;

	common->Printf( "Playing Bink video '%s' (%ix%i, %i frames, %.2f fps)\n", path.c_str(), width, height, numFrames, GetFrameRate() );

	return true;
}

/*
==============
idBinkCinematic::AnimationLength
==============
*/
int idBinkCinematic::AnimationLength() {
	if ( numFrames <= 0 ) {
		return 0;
	}
	return ( int )( ( int64 )numFrames * 1000LL * ( int64 )fpsDen / ( int64 )fpsNum );
}

/*
==============
idBinkCinematic::GetFrameRate
==============
*/
float idBinkCinematic::GetFrameRate() const {
	if ( fpsNum <= 0 || fpsDen <= 0 ) {
		return 30.0f;
	}
	return ( float )fpsNum / ( float )fpsDen;
}

/*
==============
idBinkCinematic::GetStartTime
==============
*/
int idBinkCinematic::GetStartTime() {
	return startTime;
}

/*
==============
idBinkCinematic::ResetTime

Restarts the movie at the given time
==============
*/
void idBinkCinematic::ResetTime( int time ) {
	StopAudio();

	startTime = time;
	shownFrame = -1;
	status = FMV_PLAY;

	if ( file != NULL ) {
		bink_seek( file, 0 );
	}
}

/*
==============
idBinkCinematic::StartAudio

Creates the XAudio2 voice used to play the movie's first audio track.
==============
*/
bool idBinkCinematic::StartAudio() {
	if ( audioVoice != NULL ) {
		return true;
	}
	if ( audioTrack < 0 || audioChannels <= 0 || audioSampleRate <= 0 || soundSystem == NULL ) {
		return false;
	}

	IXAudio2 * xaudio = ( IXAudio2 * )soundSystem->GetIXAudio2();
	if ( xaudio == NULL ) {
		return false;
	}

	WAVEFORMATEX format;
	memset( &format, 0, sizeof( format ) );
	format.wFormatTag = FAUDIO_FORMAT_PCM;
	format.nChannels = audioChannels;
	format.nSamplesPerSec = audioSampleRate;
	format.wBitsPerSample = 16;
	format.nBlockAlign = audioChannels * sizeof( int16_t );
	format.nAvgBytesPerSec = audioSampleRate * format.nBlockAlign;
	format.cbSize = 0;

	if ( FAILED( xaudio->CreateSourceVoice( &audioVoice, &format ) ) || audioVoice == NULL ) {
		common->Warning( "idBinkCinematic: couldn't create audio voice for %i Hz, %i channel(s)", audioSampleRate, audioChannels );
		audioVoice = NULL;
		audioTrack = -1;
		return false;
	}

	audioStarted = false;
	return true;
}

/*
==============
idBinkCinematic::SubmitAudio

Converts the decoded float PCM to 16 bit and queues it on the audio voice.
==============
*/
void idBinkCinematic::SubmitAudio( const BinkFrameResult &frame ) {
	if ( audioTrack < 0 || frame.nb_audio <= audioTrack ) {
		return;
	}
	if ( frame.audio[ audioTrack ] == NULL || frame.audio_nb_samples[ audioTrack ] <= 0 ) {
		return;
	}

	const int channels = frame.audio_channels[ audioTrack ];
	const int samples = frame.audio_nb_samples[ audioTrack ];
	if ( channels <= 0 || samples <= 0 ) {
		return;
	}
	if ( !StartAudio() ) {
		return;
	}

	const int numSamples = samples * channels;
	int16_t * pcm = ( int16_t * )malloc( ( size_t )numSamples * sizeof( int16_t ) );
	if ( pcm == NULL ) {
		return;
	}

	for ( int i = 0; i < numSamples; i++ ) {
		float sample = frame.audio[ audioTrack ][ i ];
		if ( sample < -1.0f ) {
			sample = -1.0f;
		} else if ( sample > 1.0f ) {
			sample = 1.0f;
		}
		pcm[ i ] = ( int16_t )( sample * 32767.0f );
	}

	XAUDIO2_BUFFER buffer;
	memset( &buffer, 0, sizeof( buffer ) );
	buffer.AudioBytes = numSamples * sizeof( int16_t );
	buffer.pAudioData = ( const uint8_t * )pcm;

	if ( FAILED( audioVoice->SubmitSourceBuffer( &buffer ) ) ) {
		free( pcm );
		common->Warning( "idBinkCinematic: couldn't queue audio" );
		StopAudio();
		audioTrack = -1;
		return;
	}
	audioBuffers.Append( pcm );

	if ( !audioStarted ) {
		if ( FAILED( audioVoice->Start() ) ) {
			StopAudio();
			audioTrack = -1;
			return;
		}
		audioStarted = true;
	}
}

/*
==============
idBinkCinematic::StopAudio
==============
*/
void idBinkCinematic::StopAudio() {
	if ( audioVoice != NULL ) {
		audioVoice->Stop();
		audioVoice->FlushSourceBuffers();
		audioVoice->DestroyVoice();
		audioVoice = NULL;
	}

	for ( int i = 0; i < audioBuffers.Num(); i++ ) {
		free( audioBuffers[ i ] );
	}
	audioBuffers.Clear();
	audioStarted = false;
}

/*
==============
idBinkCinematic::EnsureImages

Creates the Y/Cr/Cb textures on demand, and resizes them if the movie changed
==============
*/
void idBinkCinematic::EnsureImages() {
	const int chromaWidth = ( width + 1 ) / 2;
	const int chromaHeight = ( height + 1 ) / 2;

	// Each cinematic needs its own textures: several videos (for example the
	// shell background and a PDA clip) can be on screen at the same time, and
	// resizing shared images while another one is being drawn makes it flash
	// black or green.
	if ( imageY == NULL ) {
		idStr name = va( "_binkY_%i", imageID );
		imageY = globalImages->GetImage( name );
		if ( imageY == NULL ) {
			imageY = globalImages->AllocImage( name );
			imageY->SetReferencedOutsideLevelLoad();
		}
	}
	if ( imageCr == NULL ) {
		idStr name = va( "_binkCr_%i", imageID );
		imageCr = globalImages->GetImage( name );
		if ( imageCr == NULL ) {
			imageCr = globalImages->AllocImage( name );
			imageCr->SetReferencedOutsideLevelLoad();
		}
	}
	if ( imageCb == NULL ) {
		idStr name = va( "_binkCb_%i", imageID );
		imageCb = globalImages->GetImage( name );
		if ( imageCb == NULL ) {
			imageCb = globalImages->AllocImage( name );
			imageCb->SetReferencedOutsideLevelLoad();
		}
	}

	idImageOpts opts;
	opts.numLevels = 1;
	opts.format = FMT_LUM8;

	if ( !imageY->IsLoaded() || imageY->GetUploadWidth() != width || imageY->GetUploadHeight() != height ) {
		opts.width = width;
		opts.height = height;
		imageY->AllocImage( opts, TF_LINEAR, TR_CLAMP );
	}

	if ( !imageCr->IsLoaded() || imageCr->GetUploadWidth() != chromaWidth || imageCr->GetUploadHeight() != chromaHeight ) {
		opts.width = chromaWidth;
		opts.height = chromaHeight;
		imageCr->AllocImage( opts, TF_LINEAR, TR_CLAMP );
		imageCb->AllocImage( opts, TF_LINEAR, TR_CLAMP );
	}
}

/*
==============
idBinkCinematic::UploadFrame
==============
*/
void idBinkCinematic::UploadFrame( const BinkFrameResult &frame ) {
	if ( frame.planes[ 0 ] == NULL ) {
		return;
	}

	EnsureImages();

	// the decoder pads the rows of the planes, so the row length has to be passed in
	imageY->SubImageUpload( 0, 0, 0, 0, width, height, frame.planes[ 0 ], frame.strides[ 0 ] );

	// the decoder stores chroma as U, V (plane 1, plane 2), while the bink render
	// programs label their samplers Cr, Cb, so the two planes go in swapped
	if ( frame.planes[ 1 ] != NULL && frame.planes[ 2 ] != NULL ) {
		const int chromaWidth = ( width + 1 ) / 2;
		const int chromaHeight = ( height + 1 ) / 2;
		imageCr->SubImageUpload( 0, 0, 0, 0, chromaWidth, chromaHeight, frame.planes[ 2 ], frame.strides[ 2 ] );
		imageCb->SubImageUpload( 0, 0, 0, 0, chromaWidth, chromaHeight, frame.planes[ 1 ], frame.strides[ 1 ] );
	}
}

/*
==============
idBinkCinematic::ImageForTime

Returns the images holding the frame for the given time.  Bink frames depend on
the frames before them, so decoding always moves forward and a rewind seeks back
to the closest key frame.
==============
*/
cinData_t idBinkCinematic::ImageForTime( int milliseconds ) {
	cinData_t	cin;
	memset( &cin, 0, sizeof( cin ) );
	cin.status = FMV_IDLE;

	if ( file == NULL ) {
		return cin;
	}

	// The 2D render path used by RenderBink leaves renderView.time[0] at zero,
	// so drive playback from the real clock when no time is supplied.
	if ( milliseconds <= 0 ) {
		milliseconds = Sys_Milliseconds();
	}

	if ( startTime < 0 ) {
		startTime = milliseconds;
	}

	int64 target = ( int64 )( milliseconds - startTime ) * ( int64 )fpsNum / ( 1000LL * ( int64 )fpsDen );
	if ( target < 0 ) {
		target = 0;
	}
	if ( numFrames > 0 ) {
		if ( target >= numFrames ) {
			if ( loop ) {
				target %= numFrames;
			} else {
				target = numFrames - 1;
				status = FMV_EOF;
			}
		}
	} else {
		target = 0;
	}

	if ( shownFrame != target ) {
		if ( shownFrame > target ) {
			StopAudio();
			if ( bink_seek( file, target ) < 0 ) {
				common->Warning( "idBinkCinematic: couldn't seek to frame %i", ( int )target );
				return cin;
			}
			shownFrame = -1;
		}

		BinkFrameResult frame;
		bool haveFrame = false;

		while ( shownFrame < target ) {
			const int ret = bink_decode_frame( file, &frame );
			if ( ret <= 0 ) {
				if ( ret < 0 ) {
					common->Warning( "idBinkCinematic: error %i while decoding frame %i", ret, ( int )target );
				}
				status = FMV_EOF;
				break;
			}
			shownFrame = frame.pts;
			SubmitAudio( frame );
			haveFrame = true;
		}

		if ( haveFrame ) {
			UploadFrame( frame );
		}
	}

	EnsureImages();

	cin.imageWidth = width;
	cin.imageHeight = height;
	cin.status = status;
	cin.imageY = imageY;
	cin.imageCr = imageCr;
	cin.imageCb = imageCb;

	return cin;
}

/*
==============
idBinkCinematic::Close
==============
*/
void idBinkCinematic::Close() {
	StopAudio();

	if ( file != NULL ) {
		bink_close( &file );
		file = NULL;
	}

	width = 0;
	height = 0;
	numFrames = 0;
	shownFrame = -1;
	status = FMV_IDLE;

	audioTrack = -1;
	audioChannels = 0;
	audioSampleRate = 0;
}

/*
==============
R_AllocBinkCinematic
==============
*/
idCinematic * R_AllocBinkCinematic() {
	return new idBinkCinematic;
}
