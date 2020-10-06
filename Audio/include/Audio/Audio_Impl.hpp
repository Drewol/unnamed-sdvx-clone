#pragma once
#include "AudioOutput.hpp"
#include "AudioBase.hpp"

#include <array>

// Threading
#include <thread>
#include <mutex>
using std::thread;
using std::mutex;

class Audio_Impl : public IMixer
{
public:
	Audio_Impl();

	void Start();
	void Stop();
	// Get samples
	virtual void Mix(void* data, uint32& numSamples) override;
	// Registers an AudioBase to be rendered
	void Register(AudioBase* audio);
	// Removes an AudioBase so it is no longer rendered
	void Deregister(AudioBase* audio);

	uint32 GetSampleRate() const;
	double GetSecondsPerSample() const;

	float globalVolume = 1.0f;

	mutex lock;
	Vector<AudioBase*> itemsToRender;
	Vector<DSP*> globalDSPs;

	class LimiterDSP* limiter = nullptr;
	uint32 m_remainingSamples = 0;

	thread audioThread;
	bool runAudioThread = false;
	AudioOutput* output = nullptr;

protected:
	// Used to limit rendering to a fixed number of samples
	constexpr static uint32 m_sampleBufferLength = 384;
	std::array<float, 2*m_sampleBufferLength> m_sampleBuffer;
	
private:
	alignas(sizeof(float))
	std::array<float, 2 * m_sampleBufferLength> m_itemBuffer;

	// Check memory corruption during filling m_itemBuffer
#if _DEBUG
	constexpr static uint32 m_guardBandSize = 256;

	alignas(1)
	std::array<uint8, m_guardBandSize> m_guard;

	void InitMemoryGuard();
	void CheckMemoryGuard();
#endif
};