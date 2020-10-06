#include "stdafx.h"
#include <Audio/Audio.hpp>
#include <Audio/DSP.hpp>
#include <float.h>
#include "TestMusicPlayer.hpp"

#include <thread>
using namespace std;

static String testSamplePath = Path::Normalize("audio/laser_slam1.wav");

// Test for music player
//static String testSongPath = Path::Normalize("songs/CHNLDiVR/mix1.ogg");
//static uint32 testSongOffset = 180000;
static String testSongPath = Path::Normalize("songs/noise/noise.ogg");
static uint32 testSongOffset = 0;

Test("Audio.Playback")
{
	Audio* audio = new Audio();
	TestEnsure(audio->Init(false));

	Sample testSample = audio->CreateSample(testSamplePath);
	TestEnsure(testSample);

	testSample->SetVolume(1.0f);

	Timer t;
	float time = FLT_MAX;
	while(true)
	{
		float deltaTime = t.SecondsAsFloat();
		t.Restart();
		time += deltaTime;
		t.Restart();
		if(time > 3.0f)
		{
			Log("Playing sample", Logger::Severity::Info);
			testSample->Play();
			time = 0.0;
		}
		this_thread::sleep_for(chrono::milliseconds(5));
	}

	delete audio;
}

Test("Audio.Music.Phaser")
{
	class MusicPlayer : public TestMusicPlayer
	{
		PhaserDSP* phaser;
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			phaser = new PhaserDSP(song->GetAudioSampleRate());
			phaser->dmin = 800.0f;
			phaser->dmax = 1000.0f;
			phaser->fb = 0.8f;
			song->AddDSP(phaser);
			phaser->SetLength(1000);
		}
		virtual void Update(float dt) override
		{
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.Wobble")
{
	class MusicPlayer : public TestMusicPlayer
	{
		WobbleDSP* wobble;
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			wobble = new WobbleDSP(song->GetAudioSampleRate());
			song->AddDSP(wobble);
			wobble->SetLength(200);
		}
		virtual void Update(float dt) override
		{
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.LPF")
{
	printf("\n");

	class MusicPlayer : public TestMusicPlayer
	{
		BQFDSP* filter;
		float filterSetting = 0.0f;
		float sweepDir = 1.0f;
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			filter = new BQFDSP(song->GetAudioSampleRate());
			song->AddDSP(filter);
			filter->SetLowPass(1.0f, 500.0f);
		}
		virtual void Update(float dt) override
		{
			// Sweeping filter settings
			const float filterSweepStart = 200.0f;
			const float filterSweepEnd = 10000.0f;
			const float filterSweepQStart = 1.0f;
			const float filterSweepQEnd = 1.0f;

			// Increment filter
			filterSetting += dt * 0.25f * sweepDir;
			filterSetting = Math::Clamp(filterSetting, 0.0f, 1.0f);
			if(filterSetting >= 1.0f)
				sweepDir = -1.0f;
			else if(filterSetting <= 0.0f)
				sweepDir = 1.0f;

			float sweepInput = pow(filterSetting, 2.0f);
			float sweepFreq = filterSweepStart + (filterSweepEnd - filterSweepStart) * sweepInput;
			float sweepQ = filterSweepQStart + (filterSweepQEnd - filterSweepQStart) * sweepInput;

			int32 playbackTime = song->GetPosition();
			printf("%08d > f:%f q:%f", playbackTime, sweepFreq, sweepQ);

			filter->SetLowPass(sweepQ, sweepFreq);
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.LPFMix")
{
	printf("\n");

	class MusicPlayer : public TestMusicPlayer
	{
		BQFDSP* filter;
		float filterSetting = 0.0f;
		float sweepDir = 1.0f;
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			filter = new BQFDSP(song->GetAudioSampleRate());
			song->AddDSP(filter);
			filter->SetLowPass(1.0f, 500.0f);
		}
		virtual void Update(float dt) override
		{
			// Sweeping filter settings
			const float filterSweepStart = 200.0f;
			const float filterSweepEnd = 10000.0f;
			const float filterSweepQStart = 1.0f;
			const float filterSweepQEnd = 1.0f;

			// Increment filter
			filterSetting += dt * 0.2f * sweepDir;
			filterSetting = Math::Clamp(filterSetting, 0.0f, 1.0f);
			if(filterSetting >= 1.0f)
				sweepDir = -1.0f;
			else if(filterSetting <= 0.0f)
				sweepDir = 1.0f;

			int32 playbackTime = song->GetPosition();
			float freq = 400.0f;
			float q = 4.0f;
			float mix = filterSetting;
			printf("%08d > mix:%f %f:%f", playbackTime, mix, freq, q);

			filter->mix = mix;
			filter->SetLowPass(q, freq);
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.Peaking")
{
	printf("\n");

	class MusicPlayer : public TestMusicPlayer
	{
		BQFDSP* filter;
		float filterSetting = 0.0f;
		float sweepDir = 1.0f;
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			filter = new BQFDSP(song->GetAudioSampleRate());
			song->AddDSP(filter);
			filter->SetLowPass(1.0f, 500.0f);
		}
		virtual void Update(float dt) override
		{
			// Sweeping filter settings
			const float filterSweepStart = 80.0f;
			const float filterSweepEnd = 5000.0f;
			const float filterSweepBwStart = 1.5f;
			const float filterSweepBwEnd = 3.0f;
			const float filterSweepAmpStart = 30.0f;
			const float filterSweepAmpEnd = 20.0f;

			// Increment filter
			filterSetting += dt * 0.5f * sweepDir;
			filterSetting = Math::Clamp(filterSetting, 0.0f, 1.0f);
			if(filterSetting >= 1.0f)
				sweepDir = -1.0f;
			else if(filterSetting <= 0.0f)
				sweepDir = 1.0f;

			float sweepInput = pow(filterSetting, 2.0f);
			float sweepFreq = filterSweepStart + (filterSweepEnd - filterSweepStart) * sweepInput;
			float sweepBw = filterSweepBwStart + (filterSweepBwEnd - filterSweepBwStart) * sweepInput;
			float sweepAmp = filterSweepAmpStart + (filterSweepAmpEnd - filterSweepAmpStart) * sweepInput;

			int32 playbackTime = song->GetPosition();
			printf("%08d > f:%f bw:%f amp:%f", playbackTime, sweepFreq, sweepBw, sweepAmp);

			filter->SetPeaking(sweepBw, sweepFreq, sweepAmp);
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.Echo")
{
	class MusicPlayer : public TestMusicPlayer
	{
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			EchoDSP* echo = new EchoDSP(song->GetAudioSampleRate());
			song->AddDSP(echo);
			echo->SetLength(3000);
			echo->feedback = 0.4f;
		}
		virtual void Update(float dt) override
		{
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}

Test("Audio.Music.Flanger")
{
	class MusicPlayer : public TestMusicPlayer
	{
	public:
		virtual void Init(const String& songPath, uint32 startOffset) override
		{
			TestMusicPlayer::Init(songPath, startOffset);

			FlangerDSP* fl = new FlangerDSP(song->GetAudioSampleRate());
			song->AddDSP(fl);
			fl->SetDelayRange(10, 120);
			fl->SetLength(24100);
			fl->mix = 1.0f;
		}
		virtual void Update(float dt) override
		{
		}
	};

	MusicPlayer mp;
	mp.Init(testSongPath, testSongOffset);
	mp.Run();
}
