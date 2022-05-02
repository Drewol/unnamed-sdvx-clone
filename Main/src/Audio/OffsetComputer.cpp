#include "stdafx.h"
#include "Audio/AudioPlayback.hpp"
#include "Audio/OffsetComputer.hpp"

#include <Audio/Audio_Impl.hpp>
#include <Beatmap/Beatmap.hpp>
#include <Beatmap/BeatmapPlayback.hpp>
#include <Beatmap/MapDatabase.hpp>
#include <Shared/Profiling.hpp>
#include <array>

static inline double GetBeatWeight(double d)
{
	d -= Math::Floor(d);
	if (d >= 0.5) return d;
	return 1.0 - d;
}

OffsetComputer::OffsetComputer(AudioPlayback& audioPlayback)
	: OffsetComputer(audioPlayback.GetMusic(), audioPlayback.GetBeatmap())
{
}

OffsetComputer::OffsetComputer(Ref<AudioStream> music, const Beatmap& beatmap)
	: m_pcm(music->GetPCM()), m_pcmCount(music->GetPCMCount()), m_sampleRate(music->GetSampleRate()),
	m_beatmap(beatmap)
{
}

bool OffsetComputer::Compute(const ChartIndex* chart, int& outOffset)
{
	const String chartPath = Path::Normalize(chart->path);
	const String chartRootPath = Path::RemoveLast(chartPath, nullptr);

	Beatmap beatmap;
	File mapFile;

	if (!mapFile.OpenRead(chartPath))
		return false;

	FileReader reader(mapFile);
	if (!beatmap.Load(reader))
		return false;

	BeatmapPlayback beatmapPlayback(beatmap);
	beatmapPlayback.Reset();

	AudioPlayback audioPlayback;
	if (!audioPlayback.Init(beatmapPlayback, chartRootPath, false))
		return false;

	return OffsetComputer(audioPlayback).Compute(outOffset);
}

bool OffsetComputer::Compute(int& outOffset)
{
	ProfilerScope $("OffsetComputer::Compute");

	if (!m_pcm || m_pcmCount <= 0 || m_sampleRate <= 0)
	{
		Log("OffsetComputer::Compute: The stream is not loaded!", Logger::Severity::Warning);
		return false;
	}

	ReadBeats();

	if (m_beats.empty())
	{
		Log("OffsetComputer::Compute: # of beats is zero!", Logger::Severity::Warning);
		return false;
	}

	Logf("OffsetComputer::Compute: Using %d beats starting from %d...", Logger::Severity::Debug,
		m_beats.size(), m_beats[0].time);

	m_offsetCenter = outOffset;

	ComputeEnergy();
	
	if (m_energy.empty())
	{
		Log("OffsetComputer::Compute: Insufficient data...", Logger::Severity::Warning);
		return false;
	}

	std::array<int, MAX_OFFSET*2 + 1> fitnesses;

	// This O(MAX_OFFSET*MAX_BEATS) is sub-optimal for computing convolution,
	// but it's simple and doesn't matter a lot in practice.
	for(MapTime offset = -MAX_OFFSET; offset <= MAX_OFFSET; ++offset)
	{
		fitnesses[MAX_OFFSET + offset] = ComputeFitness(offset+m_offsetCenter);
	}

	std::vector<MapTime> peaks;

	for (MapTime offset = -MAX_OFFSET; offset <= MAX_OFFSET; ++offset)
	{
		// Only consider the peaks of fitnesses
		if (-MAX_OFFSET < offset && offset < MAX_OFFSET)
		{
			const int index = static_cast<int>(offset + MAX_OFFSET);
			if (!(fitnesses[index - 1] < fitnesses[index] && fitnesses[index] >= fitnesses[index + 1]))
				continue;
		}

		peaks.emplace_back(offset);
	}

	if (peaks.empty())
	{
		Log("OffsetComputer::Compute: Insufficient candidates...", Logger::Severity::Warning);
		return false;
	}

	std::sort(peaks.begin(), peaks.end(), [&fitnesses](MapTime a, MapTime b) {
		return fitnesses[a + MAX_OFFSET] > fitnesses[b + MAX_OFFSET];
	});

	for (size_t i = 0; i < 5 && i < peaks.size(); ++i)
	{
		Logf("offset %3d | score = %d", Logger::Severity::Info, peaks[i]+m_offsetCenter, fitnesses[peaks[i] + MAX_OFFSET]);
	}

	Logf("OffsetComputer::Compute: Determined offset: %d (fitness = %d)", Logger::Severity::Info, peaks[0]+m_offsetCenter, fitnesses[peaks[0] + MAX_OFFSET]);

	outOffset = static_cast<int>(peaks[0]+m_offsetCenter);
	return true;
}

