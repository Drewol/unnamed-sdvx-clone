#include "stdafx.h"
#include <Audio/Audio.hpp>
#include <Beatmap/BeatmapPlayback.hpp>
#include <Audio/DSP.hpp>
#include "TestMusicPlayer.hpp"

// Normal test map
static String testBeatmapPath = Path::Normalize("songs/love is insecurable/love_is_insecurable.ksh");
//static String testBeatmapPath = Path::Normalize("songs/Yggdrasil/Yggdrasil_ex.ksh");
// Song with speed changes
static String testBeatmapPath1 = Path::Normalize("songs/soflan/Konran shoujo Soflan-chan!!.ksh");

static inline void LoadTestBeatmap(Beatmap& beatmap, const String& mapPath = testBeatmapPath)
{
	File file;
	TestEnsure(file.OpenRead(mapPath));
	FileReader reader(file);
	TestEnsure(beatmap.Load(reader));
}

Test("Beatmap.v160")
{
	Beatmap map;
	LoadTestBeatmap(map, Path::Normalize("D:\\KShoot/songs/Other/CHNLDiVR/exh.ksh"));

	// Should have bitcrush effect
	bool haveBitc = false;
	for(auto& it : map.GetObjectStates())
	{
		MultiObjectState* mobj = *(it.get());
		if(mobj->type == ObjectType::Hold)
		{
			if(mobj->hold.effectType == EffectType::Bitcrush)
			{
				haveBitc = true;
			}
		}
	}
	TestEnsure(haveBitc);
}

// Test loading fo map + metadata without errors
Test("Beatmap.Loading")
{
	Beatmap beatmap;
	LoadTestBeatmap(beatmap);
	BeatmapSettings settings = beatmap.GetMapSettings();
	Logf("Artist: %s\n\t\tTitle: %s\n\t\tEffector: %s\n\t\tIllust: %s", Logger::Severity::Info,
		settings.artist, settings.title, settings.effector, settings.illustrator);
	Logf("Audio File: %s;%s(fx)", Logger::Severity::Info, settings.audioNoFX, settings.audioFX);
	Logf("Jacket File: %s", Logger::Severity::Info, settings.jacketPath);
}

// Test 4/4 single bpm map
Test("Beatmap.Playback")
{
	Beatmap beatmap;
	LoadTestBeatmap(beatmap);
	String mapRootPath = Path::RemoveLast(testBeatmapPath);

	class Player : public TestMusicPlayer
	{
	public:
		Beatmap& beatmap;
		BeatmapPlayback playback;
		String beatmapRootPath;
		Player(Beatmap& bm, const String& beatmapRootPath) : beatmap(bm), beatmapRootPath(beatmapRootPath)
		{
			const BeatmapSettings& settings = beatmap.GetMapSettings();
			String songPath = Path::Normalize(beatmapRootPath + Path::sep + settings.audioNoFX);
			Init(songPath, settings.previewOffset);
			playback = BeatmapPlayback(beatmap);
			playback.Reset(settings.previewOffset);
			playback.OnTimingPointChanged.AddLambda([](Beatmap::TimingPointsIterator it)
			{
				float bpm = (float)it->GetBPM();
				Logf("T %.2f BPM %d/%d", Logger::Severity::Info, bpm, it->numerator, it->denominator);
			});
		}
		void Update(float dt) override
		{
			uint32 lastTime = playback.GetLastTime();
			uint32 mapTime = song->GetPosition();
			int32 beatIndex = 0;
			int32 beatCount = playback.CountBeats(lastTime, mapTime - lastTime, beatIndex, 1);
			if(beatCount > 0)
				Logf("> %d", Logger::Severity::Info, beatIndex);
			playback.Update(mapTime);
		}
	};
	Player player(beatmap, mapRootPath);
	player.Run();
}

