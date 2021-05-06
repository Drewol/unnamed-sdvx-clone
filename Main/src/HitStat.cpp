#include "stdafx.h"
#include "HitStat.hpp"
#include "GameConfig.hpp"

const HitWindow HitWindow::NORMAL = HitWindow(46, 150);
const HitWindow HitWindow::HARD = HitWindow(23, 75);

HitStat::HitStat(ObjectState* object) : object(object)
{
	time = object->time;
}
bool HitStat::operator<(const HitStat& other)
{
	return time < other.time;
}

HitWindow HitWindow::FromConfig()
{
	HitWindow hitWindow = HitWindow(
		g_gameConfig.GetInt(GameConfigKeys::HitWindowPerfect),
		g_gameConfig.GetInt(GameConfigKeys::HitWindowGood),
		g_gameConfig.GetInt(GameConfigKeys::HitWindowHold),
		g_gameConfig.GetInt(GameConfigKeys::HitWindowSlam)
	);

	if (!(hitWindow <= HitWindow::NORMAL))
	{
		Log("HitWindow is automatically adjusted to NORMAL", Logger::Severity::Warning);
		hitWindow = HitWindow::NORMAL;
		hitWindow.SaveConfig();
	}

	return hitWindow;
}

void HitWindow::SaveConfig() const
{
	g_gameConfig.Set(GameConfigKeys::HitWindowPerfect, perfect);
	g_gameConfig.Set(GameConfigKeys::HitWindowGood, good);
	g_gameConfig.Set(GameConfigKeys::HitWindowHold, hold);
	g_gameConfig.Set(GameConfigKeys::HitWindowSlam, slam);
}

void HitWindow::ToLuaTable(lua_State* L) const
{
	auto pushIntToTable = [&](const char* name, int data)
	{
		lua_pushstring(L, name);
		lua_pushinteger(L, data);
		lua_settable(L, -3);
	};

	lua_newtable(L);
	pushIntToTable("type", static_cast<int>(GetType()));
	pushIntToTable("perfect", perfect);
	pushIntToTable("good", good);
	pushIntToTable("hold", hold);
	pushIntToTable("miss", miss);
	pushIntToTable("slam", slam);
}
