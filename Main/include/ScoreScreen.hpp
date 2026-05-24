#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"
#include "json.hpp"

class MultiplayerScreen;

class ScoreScreen : public IAsyncLoadableApplicationTickable
{
public:
	virtual ~ScoreScreen() = default;
	[[nodiscard]]
	static ScoreScreen* Create(class Game* game);
	[[nodiscard]]
	static ScoreScreen* Create(class Game* game, class ChallengeManager*);
	[[nodiscard]]
	static ScoreScreen* Create(class Game* game, String uid,
            Vector<nlohmann::json> const*, MultiplayerScreen*);
};
