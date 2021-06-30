#pragma once
#include <Shared/Types.hpp>

enum class GaugeType : uint16 {
	Normal = 0,
	Hard,
	Permissive,
	Blastive,
};

enum class AutoFlags : uint8 {
	None = 0,
	AutoBT = 0x1,
	AutoFX = 0x2,
	AutoLaser = 0x4,
};

typedef struct PlaybackOptions
{
	static PlaybackOptions FromFlags(uint32 flags);
	static uint32 ToLegacyFlags(const PlaybackOptions& options);

	GaugeType gaugeType = GaugeType::Normal;
	float gaugeLevel = 0;

	// Per gauge type defined option, i.e. star rating
	uint32 gaugeOption = 0;
	bool mirror = false;
	bool random = false;

	// If true and gaugeType != normal then insert a normal type gauge as a fallback
	bool backupGauge = false;

	AutoFlags autoFlags = AutoFlags::None;
} PlaybackOptions;