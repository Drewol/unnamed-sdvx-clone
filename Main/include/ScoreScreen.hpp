#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include "json.hpp"

class MultiplayerScreen;

class ScoreScreen : public IAsyncLoadableApplicationTickable
{
public:
	virtual ~ScoreScreen() = default;
	static ScoreScreen* Create(class Game* game);
	static ScoreScreen* Create(class Game* game, class ChallengeManager*);
	static ScoreScreen* Create(class Game* game, String uid,
		Vector<nlohmann::json> const*, MultiplayerScreen*);
};
