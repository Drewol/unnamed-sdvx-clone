#pragma once

#include "HitStat.hpp"
#include "Beatmap/MapDatabase.hpp"



enum class TickFlags : uint8;

struct ReplayJudgement
{
	uint8 rating : 3;
	uint8 type : 5;
	uint8 lane = 0;
	int16 delta = 0;
	MapTime time = 0;

	static_assert((uint8)HitStatType::_TYPE_MAX <= (1 << 5));

	ReplayJudgement() = default;
	ReplayJudgement(const SimpleHitStat& s) :
		ReplayJudgement(s.rating, HitStatType::Unknown, s.lane, s.delta, s.time) {};
	ReplayJudgement(int8 r, HitStatType ty, int8 l, int16 d, MapTime t) :
		rating(r), type((uint8)ty), lane(l), delta(d), time(t) {};

	inline MapTime GetHitTime() const { return time + delta; }
	inline HitStatType GetType() const { return HitStatType(type); }
	inline void ToSimpleHitStat(SimpleHitStat& stat) const
	{
		stat.rating = rating;
		stat.type = type;
		stat.lane = lane;
		stat.time = time;
		stat.delta = delta;
		stat.hold = (GetType() == HitStatType::Hold) ? 1 : 0;
		stat.holdMax = stat.hold;
	}
};

static_assert(sizeof(ReplayJudgement) == 8);


template<int V>
struct ForwardCompatStruct
{
	static_assert(V > 0);
	uint8 version = 0;
	bool Version(BinaryStream& stream)
	{
		if (stream.IsWriting() && !IsInitialized())
			return false;

		version = V;
		stream << this->version;
		if (!stream.IsOk() || (stream.IsReading() && version > V))
			return false;
		return true;
	}
	void SetDone() {
		version = V;
	}
	bool IsInitialized() const
	{
		return version != 0;
	}
};

struct ReplayOffsets : ForwardCompatStruct<1>
{
	int32 global;
	int32 input;
	int32 laser;
	int32 song;
	ReplayOffsets() = default;
	ReplayOffsets(int32 g, int32 i, int32 l, int32 s) :
		global(g), input(i), laser(l), song(s) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayOffsets*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->global << t->input << t->laser << t->song;
		return stream.IsOk();
	}
};

struct ReplayChartInfo : ForwardCompatStruct<1>
{
	String hash;
	String title;
	String artist;
	String effector;
	int32 level;
	ReplayChartInfo() = default;
	ReplayChartInfo(const ChartIndex* c) :
		hash(c->hash), title(c->title), artist(c->artist),
		effector(c->effector), level(c->level) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayChartInfo*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->hash << t->title << t->artist << t->effector << t->level;
		return stream.IsOk();
	}
};

struct ReplayScoreInfo : ForwardCompatStruct<2>
{
	int32 score;
	int32 crit;
	int32 almost;
	int32 miss;
	float gauge;
	GaugeType gaugeType;
	uint32 gaugeOption;
	bool random; // TODO save random state?
	bool mirror;
	uint64 timestamp;
	String userName;
	String userId;
	uint32 chain = 0;
	uint32 hitScore = 0;
	uint32 maxHitScore = 0;
	ReplayScoreInfo() = default;
	ReplayScoreInfo(const ScoreIndex* s) :
		score(s->score), crit(s->crit), almost(s->almost), miss(s->miss),
		gauge(s->gauge), gaugeType(s->gaugeType), gaugeOption(s->gaugeOption),
		random(s->random), mirror(s->mirror),
		timestamp(s->timestamp), userName(s->userName), userId(s->userId) {};
	static bool StaticSerialize(BinaryStream& stream, ReplayScoreInfo*& t)
	{
		if (!t->Version(stream)) return false;
		stream << t->score << t->crit << t->almost << t->miss << t->gauge <<
			t->gaugeType << t->gaugeOption << t->random << t->mirror;
		if (t->version >= 2)
		{
			stream << t->chain << t->hitScore << t->maxHitScore;
		}
		stream << t->timestamp << t->userName << t->userId;
		return stream.IsOk();
	}
	bool MatchesScore(const ScoreIndex* s);
};

struct ReplayInput
{
	uint16 a;
};

