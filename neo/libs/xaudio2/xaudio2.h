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

#ifndef __XAUDIO2_INCLUDED__
#define __XAUDIO2_INCLUDED__

/*
================================================================================================

	XAudio2 API on top of FAudio.

	The engine's sound system is written against the XAudio2 C++ interfaces, which do not
	exist outside of Windows. FAudio is a reimplementation of the same API for other
	platforms, but exposes it as a plain C API, so this header provides the missing C++
	shell around it. Only the parts the engine actually uses are declared here.

================================================================================================
*/

#include <stdint.h>

#include "FAudio.h"

typedef uint32_t	HRESULT;
typedef uint32_t	UINT32;
typedef uint64_t	UINT64;
typedef uint8_t		BYTE;
typedef uint16_t	WORD;
typedef uint32_t	DWORD;
typedef int32_t		INT32;
typedef float		FLOAT32;
typedef uint32_t	XAUDIO2_PROCESSOR;
typedef void		THIS_TYPE;

#define STDMETHODCALLTYPE
#define THIS
#define THIS_
#define STDMETHOD( method )				virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_( type, method )		virtual type STDMETHODCALLTYPE method

#define S_OK							0
#define SUCCEEDED( hr )					( ( ( HRESULT )( hr ) & 0x80000000 ) == 0 )
#define FAILED( hr )					( ( ( HRESULT )( hr ) & 0x80000000 ) != 0 )
#define XAUDIO2_E_INVALID_CALL			FAUDIO_E_INVALID_CALL
#define XAUDIO2_E_DEVICE_INVALIDATED	FAUDIO_E_DEVICE_INVALIDATED

#define XAUDIO2_DEFAULT_PROCESSOR		FAUDIO_DEFAULT_PROCESSOR
#define XAUDIO2_ANY_PROCESSOR			FAUDIO_DEFAULT_PROCESSOR
#define XAUDIO2_DEFAULT_CHANNELS		FAUDIO_DEFAULT_CHANNELS
#define XAUDIO2_DEFAULT_SAMPLERATE		FAUDIO_DEFAULT_SAMPLERATE
#define XAUDIO2_END_OF_STREAM			FAUDIO_END_OF_STREAM
#define XAUDIO2_COMMIT_NOW				FAUDIO_COMMIT_NOW
#define XAUDIO2_COMMIT_ALL				FAUDIO_COMMIT_ALL
#define XAUDIO2_NO_LOOP_REGION			FAUDIO_NO_LOOP_REGION
#define XAUDIO2_LOOP_INFINITE			FAUDIO_LOOP_INFINITE
#define XAUDIO2_VOICE_USEFILTER			FAUDIO_VOICE_USEFILTER
#define XAUDIO2_DEBUG_ENGINE			FAUDIO_DEBUG_ENGINE
#define XAUDIO2_LOG_ERRORS				FAUDIO_LOG_ERRORS
#define XAUDIO2_LOG_WARNINGS			FAUDIO_LOG_WARNINGS
#define XAUDIO2_MAX_FILTER_FREQUENCY	FAUDIO_MAX_FILTER_FREQUENCY
#define XAUDIO2_MAX_FILTER_ONEOVERQ		FAUDIO_MAX_FILTER_ONEOVERQ
#define XAUDIO2_MAX_FREQ_RATIO			FAUDIO_MAX_FREQ_RATIO
#define XAUDIO2_MIN_FREQ_RATIO			FAUDIO_MIN_FREQ_RATIO
#define XAUDIO2_MAX_VOLUME_LEVEL		FAUDIO_MAX_VOLUME_LEVEL
#define XAUDIO2_MIN_SAMPLE_RATE			FAUDIO_MIN_SAMPLE_RATE
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MONO			600000.0f
#define XAUDIO2_MAX_RATIO_TIMES_RATE_XMA_MULTICHANNEL	300000.0f

