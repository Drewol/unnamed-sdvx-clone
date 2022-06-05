#include "stdafx.h"
#include <Graphics/ResourceManagers.hpp>
#include "Image.hpp"
#include "Texture.hpp"

namespace Graphics
{
	static Timer gcTimer;
	static Timer cleanupTimer;
	static int disabled = 0;

	static ResourceManagers inst;
	static IResourceManager* managers[(size_t)ResourceType::_Length] = { nullptr };

	ResourceManagers::ResourceManagers()
	{
		// Create common resource managers
		CreateResourceManager<ResourceType::Image>();
		CreateResourceManager<ResourceType::SpriteMap>();
	}
	ResourceManagers::~ResourceManagers()
	{
		for (size_t i = 0; i < (size_t)ResourceType::_Length; i++)
		{
			if (managers[i])
			{
				delete managers[i];
				managers[i] = nullptr;
			}
		}
	}

	void ResourceManagers::DestroyResourceManager(ResourceType type)
	{
		size_t idx = (size_t)type;
		if (managers[idx])
		{
			managers[idx]->ReleaseAll();

			delete managers[idx];
			managers[idx] = nullptr;
		}
	}
	void ResourceManagers::AssignResourceManager(ResourceType type, IResourceManager* mgr)
	{
		size_t idx = (size_t)type;
		assert(managers[idx] == nullptr);
		managers[idx] = mgr;
	}

	IResourceManager* ResourceManagers::GetResourceManager(ResourceType type)
	{
		size_t idx = (size_t)type;
		assert(managers[idx] != nullptr);
		return managers[idx];
	}

	void ResourceManagers::SuspendGC()
	{
		disabled++;
	}
	void ResourceManagers::ContinueGC()
	{
		disabled--;
		if (disabled < 0)
			disabled = 0;
	}
	void ResourceManagers::TickAll()
	{
		inst.m_TickAll();
	}
	void ResourceManagers::m_TickAll()
	{
		if (gcTimer.Milliseconds() > 250 && disabled == 0)
		{
			for (auto rm : managers)
			{
				if (rm)
					rm->GarbageCollect();
			}
			gcTimer.Restart();
		}
	}
}
