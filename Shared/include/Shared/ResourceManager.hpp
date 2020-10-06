#pragma once
#include "Shared/String.hpp"
#include "Shared/Vector.hpp"
#include "Shared/Ref.hpp"
#include "Shared/TypeInfo.hpp"
#include "Shared/Unique.hpp"
#include "Shared/Log.hpp"
#include "Shared/Thread.hpp"

class IResourceManager
{
public:
	// Collects unused object
	virtual void GarbageCollect() = 0;
	// Forcefully releases all objects from this resource manager
	virtual void ReleaseAll() = 0;
	virtual ~IResourceManager() = default;
};

/*
	Templated resource managed that keeps Ref<> objects
	the GarbageCollect function checks these and cleans up unused ones
*/
template<typename T>
class ResourceManager : public IResourceManager, Unique
{
	// List of managed object
	Vector<Ref<T>> m_objects;
	Mutex m_lock;
public:
	ResourceManager()
	{
	}
	~ResourceManager()
	{
	}
	// Creates a new reference counted object to this object and returns it
	// when the object is no longer referenced the resource manager will collect it when the next garbage collection triggers
	const Ref<T> Register(T* pObject)
	{
		Ref<T> ret(pObject);
		m_lock.lock();
		m_objects.push_back(ret);
		m_lock.unlock();
		return ret;
	}
	virtual void GarbageCollect() override
	{
		size_t numCleanedUp = 0;
		m_lock.lock();
		for(auto it = m_objects.begin(); it != m_objects.end();)
		{
			if(it->use_count() <= 1)
			{
				numCleanedUp++;
				it = m_objects.erase(it);
				continue;
			}
			it++;
		}
		m_lock.unlock();
		if(numCleanedUp > 0)
		{
			//Logf("Cleaned up %d resource(s) of %s", Logger::Info, numCleanedUp, Utility::TypeInfo<T>::name);
		}
	}
	void ReleaseAll() override
	{
		m_lock.lock();
		size_t numCleanedUp = m_objects.size();
		for(auto it = m_objects.begin(); it != m_objects.end(); it++)
		{
			if(*it)
				it->reset();
		}
		m_objects.clear();
		m_lock.unlock();
		if(numCleanedUp > 0)
		{
			Logf("Cleaned up %d resource(s) of %s", Logger::Severity::Info, numCleanedUp, Utility::TypeInfo<T>::name);
		}
	}
};