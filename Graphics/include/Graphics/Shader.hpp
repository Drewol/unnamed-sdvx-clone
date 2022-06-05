#pragma once
#include <Graphics/ResourceTypes.hpp>

namespace Graphics
{
#ifndef EMBEDDED
	extern const uint32 shaderStageMap[];
#endif
	/* Enum of supported shader types */
	enum class ShaderType
	{
		Vertex,
		Fragment,
		Geometry
	};

	/*
		A single unlinked OpenGL shader
	*/
	class ShaderRes
	{
	public:
		virtual ~ShaderRes() = default;
		static Ref<ShaderRes> Create(class OpenGL* gl, ShaderType type, const String& assetPath);
		static void Unbind(class OpenGL* gl, ShaderType type);
		friend class OpenGL;
	public:
		// Tries to hot-reload the shader program, only works if _DEBUG is defined
		// returns true if the program was changed and thus the handle value also changed
		virtual bool UpdateHotReload() = 0;
#ifndef EMBEDDED
		virtual void Bind() = 0;
		virtual bool IsBound() const = 0;
		virtual uint32 GetLocation(const String& name) const = 0;
		uint32 operator[](const char* name) const
		{
			return GetLocation(name);
		}
#endif
		virtual uint32 Handle() = 0;

		virtual String GetOriginalName() const = 0;
	};

	typedef Ref<ShaderRes> Shader;

	DEFINE_RESOURCE_TYPE(Shader, ShaderRes)
}
