#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include <Beatmap/MapDatabase.hpp>
#include "json.hpp"

class MultiplayerScreen;

enum class GameFlags : uint32
{
	None = 0,

	Hard = 0b1,

	Mirror = 0b10,

	Random = 0b100,

	AutoBT = 0b1000,

	AutoFX = 0b10000,

	AutoLaser = 0b100000,
End};

struct ScoreReplay
{
	int32 currentScore = 0;
	int32 maxScore = 0;
	int32 nextHitStat = 0;
	Vector<SimpleHitStat> replay;
};
GameFlags operator|(const GameFlags& a, const GameFlags& b);
GameFlags operator&(const GameFlags& a, const GameFlags& b);
GameFlags operator~(const GameFlags& a);

/*
	Main game scene / logic manager
*/
class Game : public IAsyncLoadableApplicationTickable
{
protected:
	Game() = default;
public:
	virtual ~Game() = default;
	static Game* Create(ChartIndex* chart, GameFlags flags);
	static Game* Create(MultiplayerScreen*, ChartIndex* chart, GameFlags flags);
	static Game* Create(const String& mapPath, GameFlags flags);
	static GameFlags FlagsFromSettings();

public:
	// When the game is still going, false when the map is done, all ending sequences have played, etc.
	// also false when the player leaves the game
	virtual bool IsPlaying() const = 0;

	virtual class Track& GetTrack() = 0;
	virtual class Camera& GetCamera() = 0;
	virtual class BeatmapPlayback& GetPlayback() = 0;
	virtual class Scoring& GetScoring() = 0;
	// Samples of the gauge for the performance graph
	virtual float* GetGaugeSamples() = 0;
	virtual GameFlags GetFlags() = 0;
	// Map jacket image
	virtual Texture GetJacketImage() = 0;
	// Difficulty data
	virtual ChartIndex* GetChartIndex() = 0;
	// The beatmap
	virtual Ref<class Beatmap> GetBeatmap() = 0;
	// Song was manually ended
	virtual bool GetManualExit() = 0;
	virtual float GetPlaybackSpeed() = 0;
	// Get lua state
	virtual struct lua_State* GetLuaState() = 0;
	// Set demo mode
	virtual void SetDemoMode(bool value) = 0; 
	// Set song db so a random song can be selected
	virtual void SetSongDB(class MapDatabase* db) = 0;
	// The folder that contians the map
	virtual const String& GetChartRootPath() const = 0;
	// Setup and set gameplay lua
	virtual void SetInitialGameplayLua(struct lua_State* L) = 0;
	virtual void SetGameplayLua(struct lua_State* L) = 0;
	// Full path to map
	virtual const String& GetChartPath() const = 0;
	// Is this a multiplayer game
	virtual bool IsMultiplayerGame() const = 0;
};