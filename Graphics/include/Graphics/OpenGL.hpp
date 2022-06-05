#pragma once
#include <Graphics/GL.hpp>
#include <Graphics/Window.hpp>

namespace Graphics
{
#ifdef _WIN32
	void __stdcall GLDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
#else
	void GLDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
#endif

	/*
		OpenGL context wrapper with common functionality
	*/
	using Shared::Recti;
	class OpenGL
	{
		class ShaderRes* m_activeShaders[3] = { 0 };
		uint32 m_mainProgramPipeline;
		class OpenGL_Impl* m_impl;
		Window* m_window;

		friend class ShaderRes;
		friend class TextureRes;
		friend class MeshRes;
		friend class Shader_Impl;
		friend class RenderQueue;

	public:
		OpenGL();
		virtual ~OpenGL();
		void InitResourceManagers();
		bool Init(Window& window, uint32 antialiasing);

		Recti GetViewport() const;
		uint32 GetFramebufferHandle();
		void SetViewport(Vector2i size);
		void SetViewport(Recti vp);
		void MakeCurrent();
		void ReleaseCurrent();

		// Check if the calling thread is the thread that runs this OpenGL context
		bool IsOpenGLThread() const;

		virtual void SwapBuffers();
	};
}
