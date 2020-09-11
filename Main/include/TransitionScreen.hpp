#pragma once
#include "ApplicationTickable.hpp"
#include "AsyncLoadable.hpp"

/*
	Loading and transition screen
*/
class TransitionScreen : public IApplicationTickable
{
public:
	virtual ~TransitionScreen() = default;
	static TransitionScreen* Create();
	virtual void TransitionTo(class Game* next) = 0;
	virtual void TransitionTo(IAsyncLoadableApplicationTickable* next, bool noCancel = true) = 0;
	virtual void RemoveOnComplete(DelegateHandle handle) = 0;
	virtual void RemoveAllOnComplete(void* handle) = 0;

	// Called when target screen is loaded
	//	can also give a null pointer if the screen didn't load successfully
	Delegate<IAsyncLoadableApplicationTickable*> OnLoadingComplete;
};