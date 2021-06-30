#pragma once

#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include "Game.hpp"
#include <Beatmap/MapDatabase.hpp>

template<typename T>
class Optional {
protected:
	bool m_hasValue = false;
	T m_val;
public:
	Optional() : m_hasValue(false) {}
	Optional(T v) : m_hasValue(true), m_val(v) {}
	bool HasValue() const { return m_hasValue; }
	T operator*() const
	{
		assert(m_hasValue);
		return m_val;
	}
	void Reset()
	{
		m_hasValue = false;
	}
};

class Game;

template<typename T>
class ChallengeOption : public Optional<T> {
protected:
	bool m_isNull;
public:
	ChallengeOption() : Optional<T>(), m_isNull(false) {}
	ChallengeOption(T v) : Optional<T>(v), m_isNull(false) {}
	bool IsNull() const { return m_isNull; }
	void Reset()
	{
		Optional<T>::Reset();
		m_isNull = false;
	}
	ChallengeOption Merge(const ChallengeOption& overrider) const
	{
		if (overrider.HasValue() || overrider.IsNull())
			return overrider;
		return *this;
	}
	// Get the value with an default if it is not set
	T Get(const T& def) const
	{
		if (!this->HasValue())
			return def;
		return **this;
	}

	// This will pass if not overriden
	static ChallengeOption<T> IgnoreOption() {
		return ChallengeOption<T>();
	}
	// This will force an overriden to pass
	static ChallengeOption<T> DisableOption() {
		auto v = ChallengeOption<T>();
		v.m_isNull = true;
		return v;
	}
};

// This class add a m_passed value, which is true if the requirement
// was passed by the user (including a null requirement)
template<typename T>
class ChallengeRequirement : public ChallengeOption<T> {
protected:
	bool m_passed = false;
public:
	ChallengeRequirement() : ChallengeOption<T>(), m_passed(false) {}
	ChallengeRequirement(T v) : ChallengeOption<T>(v), m_passed(false) {}
	ChallengeRequirement(const ChallengeOption<T>& o) : ChallengeOption<T>(o), m_passed(false) {}
	// This will be true if marked as passed or no value
	bool PassedOrIgnored() const { return !this->HasValue() || m_passed; }
	// This will be true if marked as passed or null
	bool PassedOrNull() const { return m_passed || this->IsNull(); }
	void MarkPassed() { m_passed = true; }
	void Reset()
	{
		ChallengeOption<T>::Reset();
		m_passed = false;
	}
};

// NOTE: macros are used here to reduce the amount of copy paste required to update
//       or add new options
struct ChallengeOptions{
#define CHALLENGE_OPTIONS_ALL(v) \
	v(bool, mirror) \
	v(bool, excessive) \
	v(bool, permissive) \
	v(bool, blastive) \
	v(float, gauge_level) \
    v(bool, ars) \
	v(bool, gauge_carry_over) \
	v(bool, use_sdvx_complete_percentage) \
	v(uint32, min_modspeed) \
	v(uint32, max_modspeed) \
	v(float, hidden_min) \
	v(float, sudden_min) \
	v(uint32, crit_judge) \
	v(uint32, near_judge) \
	v(uint32, hold_judge) \
	v(uint32, slam_judge) \
	v(bool, allow_cmod) \
    v(bool, allow_excessive) \
    v(bool, allow_blastive) \
    v(bool, allow_permissive) \
    v(bool, allow_ars) \
	\
	v(uint32, average_percentage) \
	v(float, average_gauge) \
	v(uint32, average_errors) \
	v(uint32, max_overall_errors) \
	v(uint32, average_nears) \
	v(uint32, max_overall_nears) \
	v(uint32, average_crits) \
	v(uint32, min_overall_crits)


#define CHALLENGE_OPTION_DEC(t, n) ChallengeOption<t> n;
	CHALLENGE_OPTIONS_ALL(CHALLENGE_OPTION_DEC);
#undef CHALLENGE_OPTION_DEC

