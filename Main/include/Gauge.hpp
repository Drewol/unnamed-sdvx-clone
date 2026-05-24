#pragma once
#include <Beatmap/BeatmapObjects.hpp>
#include "Scoring.hpp"

class Gauge {
public:
	Gauge() = default;
	virtual ~Gauge() = default;
	virtual bool Init(MapTotals mapTotals, uint16 total, MapTime length) = 0;
	virtual void LongHit(bool sCritical = false) = 0;
	virtual void CritHit(bool sCritical = false) = 0;
	virtual void NearHit() = 0;
	virtual void LongMiss() = 0;
	virtual void ShortMiss() = 0;
	virtual void SetValue(float v) {
		m_gauge = v;
	}
	[[nodiscard]]
	virtual float GetValue() const {
		return m_gauge;
	};
	[[nodiscard]]
	virtual const std::array<float, 256>& GetSamples() const {
		return m_samples;
	}

	virtual void Update(MapTime currentTime);


	[[nodiscard]]
	virtual bool GetClearState() const = 0;
	[[nodiscard]]
	virtual const char* GetName() const = 0;
	[[nodiscard]]
	virtual GaugeType GetType() const = 0;
	[[nodiscard]]
	virtual uint32 GetOpts() const { return 0; };

	// Returns true if the gauge should fail out the player
	[[nodiscard]]
	virtual bool FailOut() const {
		return false;
	};
protected:
	virtual void InitSamples(MapTime length);

	std::array<float, 256> m_samples;
	float m_gauge = 0.0f;
	MapTime m_sampleDuration = 1;

};

class GaugeNormal : public Gauge {
public:
	GaugeNormal(float gainRate = 1.0f, float missDrainPercent = 0.02f) :
		s_gainRate(gainRate), s_missDrainPercent(missDrainPercent) {};
	~GaugeNormal() = default;
	bool Init(MapTotals mapTotals, uint16 total, MapTime length);
	void LongHit(bool sCritical = false);
	void CritHit(bool sCritical = false);
	void NearHit();
	void LongMiss();
	void ShortMiss();
	[[nodiscard]]
	bool GetClearState() const;
	[[nodiscard]]
	const char* GetName() const;
	[[nodiscard]]
	GaugeType GetType() const;

protected:
	const float s_gainRate = 1.0f;
	const float s_missDrainPercent = 0.02f;

	float m_shortMissDrain;
	float m_drainMultiplier;
	float m_shortGaugeGain;
	float m_tickGaugeGain;
};

class GaugeHard : public GaugeNormal {
public:
	GaugeHard(float gainRate = 12.f / 21.f, float missDrainPercent = 0.09f) :
		GaugeNormal(gainRate, missDrainPercent) {};
	~GaugeHard() = default;
	bool Init(MapTotals mapTotals, uint16 total, MapTime length) override;
	void LongMiss() override;
	void ShortMiss() override;
	[[nodiscard]]
	bool GetClearState() const;
	[[nodiscard]]
	const char* GetName() const;
	[[nodiscard]]
	bool FailOut() const;
	[[nodiscard]]
	GaugeType GetType() const;

protected:
	[[nodiscard]]
	float DrainMultiplier() const;
};

class GaugePermissive : public GaugeHard {
public:
	GaugePermissive(float gainRate = 16.f / 21.f, float missDrainPercent = 0.034f) :
		GaugeHard(gainRate, missDrainPercent) {};
protected:
	[[nodiscard]]
	const char* GetName() const;
	[[nodiscard]]
	GaugeType GetType() const;
};

class GaugeWithLevel : public GaugeHard {
public:
	GaugeWithLevel(float level, float gainRate, float missDrainPercent) :
		GaugeHard(gainRate, missDrainPercent), m_level(level) {};
	void LongMiss() override;
	void ShortMiss() override;
	[[nodiscard]]
	uint32 GetOpts() const override;
	[[nodiscard]]
	float GetLevel() const noexcept { return m_level; }
protected:
	float m_level;
};

