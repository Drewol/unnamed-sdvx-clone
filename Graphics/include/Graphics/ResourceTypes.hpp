#pragma once

namespace Graphics
{
	// Macro used to create specializations for the ResourceManagerTypes struct
#define DEFINE_RESOURCE_TYPE(_EnumMember, _Type)\
template<> struct ResourceManagerTypes<ResourceType::_EnumMember>\
{\
	typedef _Type Type;\
};

/* Enum of all graphics resource types in this library */
	enum class ResourceType
	{
		Image = 0,
		SpriteMap,
		Texture,
		Font,
		Mesh,
		Shader,
		Material,
		ParticleSystem,
		_Length
	};

	template<ResourceType E>
	struct ResourceManagerTypes
	{
		// Dummy for gcc compiler
		typedef void Type;
	};
}
