#pragma once
#include "ApplicationTickable.hpp"

/*
	Used for assets that can be loaded asynchronously
	the interface provides a Load and Finalize function
	the Load function is called on another thread than the main thread
	the Finalize function is called on the main thread once the load function completed (only if succeeded)
*/
class IAsyncLoadable
{
public:
	virtual ~IAsyncLoadable() = default;
	// Loads this object
	//	returns success
	virtual bool AsyncLoad() = 0;
	// Finalize loading of this object on the main thread
	//	for example, any OpenGL stuff
	//	returns success
	virtual bool AsyncFinalize() = 0;
};

// Both an application tickable and async loadable
class IAsyncLoadableApplicationTickable : public IAsyncLoadable, public IApplicationTickable
{
};
