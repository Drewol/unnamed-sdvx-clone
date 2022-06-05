#pragma once
#include <Graphics/ResourceTypes.hpp>
#include <Shared/ResourceManager.hpp>

namespace Graphics
{
	/*
		Holds all graphics resource managers used
	*/
	class ResourceManagers
	{
	private:
		void m_TickAll();

	public:
		ResourceManagers();
		~ResourceManagers();
		static void DestroyResourceManager(ResourceType type);
		template<ResourceType E>
		static void DestroyResourceManager()
		{
			DestroyResourceManager(E);
		}

		template<ResourceType E>
		static void CreateResourceManager()
		{
			AssignResourceManager(E, new ResourceManager<typename ResourceManagerTypes<E>::Type>());
		}
		static void AssignResourceManager(ResourceType type, IResourceManager* mgr);

		template<ResourceType E>
		static ResourceManager<typename ResourceManagerTypes<E>::Type>& GetResourceManager()
		{
			return *(ResourceManager<typename ResourceManagerTypes<E>::Type>*)GetResourceManager(E);
		}
		static IResourceManager* GetResourceManager(ResourceType type);

		static void SuspendGC();
		static void ContinueGC();
		static void TickAll();

	private:
	};

	template<ResourceType E>
	ResourceManager<typename ResourceManagerTypes<E>::Type>& GetResourceManager()
	{
		return ResourceManagers::GetResourceManager<E>();
	}
}
