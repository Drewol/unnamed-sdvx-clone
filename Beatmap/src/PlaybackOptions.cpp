#include "stdafx.h"
#include "PlaybackOptions.hpp"

enum class LegacyGameFlags : uint32
{
	None = 0,

	Hard = 0b1,

	Mirror = 0b10,

	Random = 0b100,

	AutoBT = 0b1000,

	AutoFX = 0b10000,

	AutoLaser = 0b100000,
	End
};


PlaybackOptions PlaybackOptions::FromFlags(uint32 flags)
{
	PlaybackOptions res;
	if ((flags & (uint32)LegacyGameFlags::Hard) != 0) {
		res.gaugeType = GaugeType::Hard;
	}

	res.random = (flags & (uint32)LegacyGameFlags::Random);
	res.mirror = (flags & (uint32)LegacyGameFlags::Mirror);
	res.autoFlags = (AutoFlags)((flags >> 3) & 0b111);

	return res;
}

uint32 PlaybackOptions::ToLegacyFlags(const PlaybackOptions& options)
{
	uint32 flags = 0;
	flags |= options.gaugeType != GaugeType::Normal ? (uint32)LegacyGameFlags::Hard : 0;
	flags |= options.mirror ? (uint32)LegacyGameFlags::Mirror : 0;
	flags |= options.random ? (uint32)LegacyGameFlags::Random : 0;
	flags |= (uint32)options.autoFlags << 3;
	return flags;
}