// Read beats based on m_beatmap.
void OffsetComputer::ReadBeats()
{
	m_beats.clear();

	MapTime lastBeat = -1;

	int regionBeginInd = 0;
	int regionEndInd = 0;

	int maxBeatsBeginInd = 0;
	int maxBeatsCount = 0;

	const Vector<TimingPoint>& timingPoints = m_beatmap.GetTimingPoints();
	int timingPointInd = 0;

	for (const auto& object : m_beatmap.GetObjectStates())
	{
		MapTime currBeat = lastBeat;
		switch (object->type)
		{
		case ObjectType::Single:
		case ObjectType::Hold:
			currBeat = object->time;
			break;
		default:
			continue;
			break;
		}

		if (currBeat == lastBeat) continue;

		while (regionBeginInd < regionEndInd && COMPUTE_WINDOW <= currBeat - m_beats[regionBeginInd].time)
		{
			++regionBeginInd;
		}

		// Compute weight based on beats
		float weight = 1.0f;
		if (!timingPoints.empty())
		{
			if (timingPointInd + 1 < static_cast<int>(timingPoints.size()))
			{
				if (timingPoints[timingPointInd + 1].time <= currBeat)
					++timingPointInd;
			}

			const TimingPoint& timingPoint = timingPoints[timingPointInd];
			const double barDuration = timingPoint.GetBarDuration();
			const double beatDuration = barDuration / timingPoint.numerator;

			double timingOffset = static_cast<double>(currBeat - timingPoint.time);

			weight = static_cast<float>(GetBeatWeight(timingOffset / barDuration) * 0.75 + GetBeatWeight(timingOffset / beatDuration) * 0.25);
		}

		m_beats.emplace_back(currBeat, weight);
		++regionEndInd;

		if (regionEndInd - regionBeginInd > maxBeatsCount)
		{
			maxBeatsCount = regionEndInd - regionBeginInd;
			maxBeatsBeginInd = regionBeginInd;
		}

		lastBeat = currBeat;
	}

	if (m_beats.empty()) return;
	assert(maxBeatsCount > 0);

	if (maxBeatsCount > MAX_BEATS)
	{
		Logf("The chart contains too much # of beats (%d / max %d)", Logger::Severity::Warning,
			maxBeatsCount, MAX_BEATS);

		maxBeatsCount = MAX_BEATS;
	}

	for (int i = 0; i < maxBeatsCount; ++i)
	{
		m_beats[i] = m_beats[maxBeatsBeginInd + i];
	}

	m_beats.resize(maxBeatsCount);
}

void OffsetComputer::ComputeEnergy()
{
	constexpr MapTime ENERGY_MARGIN = MAX_OFFSET + 5;
	constexpr MapTime ENERGY_COUNT = COMPUTE_WINDOW + ENERGY_MARGIN * 2;
	constexpr float ENERGY_EPSILON = 0.000'001f;

	m_energy.clear();
	m_energy.resize(ENERGY_COUNT, 0);
	
	m_onsetScore.clear();
	m_onsetScore.resize(ENERGY_COUNT, 0);

	if (m_beats.empty())
	{
		m_energyOffset = 0;
		return;
	}

	m_energyOffset = m_beats[0].time + m_offsetCenter - ENERGY_MARGIN;

	int64 ind = (static_cast<int64>(m_energyOffset) * m_sampleRate) / 1000;
	if (ind >= static_cast<int64>(m_pcmCount))
	{
		m_energy.clear();
		m_energyOffset = 0;
		m_onsetScore.clear();

		return;
	}

	const int64 endInd = ((static_cast<int64>(m_energyOffset) + ENERGY_COUNT) * m_sampleRate) / 1000;
	int64 nextInd = (static_cast<int64>(m_energyOffset + 1) * m_sampleRate) / 1000;
	int64 intervalSize = nextInd - ind;

	int64 energyInd = 0;
	
	float prevAmp = ind <= 0 ? 0 : std::hypotf(m_pcm[2*ind - 2], m_pcm[2*ind - 1]);
	float currAmp = ind < 0 ? 0 : std::hypotf(m_pcm[2*ind], m_pcm[2*ind + 1]);

	for (; ind < endInd; ++ind)
	{
		if (ind >= nextInd)
		{
			++energyInd;
			nextInd = ((static_cast<int64>(m_energyOffset) + energyInd + 1) * m_sampleRate) / 1000;
			intervalSize = nextInd - ind;

			// Check for abnormal cases
			if (intervalSize <= 0) intervalSize = 0;
			if (energyInd >= COMPUTE_WINDOW) break;
		}

		const float nextAmp = ind < -1 || ind + 1 >= static_cast<int64>(m_pcmCount) ? 0 : std::hypotf(m_pcm[2*ind + 2], m_pcm[2*ind + 3]);

		// Compute energy based on Newton's laws (?)
		const float v = (nextAmp - prevAmp) / 2;
		const float a = (prevAmp + nextAmp - 2 * currAmp);
		const float energySq = v * v - currAmp * a;
		m_energy[energyInd] += std::sqrt(energySq < 0 ? 0 : energySq) / intervalSize;

		prevAmp = currAmp;
		currAmp = nextAmp;

		if (ind + 1 >= static_cast<int64>(m_pcmCount)) break;
	}

	bool isQuiet = true;

	// This is an experimentally-determined formula.
	for (ind = 1; ind < ENERGY_COUNT; ++ind)
	{
		const float min_energy = m_energy[ind - 1] * 0.995f;
		if (m_energy[ind] < min_energy) m_energy[ind] = min_energy;

		if (m_energy[ind] > ENERGY_EPSILON)
			isQuiet = false;

		const float prevEnergy = std::log(std::max(m_energy[ind - 1], ENERGY_EPSILON));
		const float currEnergy = std::log(std::max(m_energy[ind], ENERGY_EPSILON));

		m_onsetScore[ind] = (currEnergy - prevEnergy) * m_energy[ind];
	}

	if (isQuiet)
	{
		m_energy.clear();
		m_onsetScore.clear();
	}
}

int OffsetComputer::ComputeFitness(MapTime offset)
{
	float fitness = 0;
	for (const Beat& beat : m_beats)
	{
		fitness += 10.0f * GetOnsetScore(beat.time + offset) * beat.weight;
	}

	return static_cast<int>(fitness);
}

float OffsetComputer::GetOnsetScore(MapTime time)
{
	time -= m_energyOffset;
	if (time < 0 || time >= static_cast<MapTime>(m_onsetScore.size()))
		return 0;

	return Math::Clamp(100 * m_onsetScore[time], -100.0f, 100.0f);
}
