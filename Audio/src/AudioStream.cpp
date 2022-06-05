#include "stdafx.h"
#include "AudioStream.hpp"
#include "AudioStreamMa.hpp"
#include "AudioStreamMp3.hpp"
#include "AudioStreamOgg.hpp"
#include "AudioStreamWav.hpp"
#include "AudioStreamPcm.hpp"
#include <unordered_map>

using CreateFunc = Ref<AudioStream>(Audio*, const String&, bool);

static std::unordered_map<std::string, CreateFunc&> decoders = {
	{"mp3", AudioStreamMp3::Create},
	{"ogg", AudioStreamOgg::Create},
	{"wav", AudioStreamMa::Create},
	//{"wav", AudioStreamWav::Create},
};

static Ref<AudioStream> FindImplementation(Audio* audio, const String& path, bool preload)
{
	Ref<AudioStream> impl;
	// Try decoder based on extension
	auto fav = decoders.find(Path::GetExtension(path));
	if (fav != decoders.end())
	{
		impl = fav->second(audio, path, preload);
		if (impl)
		{
			return impl;
		}
	}
	// Fallback on trying each other method
	for (auto it = decoders.begin(); it != decoders.end(); it++)
	{
		if (fav == it)
			continue;
		impl = it->second(audio, path, preload);
		if (impl)
		{
			return impl;
		}
	}
	return impl;
}

Ref<AudioStream> AudioStream::Create(Audio* audio, const String& path, bool preload)
{
	Ref<AudioStream> impl = FindImplementation(audio, path, preload);
	if (impl)
		audio->GetImpl()->Register(impl.get());
	return impl;
}

Ref<AudioStream> AudioStream::Clone(Audio* audio, Ref<AudioStream> source)
{
	auto clone = AudioStreamPcm::Create(audio, source);
	if (clone)
		audio->GetImpl()->Register(clone.get());
	return clone;
}