// Different map with many BPM changes
Test("Beatmap.BPMChanges")
{
	Beatmap beatmap;
	LoadTestBeatmap(beatmap, testBeatmapPath1);
	String mapRootPath = Path::RemoveLast(testBeatmapPath1);

	class Player : public TestMusicPlayer
	{
	public:
		Beatmap& beatmap;
		BeatmapPlayback playback;
		String beatmapRootPath;
		Player(Beatmap& bm, const String& beatmapRootPath) : beatmap(bm), beatmapRootPath(beatmapRootPath)
		{
			const BeatmapSettings& settings = beatmap.GetMapSettings();
			String songPath = Path::Normalize(beatmapRootPath + Path::sep + settings.audioNoFX);
			Init(songPath, settings.previewOffset);
			playback = BeatmapPlayback(beatmap);
			playback.Reset(settings.previewOffset);
			playback.OnTimingPointChanged.AddLambda([](Beatmap::TimingPointsIterator it)
			{
				float bpm = (float) it->GetBPM();
				Logf("T %.2f BPM %d/%d", Logger::Severity::Info, bpm, it->numerator, it->denominator);
			});
		}
		void Update(float dt) override
		{
			uint32 lastTime = playback.GetLastTime();
			uint32 mapTime = song->GetPosition();
			int32 beatIndex = 0;
			int32 beatCount = playback.CountBeats(lastTime, mapTime - lastTime, beatIndex, 1);
			if(beatCount > 0)
				Logf("> %d", Logger::Severity::Info, beatIndex);
			playback.Update(mapTime);
		}
	};
	Player player(beatmap, mapRootPath);
	player.Run();
}

// Test applying effects over maps
Test("Beatmap.DoubleFilter")
{
	Beatmap beatmap;
	LoadTestBeatmap(beatmap);
	String mapRootPath = Path::RemoveLast(testBeatmapPath);

	class Player : public TestMusicPlayer
	{
	public:
		Beatmap& beatmap;
		BeatmapPlayback playback;
		String beatmapRootPath;
		BQFDSP* filter[2];

		Vector<LaserObjectState*> lasers;
		float laserValue[2] = { 0.0f };

		Player(Beatmap& bm, const String& beatmapRootPath) : beatmap(bm), beatmapRootPath(beatmapRootPath)
		{
			const BeatmapSettings& settings = beatmap.GetMapSettings();
			String songPath = Path::Normalize(beatmapRootPath + Path::sep + settings.audioNoFX);
			Init(songPath, 0);

			playback = BeatmapPlayback(beatmap);
			playback.Reset(0);
			playback.OnTimingPointChanged.AddLambda([](Beatmap::TimingPointsIterator it) {
				float bpm = (float)it->GetBPM();
				Logf("T %.2f BPM %d/%d", Logger::Severity::Info, bpm, it->numerator, it->denominator);
			});
			playback.OnObjectEntered.AddLambda([&](ObjectState* obj) {
				if(obj->type == ObjectType::Laser)
				{
					LaserObjectState* laserObj = (LaserObjectState*)obj;
					lasers.Add(laserObj);
				}
			});

			for(int i = 0; i < 2; i++)
			{
				filter[i] = new BQFDSP(song->GetAudioSampleRate());
				song->AddDSP(filter[i]);
			}
		}
		void Update(float dt) override
		{
			int32 lastTime = playback.GetLastTime();
			int32 mapTime = song->GetPosition();
			int32 beatIndex = 0;
			int32 beatCount = playback.CountBeats(lastTime, mapTime - lastTime, beatIndex, 4);
			if(beatCount > 0)
				Logf("> %d [%f] [%f]", Logger::Severity::Info, beatIndex, laserValue[0], laserValue[1]);
			playback.Update(mapTime);

			laserValue[0] = 0.0f;
			laserValue[1] = 0.0f;
			for(auto it = lasers.begin(); it != lasers.end();)
			{
				LaserObjectState* laser = *it;
				if(laser->time + laser->duration < mapTime)
				{
					it = lasers.erase(it);
					continue;
				}
				if(laser->time > mapTime)
				{
					it++;
					continue;
				}

				float& dst = laserValue[laser->index];
				dst = laser->SamplePosition(mapTime);
				if((laser->flags & LaserObjectState::flag_Extended) != 0)
					dst = LaserObjectState::ConvertToNormalRange(dst);
				it++;
			}

			float dist = abs(laserValue[0] - laserValue[1]);
			float distAtten = 1.0f;
			if(dist < 0.2f)
				distAtten = dist / 0.2f * 0.5f + 0.5f;

			for(int i = 0; i < 2; i++)
			{
				// Sweeping filter settings
				const float filterSweepStart = 80.0f;
				const float filterSweepEnd = 5000.0f;
				const float filterSweepBwStart = 1.5f;
				const float filterSweepBwEnd = 3.0f;
				const float filterSweepGainStart = 20.0f;
				const float filterSweepGainEnd = 10.0f;
				float sweepInput = laserValue[i];
				if(i == 1)
					sweepInput = 1.0f - laserValue[i];
				sweepInput = pow(sweepInput, 2.0f);
				float sweepFreq = filterSweepStart + (filterSweepEnd - filterSweepStart) * sweepInput;
				float sweepBw = filterSweepBwStart + (filterSweepBwEnd - filterSweepBwStart) * sweepInput;
				float sweepGain = filterSweepGainStart + (filterSweepGainEnd - filterSweepGainStart) * sweepInput;
				if(sweepInput < 0.05f)
					sweepGain *= (sweepInput / 0.05f);
				filter[i]->SetPeaking(sweepBw, sweepFreq, sweepGain * distAtten);
			}
		}
	};
	Player player(beatmap, mapRootPath);
	player.Run();
}

