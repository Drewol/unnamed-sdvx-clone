#pragma once
#include <Audio/AudioStream.hpp>
#include <Beatmap/BeatmapObjects.hpp>

class AudioPlayback;
class Beatmap;
struct ChartIndex;

class OffsetComputer
{
public:
	OffsetComputer(AudioPlayback& audioPlayback);
	OffsetComputer(Ref<AudioStream> music, const Beatmap& beatmap);
	OffsetComputer(const OffsetComputer&) = delete;
	OffsetComputer(OffsetComputer&&) = delete;

	OffsetComputer& operator= (const OffsetComputer&) = delete;
	OffsetComputer& operator= (OffsetComputer&&) = delete;

	bool Compute(int& outOffset);
	static bool Compute(const ChartIndex* chart, int& outOffset);

private:
	struct Beat {
		Beat(MapTime time, float weight) noexcept : time(time), weight(weight) {}
		Beat() noexcept : Beat(0, 1.0f) {}

		MapTime time;
		float weight;
	};

	// Length of the region to use for offset computation
	static constexpr MapTime COMPUTE_WINDOW = 25'000;

	// Maximal # of beats for consideration
	// (would never be achieved in practice)
	static constexpr int MAX_BEATS = 5'000;

	// Maximal absolute value for offset
	static constexpr MapTime MAX_OFFSET = 50;

	const float* m_pcm = nullptr;
	uint64 m_pcmCount = 0;
	uint32 m_sampleRate = 0;
	const Beatmap& m_beatmap;

	MapTime m_offsetCenter = 0;

	// Reads the beats from the chart
	void ReadBeats();
	// Most dense 30 seconds of the chart
	Vector<Beat> m_beats;

	// Computes the energy and likelihood of onset being present in the 30 seconds window
	void ComputeEnergy();
	MapTime m_energyOffset;
	// Energy and onset likelihood, stored ms-by-ms
	Vector<float> m_energy;
	Vector<float> m_onsetScore;

	int ComputeFitness(MapTime offset);
	float GetOnsetScore(MapTime time);
};
