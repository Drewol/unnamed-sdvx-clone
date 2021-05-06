#pragma once
#include <Beatmap/BeatmapObjects.hpp>

// Hit rating for hitting a button
enum class ScoreHitRating
{
	Miss = 0,
	Good,
	Perfect,
	Idle, // Not actual score, used when a button is pressed when there are no notes
};

// Hit statistic
struct HitStat
{
	HitStat(ObjectState* object);
	bool operator<(const HitStat& other);

	ObjectState* object;

	MapTime time;
	MapTime delta = 0;
	ScoreHitRating rating = ScoreHitRating::Miss;

	// Hold state
	// This is the amount of gotten ticks in a hold sequence
	uint32 hold = 0;
	// This is the amount of total ticks in this hold sequence
	uint32 holdMax = 0;
	// If at least one hold tick has been missed
	bool hasMissed = false;

	bool forReplay = true;
};

struct HitWindow
{
	enum class Type { None = 0, Normal, Hard };

	inline HitWindow(MapTime perfect, MapTime good) noexcept : perfect(perfect), good(good) { Validate(); }
	inline HitWindow(MapTime perfect, MapTime good, MapTime hold, MapTime slam) noexcept : perfect(perfect), good(good), hold(hold), slam(slam) { Validate(); }
	inline HitWindow(const HitWindow& that) noexcept : perfect(that.perfect), good(that.good), hold(that.hold), miss(that.miss), slam(that.slam) { Validate(); }

	static HitWindow FromConfig();
	void SaveConfig() const;

	void ToLuaTable(struct lua_State* L) const;

	inline HitWindow& operator= (const HitWindow& that) noexcept { perfect = that.perfect; good = that.good; hold = that.hold; miss = that.miss; slam = that.slam; return *this; }

	constexpr bool operator== (const HitWindow& that) const noexcept { return perfect == that.perfect && good == that.good && hold == that.hold && miss == that.miss && slam == that.slam; }
	constexpr bool operator<= (const HitWindow& that) const noexcept { return perfect <= that.perfect && good <= that.good && hold <= that.hold && miss <= that.miss && slam <= that.slam; }

	constexpr Type GetType() const noexcept { if (*this <= HARD) return Type::Hard; else if (*this <= NORMAL) return Type::Normal; else return Type::None; }

	inline bool Validate()
	{
		if (perfect <= good && good <= hold && hold <= miss && slam <= NORMAL.slam && miss <= NORMAL.miss)
			return true;

		Logf("Invalid timing window: %d/%d/%d/%d/%d", Logger::Severity::Warning, perfect, good, hold, slam, miss);

		if (miss > NORMAL.miss) miss = NORMAL.miss;
		if (hold > miss) hold = miss;
		if (good > hold) good = hold;
		if (perfect > good) perfect = good;
		if (slam > NORMAL.slam) slam = NORMAL.slam;

		return false;
	}

	MapTime perfect = 46;
	MapTime good = 150;
	MapTime hold = 150;
	MapTime miss = 300;
	MapTime slam = 84;

	static const HitWindow NORMAL;
	static const HitWindow HARD;
};