class GaugeBlastive : public GaugeWithLevel {
public:
	GaugeBlastive(float level, float gainRate = 12.f / 21.f, float missDrainPercent = 0.04f) :
		GaugeWithLevel(level, gainRate, missDrainPercent) {};
	bool Init(MapTotals mapTotals, uint16 total, MapTime length) override;
	void NearHit() override;
	[[nodiscard]]
	const char* GetName() const;
	[[nodiscard]]
	GaugeType GetType() const;
protected:
	const float s_nearDrainPercent = 0.01f;

	float m_shortNearDrain;
};

class GaugeMaxxive : public GaugeHard {
public:
	GaugeMaxxive() : GaugeHard(0.0f, 0.0f) {};
	bool Init(MapTotals mapTotals, uint16 total, MapTime length) override;
	void LongHit(bool sCritical = false) override;
	void CritHit(bool sCritical = false) override;
	void NearHit() override;
	void LongMiss() override;
	void ShortMiss() override;
	[[nodiscard]]
	const char* GetName() const override;
	[[nodiscard]]
	GaugeType GetType() const override;
};

class GaugeRateBase : public Gauge {
public:
	GaugeRateBase(float startGauge, float critTotal, float nearTotal, float shortMissDrain, float longMissDrain) :
		m_startGauge(startGauge),
		m_critTotal(critTotal),
		m_nearTotal(nearTotal),
		m_shortMissDrain(shortMissDrain),
		m_longMissDrain(longMissDrain) {};
	bool Init(MapTotals mapTotals, uint16 total, MapTime length) override;
	void LongHit(bool sCritical = false) override;
	void CritHit(bool sCritical = false) override;
	void NearHit() override;
	void LongMiss() override;
	void ShortMiss() override;
	[[nodiscard]]
	bool GetClearState() const override;

protected:
	void m_Add(float amount);
	void m_ConsumeShort();
	void m_ConsumeLong();

	const float m_startGauge;
	const float m_critTotal;
	const float m_nearTotal;
	const float m_shortMissDrain;
	const float m_longMissDrain;
	float m_critShortGain = 0.0f;
	float m_critLongGain = 0.0f;
	float m_nearShortGain = 0.0f;
	float m_nearLongGain = 0.0f;
	uint32 m_remainingGaugeUnits = 0;
};

class GaugeBasic : public GaugeRateBase {
public:
	GaugeBasic() : GaugeRateBase(0.10f, 3.00f, 1.50f, 0.01f, 0.003f) {};
	[[nodiscard]]
	const char* GetName() const override;
	[[nodiscard]]
	bool FailOut() const override;
	[[nodiscard]]
	GaugeType GetType() const override;
};

class GaugeEasy : public GaugeRateBase {
public:
	GaugeEasy() : GaugeRateBase(0.0f, 4.00f, 2.00f, 0.005f, 0.002f) {};
	[[nodiscard]]
	const char* GetName() const override;
	[[nodiscard]]
	GaugeType GetType() const override;
};

class GaugeMaimaiDx : public GaugeRateBase {
public:
	GaugeMaimaiDx(bool crashEnabled) : GaugeRateBase(0.0f, 1.00f, 0.80f, 0.0f, 0.0f), m_crashEnabled(crashEnabled) {};
	bool Init(MapTotals mapTotals, uint16 total, MapTime length) override;
	void LongHit(bool sCritical = false) override;
	void CritHit(bool sCritical = false) override;
	[[nodiscard]]
	bool GetClearState() const override;
	[[nodiscard]]
	const char* GetName() const override;
	[[nodiscard]]
	bool FailOut() const override;
	[[nodiscard]]
	GaugeType GetType() const override;

private:
	float m_sCriticalShortGain = 0.0f;
	float m_sCriticalLongGain = 0.0f;
	bool m_crashEnabled = false;
};
