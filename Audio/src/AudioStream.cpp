#include "stdafx.h"
#include "AudioStream.hpp"
#include "Audio.hpp"
#include "Audio_Impl.hpp"

class AudioStreamRes* CreateAudioStream_ogg(class Audio* audio, const String& path, bool preload);
class AudioStreamRes* CreateAudioStream_mp3(class Audio* audio, const String& path, bool preload);
class AudioStreamRes* CreateAudioStream_wav(class Audio* audio, const String& path, bool preload);
class AudioStreamRes* CreateAudioStream_ma(class Audio* audio, const String& path, bool preload);

Ref<AudioStreamRes> AudioStreamRes::Create(class Audio* audio, const String& path, bool preload)
{
	AudioStreamRes* impl = nullptr;

	auto TryCreateType = [&](int32 type)
	{
		if (type == 0)
			return CreateAudioStream_ogg(audio, path, preload);
		else if (type == 1)
			return CreateAudioStream_mp3(audio, path, preload);
		else
			return CreateAudioStream_ma(audio, path, preload);
	};

	int32 pref = 0;
	String ext = Path::GetExtension(path);
	if (ext == "mp3")
		pref = 1;
	else if (ext == "ogg")
		pref = 0;
	else if (ext == "wav")
		pref = 3;

	for(uint32 i = 0; i < 3; i++)
	{
		impl = TryCreateType(pref);
		if(impl)
			break;
		pref = (pref + 1) % 2;
	}

	if(!impl)
		return AudioStream();

	audio->GetImpl()->Register(impl);
	return AudioStream(impl);
}