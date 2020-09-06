#include "stdafx.h"
#include "GameFailCondition.hpp"

#include "Scoring.hpp"

const char* GameFailCondition::TYPE_STR[6] = { "None", "Score", "Grade", "Miss", "MissAndNear", "Gauge" };

bool GameFailCondition::Score::IsFailed(const Scoring& scoring) const
{
	return scoring.CalculateCurrentMaxPossibleScore() < m_score;
}

bool GameFailCondition::Grade::IsFailed(const Scoring& scoring) const
{
	return scoring.CalculateCurrentMaxPossibleScore() < ToMinScore(m_grade);
}

bool GameFailCondition::Gauge::IsFailed(const Scoring& scoring) const
{
	if (scoring.IsPerfect()) return false;

	if (m_gauge == 0) return false;
	if (m_gauge == 100) return scoring.currentGauge < 1.0f;

	return scoring.currentGauge * 100 < m_gauge;
}

bool GameFailCondition::MissCount::IsFailed(const Scoring& scoring) const
{
	return scoring.GetMisses() > m_count;
}

bool GameFailCondition::MissAndNearCount::IsFailed(const Scoring& scoring) const
{
	return scoring.GetMisses() + scoring.GetGoods() > m_count;
}

std::unique_ptr<GameFailCondition> GameFailCondition::CreateGameFailCondition(uint32 type, uint32 threshold)
{
	switch (static_cast<Type>(type))
	{
	case Type::None:
		return nullptr;
	case Type::Score:
		return std::make_unique<Score>(threshold);
	case Type::Grade:
		return std::make_unique<Grade>(static_cast<GradeMark>(threshold));
	case Type::Miss:
		return std::make_unique<MissCount>(threshold);
	case Type::MissAndNear:
		return std::make_unique<MissAndNearCount>(threshold);
	case Type::Gauge:
		return std::make_unique<Gauge>(threshold);
	default:
		Logf("GameFailCondition::CreateGameFailCondition: unknown type %d", Logger::Severity::Warning, type);
		return nullptr;
	}
}
