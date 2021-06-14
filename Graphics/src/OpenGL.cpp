#include "stdafx.h"
#include "OpenGL.hpp"
#include <Graphics/ResourceManagers.hpp>
#ifdef _MSC_VER
#pragma comment(lib, "opengl32.lib")
#endif

#include "Mesh.hpp"
#include "Texture.hpp"
#include "Shader.hpp"
#include "Font.hpp"
#include "Material.hpp"
#include "ParticleSystem.hpp"
#include "Window.hpp"
#include <Shared/Thread.hpp>

namespace Graphics
{
	class OpenGL_Impl
	{
	public:
		SDL_GLContext context;
		std::thread::id threadId;
	};

	OpenGL::OpenGL()
	{
		m_impl = new OpenGL_Impl();
	}
	OpenGL::~OpenGL()
	{
		if(m_impl->context)
		{
			// Cleanup resource managers
			ResourceManagers::DestroyResourceManager<ResourceType::Mesh>();
			ResourceManagers::DestroyResourceManager<ResourceType::Texture>();
			ResourceManagers::DestroyResourceManager<ResourceType::Shader>();
			ResourceManagers::DestroyResourceManager<ResourceType::Font>();
			ResourceManagers::DestroyResourceManager<ResourceType::Material>();
			ResourceManagers::DestroyResourceManager<ResourceType::ParticleSystem>();

#ifndef EMBEDDED
			if(m_mainProgramPipeline)
			{
				glDeleteProgramPipelines(1, &m_mainProgramPipeline);
			}
#endif

			SDL_GL_DeleteContext(m_impl->context);
			m_impl->context = nullptr;
		}
		delete m_impl;
	}
	void OpenGL::InitResourceManagers()
	{
		ResourceManagers::CreateResourceManager<ResourceType::Mesh>();
		ResourceManagers::CreateResourceManager<ResourceType::Texture>();
		ResourceManagers::CreateResourceManager<ResourceType::Shader>();
		ResourceManagers::CreateResourceManager<ResourceType::Font>();
		ResourceManagers::CreateResourceManager<ResourceType::Material>();
		ResourceManagers::CreateResourceManager<ResourceType::ParticleSystem>();
	}
	bool OpenGL::Init(Window& window, uint32 antialiasing)
	{
		if(m_impl->context)
			return true; // Already initialized

		// Store the thread ID that the OpenGL context runs on
		m_impl->threadId = std::this_thread::get_id();

		m_window = &window;
		SDL_Window* sdlWnd = (SDL_Window*)m_window->Handle();

#ifdef EMBEDDED
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
		// Create a context
		m_impl->context = SDL_GL_CreateContext(sdlWnd);
		if(!m_impl->context)
		{
            Logf("Failed to create OpenGL context: %s", Logger::Severity::Error, SDL_GetError());
            return false;
		}

		if (SDL_GL_MakeCurrent(sdlWnd, m_impl->context) < 0)
		{
			Logf("Failed to set OpenGL context to current: %s", Logger::Severity::Error, SDL_GetError());
			return false;
		}

		//windows always needs glew
#ifdef _WIN32
		glewExperimental = true;
		glewInit();
#else

		// macOS and embedded doesnt need glew
		#ifndef EMBEDDED
		#ifndef __APPLE__
		// To allow usage of experimental features
		glewExperimental = true;
		glewInit();
		#endif
		#endif
#endif

		//#define LIST_OGL_EXTENSIONS
#ifdef LIST_OGL_EXTENSIONS
		Logf("Listing OpenGL Extensions:", Logger::Info);
		GLint n, i;
		glGetIntegerv(GL_NUM_EXTENSIONS, &n);
		for(i = 0; i < n; i++) {
			Logf("%s", Logger::Info, glGetStringi(GL_EXTENSIONS, i));
		}
#endif

#ifdef _DEBUG
		// Setup GL debug messages to go to the console
		if(glDebugMessageCallback && glDebugMessageControl)
		{
			Log("OpenGL Logging on.", Logger::Severity::Info);
			glDebugMessageCallback(GLDebugProc, 0);
			glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, 0, GL_TRUE);
			glDebugMessageControl(GL_DONT_CARE, GL_DEBUG_TYPE_OTHER, GL_DONT_CARE, 0, 0, GL_FALSE);
		}
#endif

		Logf("OpenGL Version: %s", Logger::Severity::Info, glGetString(GL_VERSION));
		Logf("OpenGL Shading Language Version: %s", Logger::Severity::Info, glGetString(GL_SHADING_LANGUAGE_VERSION));
		Logf("OpenGL Renderer: %s", Logger::Severity::Info, glGetString(GL_RENDERER));
		Logf("OpenGL Vendor: %s", Logger::Severity::Info, glGetString(GL_VENDOR));

		InitResourceManagers();

#ifndef EMBEDDED
		// Create pipeline for the program
		glGenProgramPipelines(1, &m_mainProgramPipeline);
		glBindProgramPipeline(m_mainProgramPipeline);
		glEnable(GL_TEXTURE_2D);
		glEnable(GL_MULTISAMPLE);
#endif

		glDisable(GL_DEPTH_TEST);
		glEnable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glEnable(GL_STENCIL_TEST);
		return true;
	}


	Recti OpenGL::GetViewport() const
	{
		Recti vp;
		glGetIntegerv(GL_VIEWPORT, &vp.pos.x);
		return vp;
	}
	uint32 OpenGL::GetFramebufferHandle()
	{
		return GL_BACK;
	}

	void OpenGL::SetViewport(Recti vp)
	{
		glViewport(vp.pos.x, vp.pos.y, vp.size.x, vp.size.y);
	}
	void OpenGL::SetViewport(Vector2i size)
	{
		glViewport(0, 0, size.x, size.y);
	}
	bool OpenGL::IsOpenGLThread() const
	{
		return m_impl->threadId == std::this_thread::get_id();
	}

	void OpenGL::SwapBuffers()
	{
		glFlush();
		SDL_Window* sdlWnd = (SDL_Window*)m_window->Handle();
		SDL_GL_SwapWindow(sdlWnd);
	}

	#ifdef _WIN32
	void APIENTRY GLDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	#else
	void GLDebugProc(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
	#endif
	{
#define DEBUG_SEVERITY_HIGH                              0x9146
#define DEBUG_SEVERITY_MEDIUM                            0x9147
#define DEBUG_SEVERITY_LOW                               0x9148
#define DEBUG_SEVERITY_NOTIFICATION                      0x826B

		Logger::Severity mySeverity;
		switch(severity)
		{
		case DEBUG_SEVERITY_MEDIUM:
		case DEBUG_SEVERITY_HIGH:
			mySeverity = Logger::Severity::Warning;
			break;
		default:
			mySeverity = Logger::Severity::Info;
			break;
		}
		String msgString = String(message, message + length);
		Logf("GLDebug: %s", mySeverity, msgString);
	}
}
