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

// Implements the XAudio2 C++ interfaces declared in xaudio2.h on top of FAudio.

#include "xaudio2.h"
#include "xaudio2fx.h"

/*
================================================================================================

	Callback bridges

	FAudio uses plain C function pointers, the engine derives from the C++ interfaces.

================================================================================================
*/

static void FAUDIOCALL BridgeVoiceProcessingPassStart( FAudioVoiceCallback * cb, uint32_t bytesRequired );
static void FAUDIOCALL BridgeVoiceProcessingPassEnd( FAudioVoiceCallback * cb );
static void FAUDIOCALL BridgeStreamEnd( FAudioVoiceCallback * cb );
static void FAUDIOCALL BridgeBufferStart( FAudioVoiceCallback * cb, void * pBufferContext );
static void FAUDIOCALL BridgeBufferEnd( FAudioVoiceCallback * cb, void * pBufferContext );
static void FAUDIOCALL BridgeLoopEnd( FAudioVoiceCallback * cb, void * pBufferContext );
static void FAUDIOCALL BridgeVoiceError( FAudioVoiceCallback * cb, void * pBufferContext, uint32_t error );

struct voiceCallbackBridge_t {
	// must be the first member, FAudio hands us back a pointer to it
	FAudioVoiceCallback		callback;
	IXAudio2VoiceCallback *	target;
};

static void FAUDIOCALL BridgeVoiceProcessingPassStart( FAudioVoiceCallback * cb, uint32_t bytesRequired ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnVoiceProcessingPassStart( bytesRequired );
}

static void FAUDIOCALL BridgeVoiceProcessingPassEnd( FAudioVoiceCallback * cb ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnVoiceProcessingPassEnd();
}

static void FAUDIOCALL BridgeStreamEnd( FAudioVoiceCallback * cb ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnStreamEnd();
}

static void FAUDIOCALL BridgeBufferStart( FAudioVoiceCallback * cb, void * pBufferContext ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnBufferStart( pBufferContext );
}

static void FAUDIOCALL BridgeBufferEnd( FAudioVoiceCallback * cb, void * pBufferContext ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnBufferEnd( pBufferContext );
}

static void FAUDIOCALL BridgeLoopEnd( FAudioVoiceCallback * cb, void * pBufferContext ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnLoopEnd( pBufferContext );
}

static void FAUDIOCALL BridgeVoiceError( FAudioVoiceCallback * cb, void * pBufferContext, uint32_t error ) {
	reinterpret_cast< voiceCallbackBridge_t * >( cb )->target->OnVoiceError( pBufferContext, error );
}

static void InitVoiceCallbackBridge( voiceCallbackBridge_t & bridge, IXAudio2VoiceCallback * target ) {
	bridge.callback.OnBufferEnd = BridgeBufferEnd;
	bridge.callback.OnBufferStart = BridgeBufferStart;
	bridge.callback.OnLoopEnd = BridgeLoopEnd;
	bridge.callback.OnStreamEnd = BridgeStreamEnd;
	bridge.callback.OnVoiceError = BridgeVoiceError;
	bridge.callback.OnVoiceProcessingPassEnd = BridgeVoiceProcessingPassEnd;
	bridge.callback.OnVoiceProcessingPassStart = BridgeVoiceProcessingPassStart;
	bridge.target = target;
}

static void FAUDIOCALL BridgeCriticalError( FAudioEngineCallback * cb, uint32_t error );
static void FAUDIOCALL BridgeProcessingPassEnd( FAudioEngineCallback * cb );
static void FAUDIOCALL BridgeProcessingPassStart( FAudioEngineCallback * cb );

struct engineCallbackBridge_t {
	// must be the first member, FAudio hands us back a pointer to it
	FAudioEngineCallback	callback;
	IXAudio2EngineCallback * target;
};

static void FAUDIOCALL BridgeCriticalError( FAudioEngineCallback * cb, uint32_t error ) {
	reinterpret_cast< engineCallbackBridge_t * >( cb )->target->OnCriticalError( error );
}

static void FAUDIOCALL BridgeProcessingPassEnd( FAudioEngineCallback * cb ) {
	reinterpret_cast< engineCallbackBridge_t * >( cb )->target->OnProcessingPassEnd();
}

static void FAUDIOCALL BridgeProcessingPassStart( FAudioEngineCallback * cb ) {
	reinterpret_cast< engineCallbackBridge_t * >( cb )->target->OnProcessingPassStart();
}