// FAudio has the very same layout for all of these
typedef FAudioWaveFormatEx			WAVEFORMATEX;
typedef FAudioWaveFormatExtensible	WAVEFORMATEXTENSIBLE;
typedef FAudioBuffer				XAUDIO2_BUFFER;
typedef FAudioVoiceState			XAUDIO2_VOICE_STATE;
typedef FAudioVoiceDetails			XAUDIO2_VOICE_DETAILS;
typedef FAudioDeviceDetails			XAUDIO2_DEVICE_DETAILS;
typedef FAudioPerformanceData		XAUDIO2_PERFORMANCE_DATA;
typedef FAudioDebugConfiguration	XAUDIO2_DEBUG_CONFIGURATION;
typedef FAudioFilterParameters		XAUDIO2_FILTER_PARAMETERS;
typedef FAudioFilterType			XAUDIO2_FILTER_TYPE;
typedef FAudioDeviceRole			XAUDIO2_DEVICE_ROLE;

#define NotDefaultDevice				FAudioNotDefaultDevice
#define DefaultConsoleDevice			FAudioDefaultConsoleDevice
#define DefaultMultimediaDevice			FAudioDefaultMultimediaDevice
#define DefaultCommunicationsDevice		FAudioDefaultCommunicationsDevice
#define DefaultGameDevice				FAudioDefaultGameDevice
#define GlobalDefaultDevice				FAudioGlobalDefaultDevice
#define InvalidDeviceRole				FAudioInvalidDeviceRole
typedef FAudioSendDescriptor		XAUDIO2_SEND_DESCRIPTOR;
typedef FAudioVoiceSends			XAUDIO2_VOICE_SENDS;

#define LowPassFilter	FAudioLowPassFilter
#define BandPassFilter	FAudioBandPassFilter
#define HighPassFilter	FAudioHighPassFilter
#define NotchFilter		FAudioNotchFilter

// effects (and the volume meter built on top of them) are not implemented, the engine
// only uses them for cosmetic level meters
class IUnknown {
public:
	virtual ~IUnknown() {}
	virtual UINT32 Release() = 0;
};

typedef struct XAUDIO2_EFFECT_DESCRIPTOR {
	IUnknown *	pEffect;
	INT32		InitialState;
	UINT32		OutputChannels;
} XAUDIO2_EFFECT_DESCRIPTOR;

typedef struct XAUDIO2_EFFECT_CHAIN {
	UINT32						EffectCount;
	XAUDIO2_EFFECT_DESCRIPTOR *	pEffectDescriptors;
} XAUDIO2_EFFECT_CHAIN;

/*
================================================================================================

	Callbacks

================================================================================================
*/

class IXAudio2VoiceCallback {
public:
	virtual ~IXAudio2VoiceCallback() {}

	STDMETHOD_( void, OnVoiceProcessingPassStart )( UINT32 BytesRequired ) = 0;
	STDMETHOD_( void, OnVoiceProcessingPassEnd )() = 0;
	STDMETHOD_( void, OnStreamEnd )() = 0;
	STDMETHOD_( void, OnBufferStart )( void * pBufferContext ) = 0;
	STDMETHOD_( void, OnBufferEnd )( void * pBufferContext ) = 0;
	STDMETHOD_( void, OnLoopEnd )( void * pBufferContext ) = 0;
	STDMETHOD_( void, OnVoiceError )( void * pBufferContext, HRESULT Error ) = 0;
};

class IXAudio2EngineCallback {
public:
	virtual ~IXAudio2EngineCallback() {}

	STDMETHOD_( void, OnProcessingPassStart )() = 0;
	STDMETHOD_( void, OnProcessingPassEnd )() = 0;
	STDMETHOD_( void, OnCriticalError )( HRESULT Error ) = 0;
};

/*
================================================================================================

	Voices

================================================================================================
*/

class IXAudio2Voice {
public:
	virtual ~IXAudio2Voice() {}

