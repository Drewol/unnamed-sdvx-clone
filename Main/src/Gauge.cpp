#include "stdafx.h"
#include "Gauge.hpp"
#include "GameConfig.hpp"

void Gauge::InitSamples(MapTime length)
{
	m_samples.fill(0.0f);
	m_sampleDuration = Math::Max((MapTime)1, length / (MapTime)m_samples.size());
}

void Gauge::Update(MapTime currentTime)
{
	int index = Math::Clamp(currentTime / m_sampleDuration, 0, (int)m_samples.size() - 1);
	m_samples.at(index) = m_gauge;
}

bool GaugeNormal::Init(MapTotals mapTotals, uint16 total, MapTime length)
{
	m_gauge = 0.0f;
	float ftotal = 2.10f + 0.001f; //Add a little in case floats go under
	bool manualTotal = total > 99;
	if (manualTotal)
	{
		ftotal = (float)total / 100.0f + 0.001f;
	}
	ftotal *= s_gainRate;

	if (mapTotals.numTicks == 0 && mapTotals.numSingles != 0)
	{
		m_shortGaugeGain = ftotal / (float)mapTotals.numSingles;
	}
	else if (mapTotals.numSingles == 0 && mapTotals.numTicks != 0)
	{
		m_tickGaugeGain = ftotal / (float)mapTotals.numTicks;
	}
	else
	{
		m_shortGaugeGain = (ftotal * 20) / (5.0f * ((float)mapTotals.numTicks + (4.0f * (float)mapTotals.numSingles)));
		m_tickGaugeGain = m_shortGaugeGain / 4.0f;
	}

	if (manualTotal)
	{
		m_drainMultiplier = 1.0;
	}
	else
	{
		MapTime drainNormal, drainHalf;
		drainNormal = g_gameConfig.GetInt(GameConfigKeys::GaugeDrainNormal);
		drainHalf = g_gameConfig.GetInt(GameConfigKeys::GaugeDrainHalf);

		double secondsOver = ((double)length / 1000.0) - (double)drainNormal;
		secondsOver = Math::Max(0.0, secondsOver);
		m_drainMultiplier = 1.0 / (1.0 + (secondsOver / (double)(drainHalf - drainNormal)));
	}
	m_shortMissDrain = s_missDrainPercent * m_drainMultiplier;
	InitSamples(length);
	return true;
}

void GaugeNormal::LongHit()
{
	m_gauge = Math::Min(1.0f, m_gauge + m_tickGaugeGain);
}

void GaugeNormal::CritHit()
{
	m_gauge = Math::Min(1.0f, m_gauge + m_shortGaugeGain);
}

void GaugeNormal::NearHit()
{
	m_gauge = Math::Min(1.0f, m_gauge + m_shortGaugeGain / 3.0f);
}

void GaugeNormal::LongMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - m_shortMissDrain / 4.0f);
}

void GaugeNormal::ShortMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - m_shortMissDrain);
}

bool GaugeNormal::GetClearState() const
{
	return m_gauge >= 0.7f;
}

const char* GaugeNormal::GetName() const
{
	return "Normal";
}

GaugeType GaugeNormal::GetType() const
{
	return GaugeType::Normal;
}

bool GaugeHard::Init(MapTotals mapTotals, uint16 total, MapTime length)
{
	GaugeNormal::Init(mapTotals, total, length);
	m_gauge = 1.0f;
	return true;
}

void GaugeHard::LongMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - (m_shortMissDrain / 4.0f) * DrainMultiplier());
}

void GaugeHard::ShortMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - m_shortMissDrain * DrainMultiplier());
}

bool GaugeHard::GetClearState() const
{
	return m_gauge > 0.f;
}

const char* GaugeHard::GetName() const
{
	return "Hard";
}

bool GaugeHard::FailOut() const
{
	return m_gauge == 0.f;
}

GaugeType GaugeHard::GetType() const
{
	return GaugeType::Hard;
}

float GaugeHard::DrainMultiplier() const
{
	// Thanks to Hibiki_ext in the discord for help with this
	return Math::Clamp(1.0f - ((0.3f - m_gauge) * 2.f), 0.5f, 1.0f);
}

const char* GaugePermissive::GetName() const
{
	return "Permissive";
}

GaugeType GaugePermissive::GetType() const
{
	return GaugeType::Permissive;
}

void GaugeWithLevel::LongMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - (m_shortMissDrain / 4.0f) * DrainMultiplier() * m_level);
}

void GaugeWithLevel::ShortMiss()
{
	m_gauge = Math::Max(0.0f, m_gauge - m_shortMissDrain * DrainMultiplier() * m_level);
}

bool GaugeBlastive::Init(MapTotals mapTotals, uint16 total, MapTime length)
{
	GaugeWithLevel::Init(mapTotals, total, length);
	m_shortNearDrain = s_nearDrainPercent * m_drainMultiplier;
	return true;
}

void GaugeBlastive::NearHit()
{
	if (m_level < 3.5)
		m_gauge = Math::Min(1.0f, m_gauge + m_shortGaugeGain / 3.0f);
	else
		m_gauge = Math::Max(0.0f, m_gauge - m_shortNearDrain * DrainMultiplier() * m_level);
}

const char* GaugeBlastive::GetName() const
{
	return "Blastive";
}

GaugeType GaugeBlastive::GetType() const
{
	return GaugeType::Blastive;
}

uint32 GaugeWithLevel::GetOpts() const
{
	return (uint32)(2 * m_level);
}
