#pragma once
#include "Shared/Utility.hpp"
#include "Shared/Map.hpp"
#include "Shared/Bindable.hpp"

typedef void* DelegateHandle;

/*
	Template delegate class, can have multiple registered classes that handle a call to this function
*/
template<typename... A>
class Delegate
{
	Map<void*, IFunctionBinding<void, A...>*> staticMap;
	Map<void*, Map<void*, IFunctionBinding<void, A...>*>> objectMap;
	Map<void*, IFunctionBinding<void, A...>*> lambdaMap;
public:
	~Delegate()
	{
		Clear();
	}

	// Adds an object function handler
	template<typename Class>
	void Add(Class* object, void (Class::* func)(A...))
	{
		void* id = Utility::UnionCast<void*>(func);
		auto& fmap = objectMap.FindOrAdd(object);
		assert(!fmap.Contains(id));
		fmap.Add(id, new ObjectBinding<Class, void, A...>(object, func));
	}
	// Adds a static function handler
	void Add(void (*func)(A...))
	{
		void* id = Utility::UnionCast<void*>(func);
		assert(!staticMap.Contains(id));
		staticMap.Add(id, new StaticBinding<void, A...>(func));
	}
	// Adds a lambda function as a handler for this delegate
	template<typename T> DelegateHandle AddLambda(T&& lambda)
	{
		LambdaBinding<T, void, A...>* binding = new LambdaBinding<T, void, A...>(std::forward<T>(lambda));
		void* id = binding;
		assert(!lambdaMap.Contains(id));
		lambdaMap.Add(id, binding);
		return id;
	}

	// Removes an object handler
	template<typename Class>
	void Remove(Class* object, void(Class::* func)(A...))
	{
		void* id = Utility::UnionCast<void*>(func);
		assert(objectMap.Contains(object));
		auto& fmap = objectMap[object];
		assert(fmap.Contains(id));
		fmap.erase(id);
		if (fmap.empty())
			objectMap.erase(object);
	}
	// Removes a static handler
	void Remove(void(*func)(A...))
	{
		void* id = Utility::UnionCast<void*>(func);
		assert(staticMap.Contains(id));
		delete staticMap[id];
		staticMap.erase(id);
	}
	// Removes a lambda by it's handle
	void Remove(DelegateHandle handle)
	{
		assert(lambdaMap.Contains(handle));
		delete lambdaMap[handle];
		lambdaMap.erase(handle);
	}

	// Removes all handlers belonging to a specific object
	void RemoveAll(void* object)
	{
		auto it = objectMap.find(object);
		if (it != objectMap.end())
		{
			for (auto& f : it->second)
			{
				delete f.second;
			}
			objectMap.erase(it);
		}
	}

	// Removes all handlers
	void Clear()
	{
		// Cleanup the pointers
		for (auto& h : staticMap)
		{
			delete h.second;
		}
		for (auto& h : objectMap)
		{
			for (auto& f : h.second)
			{
				delete f.second;
			}
		}
		for (auto& h : lambdaMap)
		{
			delete h.second;
		}
		staticMap.clear();
		objectMap.clear();
		lambdaMap.clear();
	}

	// Calls the delegate
	void Call(A... args)
	{
		for (auto& h : staticMap)
		{
			h.second->Call(args...);
		}
		for (auto& h : objectMap)
		{
			for (auto& f : h.second)
			{
				f.second->Call(args...);
			}
		}
		for (auto& h : lambdaMap)
		{
			h.second->Call(args...);
		}
	}

	// True if anything function is handling this delegate being called
	bool IsHandled() const
	{
		return !staticMap.empty() || !objectMap.empty() || !lambdaMap.empty();
	}
};