static void InitEngineCallbackBridge( engineCallbackBridge_t & bridge, IXAudio2EngineCallback * target ) {
	bridge.callback.OnCriticalError = BridgeCriticalError;
	bridge.callback.OnProcessingPassEnd = BridgeProcessingPassEnd;
	bridge.callback.OnProcessingPassStart = BridgeProcessingPassStart;
	bridge.target = target;
}

/*
================================================================================================

	Voice implementations

	FAudio shares a single voice structure between the source, submix and mastering voices, so
	the common part of the interface can be implemented once for all of them.

================================================================================================
*/

#define XAUDIO2_VOICE_COMMON( VOICE )																				\
	virtual HRESULT SetVolume( float Volume, UINT32 OperationSet ) override {										\
		return FAudioVoice_SetVolume( ( FAudioVoice * )VOICE, Volume, OperationSet );								\
	}																												\
	virtual void GetVolume( float * pVolume ) override {															\
		FAudioVoice_GetVolume( ( FAudioVoice * )VOICE, pVolume );													\
	}																												\
	virtual HRESULT SetOutputMatrix( IXAudio2Voice * pDestinationVoice, UINT32 SourceChannels,						\
			UINT32 DestinationChannels, const float * pLevelMatrix, UINT32 OperationSet ) override {				\
		return FAudioVoice_SetOutputMatrix( ( FAudioVoice * )VOICE,													\
			pDestinationVoice != NULL ? ( FAudioVoice * )pDestinationVoice->GetFAudioVoice() : NULL,				\
			SourceChannels, DestinationChannels, pLevelMatrix, OperationSet );										\
	}																												\
	virtual HRESULT SetFilterParameters( const XAUDIO2_FILTER_PARAMETERS * pParameters, UINT32 OperationSet ) override {	\
		return FAudioVoice_SetFilterParameters( ( FAudioVoice * )VOICE, ( const FAudioFilterParameters * )pParameters, OperationSet );	\
	}																												\
	virtual HRESULT SetEffectChain( const XAUDIO2_EFFECT_CHAIN * ) override {										\
		/* effects (the level meters) are not implemented */														\
		return S_OK;																								\
	}																												\
	virtual HRESULT EnableEffect( UINT32, UINT32 ) override { return S_OK; }										\
	virtual HRESULT DisableEffect( UINT32, UINT32 ) override { return S_OK; }										\
	virtual HRESULT GetEffectParameters( UINT32, void *, UINT32 ) override { return XAUDIO2_E_INVALID_CALL; }		\
	virtual void GetVoiceDetails( XAUDIO2_VOICE_DETAILS * pVoiceDetails ) override {								\
		FAudioVoice_GetVoiceDetails( ( FAudioVoice * )VOICE, pVoiceDetails );										\
	}																												\
	virtual void DestroyVoice() override {																			\
		FAudioVoice_DestroyVoice( ( FAudioVoice * )VOICE );															\
		VOICE = NULL;																								\
		delete this;																								\
	}

/*
========================
XAudio2SourceVoiceImpl
========================
*/
class XAudio2SourceVoiceImpl : public IXAudio2SourceVoice {
public:
	XAudio2SourceVoiceImpl( FAudioSourceVoice * voice_, IXAudio2VoiceCallback * target_ )
		: voice( voice_ ), hasCallback( target_ != NULL ) {
		if ( hasCallback ) {
			InitVoiceCallbackBridge( bridge, target_ );
		}
	}

	// the bridge has to be ready before FAudio creates the voice
	FAudioVoiceCallback * GetCallbackBridge() {
		return hasCallback ? &bridge.callback : NULL;
	}

	void SetVoice( FAudioSourceVoice * voice_ ) {
		voice = voice_;
	}

	XAUDIO2_VOICE_COMMON( voice )

	virtual FAudioVoice * GetFAudioVoice() override { return ( FAudioVoice * )voice; }

	virtual HRESULT Start( UINT32 Flags, UINT32 OperationSet ) override {
		return FAudioSourceVoice_Start( voice, Flags, OperationSet );
	}

	virtual HRESULT Stop( UINT32 Flags, UINT32 OperationSet ) override {
		return FAudioSourceVoice_Stop( voice, Flags, OperationSet );
	}

	virtual HRESULT FlushSourceBuffers() override {
		return FAudioSourceVoice_FlushSourceBuffers( voice );
	}