// Test applying effects over maps
Test("Beatmap.SingleFilter")
{
	Beatmap beatmap;
	LoadTestBeatmap(beatmap);
	String mapRootPath = Path::RemoveLast(testBeatmapPath);

	class Player : public TestMusicPlayer
	{
	public:
		Beatmap& beatmap;
		BeatmapPlayback playback;
		String beatmapRootPath;
		BQFDSP* filter;

		Vector<LaserObjectState*> lasers;
		float laserValue[2] = { 0.0f };

		Player(Beatmap& bm, const String& beatmapRootPath) : beatmap(bm), beatmapRootPath(beatmapRootPath)
		{
			const BeatmapSettings& settings = beatmap.GetMapSettings();
			String songPath = Path::Normalize(beatmapRootPath + Path::sep + settings.audioNoFX);
			Init(songPath, 0);

			playback = BeatmapPlayback(beatmap);
			playback.Reset(0);
			playback.OnTimingPointChanged.AddLambda([](Beatmap::TimingPointsIterator it) {
				float bpm = (float)it->GetBPM();
				Logf("T %.2f BPM %d/%d", Logger::Severity::Info, bpm, it->numerator, it->denominator);
			});
			playback.OnObjectEntered.AddLambda([&](ObjectState* obj) {
				if(obj->type == ObjectType::Laser)
				{
					LaserObjectState* laserObj = (LaserObjectState*)obj;
					lasers.Add(laserObj);
				}
			});

			filter = new BQFDSP(song->GetAudioSampleRate());
			song->AddDSP(filter);
		}
		void Update(float dt) override
		{
			int32 lastTime = playback.GetLastTime();
			int32 mapTime = song->GetPosition();
			int32 beatIndex = 0;
			int32 beatCount = playback.CountBeats(lastTime, mapTime - lastTime, beatIndex, 4);
			if(beatCount > 0)
				Logf("> %d [%f] [%f]", Logger::Severity::Info, beatIndex, laserValue[0], laserValue[1]);
			playback.Update(mapTime);

			laserValue[0] = 0.0f;
			laserValue[1] = 0.0f;
			for(auto it = lasers.begin(); it != lasers.end();)
			{
				LaserObjectState* laser = *it;
				if(laser->time + laser->duration < mapTime)
				{
					it = lasers.erase(it);
					continue;
				}
				if(laser->time > mapTime)
				{
					it++;
					continue;
				}

				float& dst = laserValue[laser->index];
				dst = laser->SamplePosition(mapTime);
				if((laser->flags & LaserObjectState::flag_Extended) != 0)
					dst = LaserObjectState::ConvertToNormalRange(dst);
				it++;
			}

			float dist = abs(laserValue[0] - laserValue[1]);
			float distAtten = 1.0f;
			if(dist < 0.2f)
				distAtten = dist / 0.2f * 0.5f + 0.5f;

			// Sweeping filter settings
			const float filterSweepStart = 80.0f;
			const float filterSweepEnd = 5000.0f;
			const float filterSweepBwStart = 1.5f;
			const float filterSweepBwEnd = 3.0f;
			const float filterSweepGainStart = 20.0f;
			const float filterSweepGainEnd = 10.0f;
			float sweepInput = Math::Max(laserValue[0], (1.0f - laserValue[1]));
			sweepInput = pow(sweepInput, 2.0f);
			float sweepFreq = filterSweepStart + (filterSweepEnd - filterSweepStart) * sweepInput;
			float sweepBw = filterSweepBwStart + (filterSweepBwEnd - filterSweepBwStart) * sweepInput;
			float sweepGain = filterSweepGainStart + (filterSweepGainEnd - filterSweepGainStart) * sweepInput;
			if(sweepInput < 0.05f)
				sweepGain *= (sweepInput / 0.05f);
			filter->SetPeaking(sweepBw, sweepFreq, sweepGain * distAtten);
		}
	};
	Player player(beatmap, mapRootPath);
	player.Run();
}