	ChallengeOptions Merge(const ChallengeOptions& overrider)
	{
		ChallengeOptions out;
#define CHALLENGE_OPTIONS_MERGE(t, n) out. n = n.Merge(overrider. n);
		CHALLENGE_OPTIONS_ALL(CHALLENGE_OPTIONS_MERGE);
#undef CHALLENGE_OPTIONS_MERGE
		return out;
	}
	void Reset()
	{
#define CHALLENGE_OPTION_RESET(t, n) n.Reset();
		CHALLENGE_OPTIONS_ALL(CHALLENGE_OPTION_RESET);
#undef CHALLENGE_OPTION_RESET
	}
#undef CHALLENGE_OPTIONS_ALL
};

struct ChallengeRequirements
{
#define CHALLENGE_REQS_ALL(v) \
	v(bool, clear) \
	v(uint32, min_percentage) \
	v(float, min_gauge) \
	v(uint32, max_errors) \
	v(uint32, max_nears) \
	v(uint32, min_crits) \
	v(uint32, min_chain)
#define CHALLENGE_REQ_DEC(t, n) ChallengeRequirement<t> n;
	CHALLENGE_REQS_ALL(CHALLENGE_REQ_DEC);
#undef CHALLENGE_REQ_DEC
	// This takes a second evaluated reqs which override this one
	// if the second set doesn't have an active member, this one is used
	bool Passed(struct ChallengeRequirements& over)
	{
#define CHALLENGE_REQ_PASSED_OVERRIDE(t, n) res = res && (over. n.PassedOrNull() || n.PassedOrIgnored()); \
if (!res) { \
		Logf("[Challenge] Failed condition %s", Logger::Severity::Info, #n); return res; \
}
		bool res = true;
		CHALLENGE_REQS_ALL(CHALLENGE_REQ_PASSED_OVERRIDE);
		return res;
#undef CHALLENGE_REQ_PASSED_OVERRIDE
	}
	ChallengeRequirements Merge(const ChallengeRequirements& overrider)
	{
		ChallengeRequirements out;
#define CHALLENGE_REQS_MERGE(t, n) out. n = n.Merge(overrider. n);
		CHALLENGE_REQS_ALL(CHALLENGE_REQS_MERGE);
#undef CHALLENGE_REQS_MERGE
		return out;
	}
	bool Passed()
	{
#define CHALLENGE_REQ_PASSED(t, n) res = res && (n.PassedOrIgnored()); \
if (!res) { \
		Logf("[Challenge] Failed condition %s", Logger::Severity::Info, #n); return res; \
}
		bool res = true;
		CHALLENGE_REQS_ALL(CHALLENGE_REQ_PASSED);
		return res;
#undef CHALLENGE_REQ_PASSED
	}
	void Reset()
	{
#define CHALLENGE_REQ_RESET(t, n) n.Reset();
		CHALLENGE_REQS_ALL(CHALLENGE_REQ_RESET);
#undef CHALLENGE_REQ_RESET
	}
};

struct ChallengeResult
{
	bool passed;
	String failString;
	ClearMark badge;
	float gauge;
	PlaybackOptions opts;
	uint32 score;
	float percent;
	uint32 maxCombo;
	uint32 crits;
	uint32 nears;
	uint32 errors;
	// Stuff that doesn't matter to the challenge but the score screen
	// will display to the user
	struct {
		// TODO we might be able to grab this dynamically
		uint32 difficulty;
		MapTime beatmapDuration;
		MapTime medianHitDelta;
		MapTime medianHitDeltaAbs;
		float meanHitDelta;
		float meanHitDeltaAbs;
		uint32 earlies;
		uint32 lates;
		HitWindow hitWindow = HitWindow::NORMAL;
		float gaugeSamples[256];
		uint32 speedMod;
		uint32 speedModValue;
		bool showCover;
		float suddenCutoff;
		float hiddenCutoff;
		float suddenFade;
		float hiddenFade;
		Vector<SimpleHitStat> simpleNoteHitStats;
	} scorescreenInfo;
};

struct OverallChallengeResult
{
	bool passed;
	String failString;
	bool goodScore; // If this is a best clear or best score
	ClearMark clearMark;
	uint32 averagePercent;
	uint32 averageScore;
	float averageGauge;
	// TODO(itszn) should we use floats instead?
	uint32 averageErrors;
	uint32 averageNears;
	uint32 averageCrits;
	uint32 overallErrors;
	uint32 overallNears;
	uint32 overallCrits;
};

class ChallengeManager
{
private:
	class ChallengeSelect* m_challengeSelect;
	ChallengeIndex* m_chal = nullptr;
	ChartIndex* m_currentChart = nullptr;
	unsigned int m_chartIndex = 0;
	unsigned int m_chartsPlayed = 0;
	bool m_running = false;
	bool m_seenResults = false;
	Vector<uint32> m_scores;
	ChallengeRequirements m_globalReqs;
	Vector<ChallengeRequirements> m_reqs;
	ChallengeOptions m_globalOpts;
	Vector<ChallengeOptions> m_opts;

	Vector<ChallengeResult> m_results;
	OverallChallengeResult m_overallResults;

	ChallengeOptions m_currentOptions;
	bool m_passedCurrentChart;
	bool m_failedEarly;
	bool m_finishedCurrentChart;

	uint64 m_totalNears = 0;
	uint64 m_totalErrors = 0;
	uint64 m_totalCrits = 0;
	uint64 m_totalScore = 0;
	float m_totalPercentage = 0;
	float m_totalGauge = 0.0;
	Vector<float> m_lastGauges;
public:
	bool RunningChallenge() { return m_running; }
	bool StartChallenge(class ChallengeSelect* sel, ChallengeIndex* chal);
	friend class Game;
	void ReportScore(Game*, ClearMark);
	const ChallengeOptions& GetCurrentOptions() { return m_currentOptions; }
	bool ReturnToSelect();
	const Vector<ChallengeResult>& GetResults() { return m_results; }
	const OverallChallengeResult& GetOverallResults() { return m_overallResults; }
	const Vector<ChartIndex*>& GetCharts() { return m_chal->charts; }
	ChallengeIndex* GetChallenge() { return m_chal; }
	ChallengeResult& GetCurrentResultForUpdating() { return m_results[m_chartIndex]; }


private:
	ChallengeOption<uint32> m_getOptionAsPositiveInteger(
		nlohmann::json reqs, String name, int64 min=0, int64 max=INT_MAX);

	ChallengeOption<float> m_getOptionAsFloat(
		nlohmann::json reqs, String name, float min=-INFINITY, float max=INFINITY);

	ChallengeOption<bool> m_getOptionAsBool(
		nlohmann::json reqs, String name);

	ChallengeRequirements m_processReqs(nlohmann::json req);
	ChallengeOptions m_processOptions(nlohmann::json req);

	bool m_setupNextChart();
	bool m_finishedAllCharts(bool passed);
};

struct ChallengeSelectIndex
{
private:
	ChallengeIndex* m_challenge;
	// TODO folder?
public:
	ChallengeSelectIndex() = default;
	ChallengeSelectIndex(ChallengeIndex* chal)
		: m_challenge(chal), id(chal->id)
	{
	}

	int32 id;
	ChallengeIndex* GetChallenge() const { return m_challenge; }
};

class ChallengeSelect : public IAsyncLoadableApplicationTickable
{
protected:
	ChallengeSelect() = default;

	friend class ChallengeManager;
	virtual MapDatabase* GetMapDatabase() = 0;
public:
	virtual ~ChallengeSelect() = default;
	static ChallengeSelect* Create();

	virtual ChallengeIndex* GetCurrentSelectedChallenge() { return 0; }
};