	virtual HRESULT SubmitSourceBuffer( const XAUDIO2_BUFFER * pBuffer, const void * pBufferWMA ) override {
		return FAudioSourceVoice_SubmitSourceBuffer( voice, ( const FAudioBuffer * )pBuffer, ( const FAudioBufferWMA * )pBufferWMA );
	}

	virtual HRESULT Discontinuity() override {
		return FAudioSourceVoice_Discontinuity( voice );
	}

	virtual HRESULT ExitLoop( UINT32 OperationSet ) override {
		return FAudioSourceVoice_ExitLoop( voice, OperationSet );
	}

	virtual void GetState( XAUDIO2_VOICE_STATE * pVoiceState, UINT32 Flags ) override {
		FAudioSourceVoice_GetState( voice, ( FAudioVoiceState * )pVoiceState, Flags );
	}

	virtual HRESULT SetFrequencyRatio( float Ratio, UINT32 OperationSet ) override {
		return FAudioSourceVoice_SetFrequencyRatio( voice, Ratio, OperationSet );
	}

	virtual void GetFrequencyRatio( float * pRatio ) override {
		FAudioSourceVoice_GetFrequencyRatio( voice, pRatio );
	}

	virtual HRESULT SetSourceSampleRate( UINT32 NewSourceSampleRate ) override {
		return FAudioSourceVoice_SetSourceSampleRate( voice, NewSourceSampleRate );
	}

private:
	FAudioSourceVoice *		voice;
	bool					hasCallback;
	voiceCallbackBridge_t	bridge;
};

/*
========================
XAudio2MasteringVoiceImpl
========================
*/
class XAudio2MasteringVoiceImpl : public IXAudio2MasteringVoice {
public:
	XAudio2MasteringVoiceImpl( FAudioMasteringVoice * voice_ ) : voice( voice_ ) {}

	XAUDIO2_VOICE_COMMON( voice )

	virtual FAudioVoice * GetFAudioVoice() override { return ( FAudioVoice * )voice; }
	virtual UINT32 GetChannelMask() override { return 0; }

private:
	FAudioMasteringVoice *	voice;
};

/*
========================
XAudio2SubmixVoiceImpl
========================
*/
class XAudio2SubmixVoiceImpl : public IXAudio2SubmixVoice {
public:
	XAudio2SubmixVoiceImpl( FAudioSubmixVoice * voice_ ) : voice( voice_ ) {}

	XAUDIO2_VOICE_COMMON( voice )

	virtual FAudioVoice * GetFAudioVoice() override { return ( FAudioVoice * )voice; }

private:
	FAudioSubmixVoice *	voice;
};

/*
================================================================================================

	XAudio2Impl

================================================================================================
*/

class XAudio2Impl : public IXAudio2 {
public:
	XAudio2Impl() : audio( NULL ), registeredCallback( NULL ) {}

	virtual HRESULT GetDeviceCount( UINT32 * pCount ) override {
		return FAudio_GetDeviceCount( audio, pCount );
	}

	virtual HRESULT GetDeviceDetails( UINT32 DeviceIndex, XAUDIO2_DEVICE_DETAILS * pDeviceDetails ) override {
		return FAudio_GetDeviceDetails( audio, DeviceIndex, ( FAudioDeviceDetails * )pDeviceDetails );
	}

	virtual HRESULT CreateMasteringVoice( IXAudio2MasteringVoice ** ppMasteringVoice, UINT32 InputChannels,
			UINT32 InputSampleRate, UINT32 Flags, UINT32 DeviceIndex, const XAUDIO2_EFFECT_CHAIN * pEffectChain ) override {
		FAudioMasteringVoice * voice = NULL;
		const HRESULT hr = FAudio_CreateMasteringVoice( audio, &voice, InputChannels, InputSampleRate, Flags, DeviceIndex, NULL );
		if ( FAILED( hr ) ) {
			return hr;
		}
		*ppMasteringVoice = new XAudio2MasteringVoiceImpl( voice );
		return S_OK;
	}

	virtual HRESULT CreateSourceVoice( IXAudio2SourceVoice ** ppSourceVoice, const WAVEFORMATEX * pSourceFormat,
			UINT32 Flags, float MaxFrequencyRatio, IXAudio2VoiceCallback * pCallback,
			const XAUDIO2_VOICE_SENDS * pSendList, const XAUDIO2_EFFECT_CHAIN * pEffectChain ) override {
		// FAudio keeps the callback pointer, so the bridge owned by the wrapper is used
		XAudio2SourceVoiceImpl * wrapper = new XAudio2SourceVoiceImpl( NULL, pCallback );
		FAudioSourceVoice * voice = NULL;
		const HRESULT hr = FAudio_CreateSourceVoice( audio, &voice, ( const FAudioWaveFormatEx * )pSourceFormat,
			Flags, MaxFrequencyRatio, pCallback != NULL ? wrapper->GetCallbackBridge() : NULL,
			( const FAudioVoiceSends * )pSendList, NULL );
		if ( FAILED( hr ) ) {
			delete wrapper;
			return hr;
		}

		wrapper->SetVoice( voice );
		*ppSourceVoice = wrapper;
		return S_OK;
	}

