#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include "json.hpp"

class MultiplayerScreen;
class MapDatabase;

class ScoreScreen : public IAsyncLoadableApplicationTickable
{
public:
	virtual ~ScoreScreen() = default;
	static ScoreScreen* Create(class Game* game, MapDatabase*);
	static ScoreScreen* Create(class Game* game, class ChallengeManager*, MapDatabase*);
	static ScoreScreen* Create(class Game* game, String uid,
            Vector<nlohmann::json> const*, MultiplayerScreen*, MapDatabase*);
};