	virtual HRESULT	SetVolume( float Volume, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual void	GetVolume( float * pVolume ) = 0;
	virtual HRESULT	SetOutputMatrix( IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels, UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	SetFilterParameters( const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	SetEffectChain( const XAUDIO2_EFFECT_CHAIN * pEffectChain ) = 0;
	virtual HRESULT	EnableEffect( UINT32 EffectIndex, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	DisableEffect( UINT32 EffectIndex, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	GetEffectParameters( UINT32 EffectIndex, void * pParameters, UINT32 ParameterByteSize ) = 0;
	virtual void	GetVoiceDetails( XAUDIO2_VOICE_DETAILS * pVoiceDetails ) = 0;

	// destroys the underlying voice and the wrapper
	virtual void	DestroyVoice() = 0;

	// shim internal: the FAudio voice behind this wrapper
	virtual FAudioVoice *	GetFAudioVoice() = 0;
};

class IXAudio2SourceVoice : public IXAudio2Voice {
public:
	virtual HRESULT	Start( UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	Stop( UINT32 Flags = 0, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual HRESULT	FlushSourceBuffers() = 0;
	virtual HRESULT	SubmitSourceBuffer( const XAUDIO2_BUFFER * pBuffer, const void * pBufferWMA = NULL ) = 0;
	virtual HRESULT	Discontinuity() = 0;
	virtual HRESULT	ExitLoop( UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual void	GetState( XAUDIO2_VOICE_STATE * pVoiceState, UINT32 Flags = 0 ) = 0;
	virtual HRESULT	SetFrequencyRatio( float Ratio, UINT32 OperationSet = XAUDIO2_COMMIT_NOW ) = 0;
	virtual void	GetFrequencyRatio( float * pRatio ) = 0;
	virtual HRESULT	SetSourceSampleRate( UINT32 NewSourceSampleRate ) = 0;
};

class IXAudio2MasteringVoice : public IXAudio2Voice {
public:
	virtual UINT32	GetChannelMask() = 0;
};

class IXAudio2SubmixVoice : public IXAudio2Voice {
public:
};

/*
================================================================================================

	Engine

================================================================================================
*/

class IXAudio2 {
public:
	virtual ~IXAudio2() {}

	virtual HRESULT	GetDeviceCount( UINT32 * pCount ) = 0;
	virtual HRESULT	GetDeviceDetails( UINT32 DeviceIndex, XAUDIO2_DEVICE_DETAILS * pDeviceDetails ) = 0;
	virtual HRESULT	CreateMasteringVoice( IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels = XAUDIO2_DEFAULT_CHANNELS, UINT32 InputSampleRate = XAUDIO2_DEFAULT_SAMPLERATE, UINT32 Flags = 0, UINT32 DeviceIndex = 0, const XAUDIO2_EFFECT_CHAIN * pEffectChain = NULL ) = 0;
	virtual HRESULT	CreateSourceVoice( IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat, UINT32 Flags = 0, float MaxFrequencyRatio = XAUDIO2_MAX_FREQ_RATIO, IXAudio2VoiceCallback * pCallback = NULL, const XAUDIO2_VOICE_SENDS * pSendList = NULL, const XAUDIO2_EFFECT_CHAIN * pEffectChain = NULL ) = 0;
	virtual HRESULT	CreateSubmixVoice( IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate, UINT32 Flags = 0, UINT32 ProcessingStage = 0, const XAUDIO2_VOICE_SENDS * pSendList = NULL, const XAUDIO2_EFFECT_CHAIN * pEffectChain = NULL ) = 0;
	virtual HRESULT	StartEngine() = 0;
	virtual void	StopEngine() = 0;
	virtual HRESULT	CommitChanges( UINT32 OperationSet ) = 0;
	virtual void	GetPerformanceData( XAUDIO2_PERFORMANCE_DATA * pPerfData ) = 0;
	virtual void	SetDebugConfiguration( const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void * pReserved = NULL ) = 0;
	virtual HRESULT	RegisterForCallbacks( IXAudio2EngineCallback * pCallback ) = 0;
	virtual void	UnregisterForCallbacks( IXAudio2EngineCallback * pCallback ) = 0;

	virtual UINT32	Release() = 0;
};

HRESULT XAudio2Create( IXAudio2 ** ppXAudio2, UINT32 Flags = 0, XAUDIO2_PROCESSOR XAudio2Processor = XAUDIO2_DEFAULT_PROCESSOR );

#endif // __XAUDIO2_INCLUDED__
