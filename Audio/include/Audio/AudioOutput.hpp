#pragma once

class IMixer
{
public:
	virtual void Mix(void* data, uint32& numSamples) = 0;
};

/*
	Low level audio output
*/
class AudioOutput : public NonCopyable
{
public:
	AudioOutput();
	~AudioOutput();

	bool Init(bool exclusive);

	// Safe to start mixing
	void Start(IMixer* mixer);
	// Should stop mixing
	void Stop();

	uint32_t GetNumChannels() const;
	uint32_t GetSampleRate() const;

	// The actual length of the buffer in seconds
	double GetBufferLength() const;
	bool IsIntegerFormat() const;

private:
	class AudioOutput_Impl* m_impl;
};
