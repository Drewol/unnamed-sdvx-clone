#pragma once

constexpr uint32 MAX_SCORE = 10000000;

enum class GradeMark : uint8
{
	// Basic
	D, C, B, A, Ap, AA, AAp, AAA, AAAp, S,

	// Extended
	S_995, S_998, S_999, PUC,
};

enum class ClearMark : uint8
{
	NotPlayed, Played, NormalClear, HardClear, FullCombo, Perfect,
};

extern const char* GRADE_MARK_STR[14];

inline const char* ToDisplayString(GradeMark grade)
{
	return GRADE_MARK_STR[static_cast<std::uint_fast8_t>(grade)];
}

inline uint32 ToMinScore(GradeMark grade)
{
	switch (grade)
	{
	case GradeMark::D:     return 0;
	case GradeMark::C:     return 6500000;
	case GradeMark::B:     return 7500000;
	case GradeMark::A:     return 8700000;
	case GradeMark::Ap:    return 9000000;
	case GradeMark::AA:    return 9300000;
	case GradeMark::AAp:   return 9500000;
	case GradeMark::AAA:   return 9700000;
	case GradeMark::AAAp:  return 9800000;
	case GradeMark::S:     return 9900000;
	case GradeMark::S_995: return 9950000;
	case GradeMark::S_998: return 9980000;
	case GradeMark::S_999: return 9990000;
	case GradeMark::PUC:   return MAX_SCORE;
	}

	assert(false);
	return 0;
}

inline GradeMark ToGradeMark(uint32 score)
{
	if (score >= ToMinScore(GradeMark::S))    return GradeMark::S;
	if (score >= ToMinScore(GradeMark::AAAp)) return GradeMark::AAAp;
	if (score >= ToMinScore(GradeMark::AAA))  return GradeMark::AAA;
	if (score >= ToMinScore(GradeMark::AAp))  return GradeMark::AAp;
	if (score >= ToMinScore(GradeMark::AA))   return GradeMark::AA;
	if (score >= ToMinScore(GradeMark::Ap))   return GradeMark::Ap;
	if (score >= ToMinScore(GradeMark::A))    return GradeMark::A;
	if (score >= ToMinScore(GradeMark::B))    return GradeMark::B;
	if (score >= ToMinScore(GradeMark::C))    return GradeMark::C;
	return GradeMark::D;
}

inline GradeMark ToGradeMarkExt(uint32 score)
{
	if (score >= ToMinScore(GradeMark::PUC))   return GradeMark::PUC;
	if (score >= ToMinScore(GradeMark::S_999)) return GradeMark::S_999;
	if (score >= ToMinScore(GradeMark::S_998)) return GradeMark::S_998;
	if (score >= ToMinScore(GradeMark::S_995)) return GradeMark::S_995;
	return ToGradeMark(score);
}

extern const char* CLEAR_MARK_STR[6];

inline const char* ToDisplayString(ClearMark mark)
{
	return CLEAR_MARK_STR[static_cast<std::uint_fast8_t>(mark)];
}