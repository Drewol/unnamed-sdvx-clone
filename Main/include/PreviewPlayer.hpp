#pragma once

#include "stdafx.h"

class AudioStream;

class PreviewPlayer
{
public:
	void FadeTo(Ref<AudioStream> stream, int32 restart_pos=-1);
	void Update(float deltaTime);
	void Pause();
	void Restore();
	void StopCurrent();
private:
	static const float m_fadeDuration;
	static const float m_fadeDelayDuration;
	float m_fadeInTimer = 0.0f;
	float m_fadeOutTimer = 0.0f;
	float m_fadeDelayTimer = 0.0f;
	Ref<AudioStream> m_nextStream;
	int32 m_nextRestartPos = -1;
	Ref<AudioStream> m_currentStream;
	int32 m_currentRestartPos = -1;
};

typedef struct PreviewParams
{
	String filepath;
	uint32 offset;
	uint32 duration;

	bool operator!=(const PreviewParams& rhs)
	{
		return filepath != rhs.filepath || offset != rhs.offset || duration != rhs.duration;
	}
} PreviewParams;