#define REPLAY_VERSION 1
#define REPLAY_MAGIC 0x52435355u
#define COMPRESSED_REPLAY_MAGIC 0x504d4f43u
class Replay
{
public:
	enum class ReplayType
	{
		Legacy,
		NoInput, // No inputs loaded
		Normal
	};

	Replay() = default;
	Replay(ReplayType type) : m_type(type) {}

	bool Save(String path);
	static Replay* Load(String path, ReplayType type = ReplayType::Normal);
	static bool StaticSerialize(BinaryStream& stream, Replay*& obj);
	static bool SerializeLegacy(BinaryStream& stream, Replay*& obj);

	void AttachChartInfo(const ChartIndex* chart)
	{
		m_chartInfo = chart;
		m_chartInfo.SetDone();
	}
	const ReplayChartInfo& GetChartInfo() const { return m_chartInfo; }

	void AttachScoreInfo(ScoreIndex* score)
	{
		m_scoreIndex = score;
		m_scoreInfo = score;
		m_scoreInfo.SetDone();
	}
	ReplayScoreInfo& GetScoreInfo() { return m_scoreInfo; }

	void AttachJudgementEvents(const Vector<SimpleHitStat>& v)
	{
		if (m_initialized) return;
		m_judgementEvents.clear();
		for (auto& hs : v)
			m_judgementEvents.push_back(hs);
	}

	ReplayType GetType() const
	{
		return m_type;
	}

	void SetHitWindow(const HitWindow& window) { m_hitWindow = window; }
	const HitWindow& GetHitWindow() const { return m_hitWindow; }

	void SetOffsets(const ReplayOffsets& offs)
	{
		m_offsets = offs;
		m_offsets.SetDone();
	}
	ReplayOffsets& GetOffsets() { return m_offsets; }

	void DoneInit()
	{
		m_initialized = true;
	}

	ScoreIndex* GetScoreIndex() const { return m_scoreIndex; }

	bool IsPlaying() const { return m_isPlaying; }

	void Restart()
	{
		m_currentMaxScore = 0;
		m_currentScore = 0;
		m_nextJudgementIndex = 0;
		m_lastEvalTime = 0;
		if (m_isPlaying)
		{
			for (int i = 0; i < 8; i++) {
				m_playbackQueue[i] = std::deque<const ReplayJudgement*>();
			}
		}
	}

	uint32 CurrentMaxScore() const { return m_currentMaxScore; }

	uint32 CurrentScore() const { return m_currentScore; }

	// Put this replay into playback mode
	void InitializePlayback()
	{
		m_isPlaying = true;
		Restart();
	}

	void UpdateToTime(MapTime lastTime);

	bool HasJudgement(int lane) const
	{
		assert(m_isPlaying);
		assert(lane < 8);
		return !m_playbackQueue[lane].empty();
	}

	const Vector<ReplayJudgement>& GetJudgements() const
	{
		return m_judgementEvents;
	}

	const ReplayJudgement* PeekNextJudgement(int lane) const;

	const ReplayJudgement* PopNextJudgement(int lane, bool score = true);

	const ReplayJudgement* FindNextJudgement(int lane, MapTime future = 0) const;

	ChartIndex* FindChart(MapDatabase** databaseHandle = nullptr) const;

	String filePath = "";
protected:
	ReplayType m_type = ReplayType::Normal;

	/* Values saved to file */
	ReplayChartInfo m_chartInfo;
	ReplayScoreInfo m_scoreInfo;
	HitWindow m_hitWindow = HitWindow::NORMAL;
	ReplayOffsets m_offsets;
	Vector<ReplayJudgement> m_judgementEvents;
	Vector<ReplayInput> m_inputEvents;
	/* End file values */

	bool m_initialized = false;

	ScoreIndex* m_scoreIndex = nullptr;

	// Playback related vars
	bool m_isPlaying = false;
	int32 m_currentScore = 0;
	int32 m_currentMaxScore = 0; // May be wrong for some legacy replays
	std::deque<const ReplayJudgement*> m_playbackQueue[8];
	size_t m_nextJudgementIndex = 0;
	MapTime m_lastEvalTime = 0;

	bool m_requiresRewrite = false;
};

