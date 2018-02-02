#pragma once

#include "ApplicationTickable.hpp"
#include "SongSelectItem.hpp"

/*
	Song select screen
*/
class SongSelect : public IApplicationTickable
{
protected:
	SongSelect() = default;
public:
	virtual ~SongSelect() = default;
	static SongSelect* Create();
};