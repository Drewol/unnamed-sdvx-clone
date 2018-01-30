#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include <Beatmap/MapDatabase.hpp>

/*
	Main editor scene / logic manager
*/
class Editor : public IAsyncLoadableApplicationTickable
{
protected:
	Editor() = default;
public:
	virtual ~Editor() = default;
	static Editor* Create(const DifficultyIndex& difficulty);
	static Editor* Create(const String& chartPath);

	virtual bool IsPlaying() const = 0;

	virtual class BeatmapPlayback& GetPlayback() = 0;
	// Difficulty data
	virtual const DifficultyIndex& GetDifficultyIndex() const = 0;
	// The beatmap
	virtual Ref<class Beatmap> GetBeatmap() = 0;
	// The folder that contians the map
	virtual const String& GetMapRootPath() const = 0;
	// Full path to map
	virtual const String& GetMapPath() const = 0;
};

