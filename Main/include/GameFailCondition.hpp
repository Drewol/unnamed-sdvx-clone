#pragma once

class Scoring;

class GameFailCondition
{
public:
	virtual ~GameFailCondition() = default;
	enum class Type
	{
		None, Score, Grade, Miss, MissAndNear, Gauge
	};

	const static char* TYPE_STR[6];

	GameFailCondition() noexcept = default;
	virtual bool IsFailed(const Scoring&) const { return false; }
	virtual Type GetType() const { return Type::None; }
	virtual int GetThreshold() const { return 0; }
	virtual String GetDescription() const { return ""; }

	const char* GetName() const { return TYPE_STR[static_cast<unsigned int>(GetType())]; }

public:
	static std::unique_ptr<GameFailCondition> CreateGameFailCondition(uint32 type, uint32 threshold);

	// Fails when the max possible score is lower than the given value
	class Score;
	// Fails when the max possible grade is lower than the given value
	class Grade;
	// Failes when the gauge is less than the given value
	class Gauge;
	// Fails when the current # of misses is above the given value
	class MissCount;
	// Fails when the current # of misses + nears is above the given value
	class MissAndNearCount;
};

class GameFailCondition::Score : public GameFailCondition
{
public:
	Score(uint32 score) noexcept : m_score(score) {}
	bool IsFailed(const Scoring&) const override;
	Type GetType() const override { return Type::Score; }
	String GetDescription() const override { return Utility::Sprintf("Score at least %d", m_score); }

	int GetThreshold() const override { return m_score; }

protected:
	uint32 m_score;
};

class GameFailCondition::Grade : public GameFailCondition
{
public:
	Grade(GradeMark grade) noexcept : m_grade(grade) {}
	bool IsFailed(const Scoring&) const override;
	Type GetType() const override { return Type::Grade; }
	String GetDescription() const override { return Utility::Sprintf("Grade at least %s", ToDisplayString(m_grade)); }

	int GetThreshold() const override { return static_cast<int>(m_grade); }

protected:
	GradeMark m_grade;
};

class GameFailCondition::Gauge : public GameFailCondition
{
public:
	Gauge(uint32 gauge) noexcept : m_gauge(gauge) {}
	bool IsFailed(const Scoring&) const override;
	Type GetType() const override { return Type::Gauge; }
	String GetDescription() const override { return Utility::Sprintf("Gauge at least %d%%", m_gauge); }

	int GetThreshold() const override { return static_cast<int>(m_gauge); }

protected:
	uint32 m_gauge;
};

class GameFailCondition::MissCount : public GameFailCondition
{
public:
	MissCount(uint32 count) : m_count(count) {}
	bool IsFailed(const Scoring&) const override;
	Type GetType() const override { return Type::Miss; }
	String GetDescription() const override {
		if (m_count == 0) return "No misses";
		if (m_count == 1) return "At most one miss";
		return Utility::Sprintf("At most %d misses", m_count);
	}

	int GetThreshold() const override { return static_cast<int>(m_count); }

protected:
	uint32 m_count;
};

class GameFailCondition::MissAndNearCount : public GameFailCondition
{
public:
	MissAndNearCount(uint32 count) : m_count(count) {}
	bool IsFailed(const Scoring&) const override;
	Type GetType() const override { return Type::MissAndNear; }
	String GetDescription() const override {
		if (m_count == 0) return "No misses or nears";
		if (m_count == 1) return "At most one miss or near";
		return Utility::Sprintf("At most %d misses or nears", m_count);
	}

	int GetThreshold() const override { return static_cast<int>(m_count); }

protected:
	uint32 m_count;
};
