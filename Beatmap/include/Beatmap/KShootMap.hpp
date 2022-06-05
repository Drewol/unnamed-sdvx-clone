#pragma once

using Utility::Sprintf;

struct KShootTickSetting
{
	String first;
	String second;
};

/*
	Any division inside a KShootBlock
*/
class KShootTick
{
public:
	String ToString() const;
	void Clear();

	Vector<KShootTickSetting> settings;

	// Original data for this tick
	String buttons, fx, laser, add;
};

/*
	A single bar in the map file
*/
class KShootBlock
{
public:
	Vector<KShootTick> ticks;
};
class KShootTime
{
public:
	KShootTime();;
	KShootTime(uint32_t block, uint32_t tick);;
	operator bool() const;
	uint32_t block;
	uint32_t tick;
};



struct KShootEffectDefinition
{
	String typeName;
	Map<String, String> parameters;
};

/*
	Map class for that splits up maps in the ksh format into Ticks and Blocks
*/
class KShootMap
{
public:
	class TickIterator
	{
	public:
		TickIterator(KShootMap& map, KShootTime start = KShootTime(0, 0));
		TickIterator& operator++();
		operator bool() const;
		KShootTick& operator*();
		KShootTick* operator->();
		const KShootTime& GetTime() const;
		const KShootBlock& GetCurrentBlock() const;
	private:
		KShootMap& m_map;
		KShootBlock* m_currentBlock;
		KShootTime m_time;
	};

public:
	KShootMap();
	~KShootMap();
	bool Init(BinaryStream& input, bool metadataOnly);
	bool GetBlock(const KShootTime& time, KShootBlock*& tickOut);
	bool GetTick(const KShootTime& time, KShootTick*& tickOut);
	float TimeToFloat(const KShootTime& time) const;
	float TranslateLaserChar(char c) const;

	Map<String, String> settings;
	Vector<KShootBlock> blocks;
	Map<String, KShootEffectDefinition> filterDefines;
	Map<String, KShootEffectDefinition> fxDefines;

private:
	static const char* c_sep;

};

bool ParseKShootCourse(BinaryStream& input, Map<String, String>& settings, Vector<String>& charts);
