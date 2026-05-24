#pragma once

class IMixer
{
public:
	virtual void Mix(void* data, uint32& numSamples) = 0;
};

/*
	Low level audio output
*/
class AudioOutput : public Unique
{
public:
	AudioOutput();
	~AudioOutput();

	bool Init(bool exclusive);

	// Safe to start mixing
	void Start(IMixer* mixer);
	// Should stop mixing
	void Stop();

	[[nodiscard]]
	uint32_t GetNumChannels() const;
	[[nodiscard]]
	uint32_t GetSampleRate() const;

	// The actual length of the buffer in seconds
	[[nodiscard]]
	double GetBufferLength() const;
	[[nodiscard]]
	bool IsIntegerFormat() const;

private:
	class AudioOutput_Impl* m_impl;
};
