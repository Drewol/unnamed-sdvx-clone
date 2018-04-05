#include "stdafx.h"
#include "Framebuffer.hpp"
#include "Texture.hpp"
#include <Graphics/ResourceManagers.hpp>
#include "OpenGL.hpp"

namespace Graphics
{
	class Framebuffer_Impl : public FramebufferRes
	{
		OpenGL* m_gl;
		uint32 m_fb = 0;
		Vector2i m_textureSize;
		bool m_isBound = false;
		bool m_depthAttachment = false;

		friend class OpenGL;
	public:
		Framebuffer_Impl(OpenGL* gl) : m_gl(gl)
		{
		}
		~Framebuffer_Impl()
		{
			if(m_fb > 0)
				glDeleteFramebuffers(1, &m_fb);
			assert(!m_isBound);
		}
		bool Init()
		{
			glGenFramebuffers(1, &m_fb);
			return m_fb != 0;
		}
		virtual bool AttachTexture(Texture tex)
		{
			if(!tex)
				return false;
			m_textureSize = tex->GetSize();
			uint32 texHandle = (uint32)tex->Handle();
			TextureFormat fmt = tex->GetFormat();
			if(fmt == TextureFormat::D32)
			{
				#ifdef __APPLE__
				glFramebufferTexture2D(m_fb, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texHandle, 0);
				#else
				glNamedFramebufferTexture2DEXT(m_fb, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, texHandle, 0);
				#endif

				m_depthAttachment = true;
			}
			else
			{
				#ifdef __APPLE__
				glFramebufferTexture2D(m_fb, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texHandle, 0);
				#else
				glNamedFramebufferTexture2DEXT(m_fb, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texHandle, 0);
				#endif
			}

			return IsComplete();
		}
		virtual void Bind()
		{
			assert(!m_isBound);

			// Adjust viewport to frame buffer
			//glGetIntegerv(GL_VIEWPORT, &m_gl->m_lastViewport.pos.x);
			//glViewport(0, 0, m_textureSize.x, m_textureSize.y);
			glBindFramebuffer(GL_FRAMEBUFFER, m_fb);

			if(m_depthAttachment)
			{
				GLenum drawBuffers[2] =
				{
					GL_COLOR_ATTACHMENT0,
					GL_DEPTH_ATTACHMENT
				};
				glDrawBuffers(2, drawBuffers);
			}
			else
			{
				glDrawBuffer(GL_COLOR_ATTACHMENT0);
			}

			m_isBound = true;
			m_gl->m_boundFramebuffer = this;
		}
		virtual void Unbind()
		{
			assert(m_isBound && m_gl->m_boundFramebuffer == this);

			// Restore viewport
			//Recti& vp = m_gl->m_lastViewport;
			//glViewport(vp.pos.x, vp.pos.y, vp.size.x, vp.size.y);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDrawBuffer(GL_BACK);

			m_isBound = false;
			m_gl->m_boundFramebuffer = nullptr;
		}
		virtual bool IsComplete() const
		{
			#ifdef __APPLE__
			int complete = glCheckFramebufferStatus(m_fb);
			#else
			int complete = glCheckNamedFramebufferStatus(m_fb, GL_DRAW_FRAMEBUFFER);
			#endif	

			return complete == GL_FRAMEBUFFER_COMPLETE;
		}
		virtual uint32 Handle() const
		{
			return m_fb;
		}
	};

	Framebuffer FramebufferRes::Create(class OpenGL* gl)
	{
		Framebuffer_Impl* pImpl = new Framebuffer_Impl(gl);
		if(!pImpl->Init())
		{
			delete pImpl;
			return Framebuffer();
		}
		else
		{
			return GetResourceManager<ResourceType::Framebuffer>().Register(pImpl);
		}
	}
}