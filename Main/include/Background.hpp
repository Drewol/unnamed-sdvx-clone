#pragma once

/*
	Game background base class
*/
class Background
{
public:
	virtual ~Background() = default;
	virtual bool Init(bool foreground) = 0;
	virtual void Render(float deltaTime) = 0;

	class Game* game;
};

// Creates the default game background
Background* CreateBackground(class Game* game, bool foreground = false);
