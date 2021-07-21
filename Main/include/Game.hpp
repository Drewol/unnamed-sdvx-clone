#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include <Beatmap/MapDatabase.hpp>
#include "json.hpp"
#include "GameFailCondition.hpp"
#include "HitStat.hpp"

class MultiplayerScreen;
class ChallengeManager;

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
	int32 currentScore = 0; //< Current score; updated during playback
	int32 currentMaxScore = 0; //< Current max possible score; updated during playback
	int32 maxScore = 0;
	size_t nextHitStat = 0;
	Vector<SimpleHitStat> replay;

	HitWindow hitWindow = HitWindow::NORMAL;
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
	struct PlayOptions;

	virtual ~Game() = default;
	static Game* Create(ChartIndex* chart, PlayOptions&& options);
	static Game* Create(MultiplayerScreen*, ChartIndex* chart, PlayOptions&& options);
	static Game* Create(ChallengeManager*, ChartIndex* chart, PlayOptions&& options);
	static Game* Create(const String& mapPath, PlayOptions&& options);
	static Game* CreatePractice(ChartIndex* chart, PlayOptions&& options);
	static PlaybackOptions PlaybackOptionsFromSettings();

	struct PlayOptions
	{
		PlayOptions() {}

		// Implicitly used for normal gameplay
		PlayOptions(PlaybackOptions playbackOptions) : playbackOptions(playbackOptions) {}
		PlayOptions(PlayOptions&&) = default;
		
		bool loopOnSuccess = false;
		bool loopOnFail = false;

		MapTimeRange range = { 0, 0 };
		PlaybackOptions playbackOptions;

		float playbackSpeed = 1.0f;

		bool incSpeedOnSuccess = false;
		float incSpeedAmount = 0.02f;
		int incStreak = 1;

		bool decSpeedOnFail = false;
		float decSpeedAmount = 0.02f;
		float minPlaybackSpeed = 0.50f;

		bool enableMaxRewind = false;
		int maxRewindMeasure = 1;

		std::unique_ptr<GameFailCondition> failCondition = nullptr;
	};

public:
	// When the game is still going, false when the map is done, all ending sequences have played, etc.
	// also false when the player leaves the game
	virtual bool IsPlaying() const = 0;

	virtual class Track& GetTrack() = 0;
	virtual class Camera& GetCamera() = 0;
	virtual class BeatmapPlayback& GetPlayback() = 0;
	virtual class Scoring& GetScoring() = 0;
	// Samples of the gauge for the performance graph
	virtual const std::array<float, 256>& GetGaugeSamples() = 0;
	virtual PlaybackOptions GetPlaybackOptions() = 0;
	// Map jacket image
	virtual Texture GetJacketImage() = 0;
	// Difficulty data
	virtual ChartIndex* GetChartIndex() = 0;
	// The beatmap
	virtual Ref<class Beatmap> GetBeatmap() = 0;
	
	// Whether the score can be scored
	// (Full playthrough, playback speed at least x1)
	virtual bool IsStorableScore() = 0;

	// Current playback speed
	// Warning: this returns 0 when the song is not playing (ex: end of the game).
	virtual float GetPlaybackSpeed() = 0;

	virtual const PlayOptions& GetPlayOptions() const = 0;

	virtual class LuaBindable* MakeTrackLuaBindable(struct lua_State* L) = 0;

	// Get lua state
	virtual struct lua_State* GetLuaState() = 0;
	// Set demo mode
	virtual void SetDemoMode(bool value) = 0; 
	// Set song db for random song selection and practice mode setups
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

	virtual int GetRetryCount() const = 0;
	virtual String GetMissionStr() const = 0;

	virtual void SetGauge(float) = 0;

	virtual void SetAllGaugeValues(const Vector<float> values) = 0;

	virtual void PermanentlyHideTickObject(MapTime t, int lane) = 0;
};