	virtual HRESULT CreateSubmixVoice( IXAudio2SubmixVoice ** ppSubmixVoice, UINT32 InputChannels, UINT32 InputSampleRate,
			UINT32 Flags, UINT32 ProcessingStage, const XAUDIO2_VOICE_SENDS * pSendList, const XAUDIO2_EFFECT_CHAIN * pEffectChain ) override {
		FAudioSubmixVoice * voice = NULL;
		const HRESULT hr = FAudio_CreateSubmixVoice( audio, &voice, InputChannels, InputSampleRate, Flags, ProcessingStage, NULL, NULL );
		if ( FAILED( hr ) ) {
			return hr;
		}
		*ppSubmixVoice = new XAudio2SubmixVoiceImpl( voice );
		return S_OK;
	}

	virtual HRESULT StartEngine() override {
		return FAudio_StartEngine( audio );
	}

	virtual void StopEngine() override {
		FAudio_StopEngine( audio );
	}

	virtual HRESULT CommitChanges( UINT32 OperationSet ) override {
		return FAudio_CommitOperationSet( audio, OperationSet );
	}

	virtual void GetPerformanceData( XAUDIO2_PERFORMANCE_DATA * pPerfData ) override {
		FAudio_GetPerformanceData( audio, ( FAudioPerformanceData * )pPerfData );
	}

	virtual void SetDebugConfiguration( const XAUDIO2_DEBUG_CONFIGURATION * pDebugConfiguration, void * pReserved ) override {
		FAudio_SetDebugConfiguration( audio, ( FAudioDebugConfiguration * )pDebugConfiguration, NULL );
	}

	virtual HRESULT RegisterForCallbacks( IXAudio2EngineCallback * pCallback ) override {
		if ( pCallback == NULL ) {
			return XAUDIO2_E_INVALID_CALL;
		}
		InitEngineCallbackBridge( engineBridge, pCallback );
		registeredCallback = pCallback;
		return FAudio_RegisterForCallbacks( audio, &engineBridge.callback );
	}

	virtual void UnregisterForCallbacks( IXAudio2EngineCallback * pCallback ) override {
		if ( registeredCallback == NULL || pCallback != registeredCallback ) {
			return;
		}
		FAudio_UnregisterForCallbacks( audio, &engineBridge.callback );
		registeredCallback = NULL;
	}

	virtual UINT32 Release() override {
		if ( audio != NULL ) {
			FAudio_Release( audio );
			audio = NULL;
		}
		delete this;
		return 0;
	}

	FAudio * GetFAudio() const { return audio; }
	void SetFAudio( FAudio * audio_ ) { audio = audio_; }

private:
	FAudio *				audio;
	IXAudio2EngineCallback *	registeredCallback;
	engineCallbackBridge_t	engineBridge;
};

/*
================================================================================================

	Entry points

================================================================================================
*/

class VolumeMeterStub : public IUnknown {
public:
	virtual UINT32 Release() override { return 0; }
};

HRESULT XAudio2CreateVolumeMeter( IUnknown ** ppApo, UINT32 Flags ) {
	// effect chains are ignored by the FAudio backend, so hand out a dummy object
	static VolumeMeterStub dummyApo;

	if ( ppApo == NULL ) {
		return XAUDIO2_E_INVALID_CALL;
	}

	*ppApo = &dummyApo;
	return S_OK;
}

HRESULT XAudio2Create( IXAudio2 ** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor ) {
	if ( ppXAudio2 == NULL ) {
		return XAUDIO2_E_INVALID_CALL;
	}

	FAudio * audio = NULL;
	const HRESULT hr = FAudioCreate( &audio, Flags, XAudio2Processor );
	if ( FAILED( hr ) ) {
		return hr;
	}

	XAudio2Impl * xaudio2 = new XAudio2Impl();
	xaudio2->SetFAudio( audio );
	*ppXAudio2 = xaudio2;
	return S_OK;
}
