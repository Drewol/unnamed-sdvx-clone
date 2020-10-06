#include "stdafx.h"
#include "OpenGL.hpp"
#include "Texture.hpp"
#include "Image.hpp"
#include <Graphics/ResourceManagers.hpp>



namespace Graphics
{
	class Texture_Impl : public TextureRes
	{
		uint32 m_texture = 0;
		TextureWrap m_wmode[2] = { TextureWrap::Repeat };
		TextureFormat m_format = TextureFormat::Invalid;
		Vector2i m_size;
		bool m_filter = true;
		bool m_mipFilter = true;
		bool m_mipmaps = false;
		float m_anisotropic = 1.0f;
		void* m_data = nullptr;
		OpenGL* m_gl = nullptr;

	public:
		Texture_Impl(OpenGL* gl)
		{
			m_gl = gl;
		}
		~Texture_Impl()
		{
			if(m_texture)
				glDeleteTextures(1, &m_texture);
		}
		bool Init()
		{
			glGenTextures(1, &m_texture);
			if(m_texture == 0)
				return false;
			return true;
		}
		void Init(Vector2i size, TextureFormat format) override
		{
			m_format = format;
			m_size = size;

			uint32 ifmt = -1;
			uint32 fmt = -1;
			uint32 type = -1;
			if(format == TextureFormat::D32)
			{
				#ifdef EMBEDDED
				ifmt = GL_DEPTH_COMPONENT16;
				#else
				ifmt = GL_DEPTH_COMPONENT32;
				#endif
				fmt = GL_DEPTH_COMPONENT;
				type = GL_FLOAT;
			}
			else if(format == TextureFormat::RGBA8)
			{
				ifmt = GL_RGBA8;
				fmt = GL_RGBA;
				type = GL_UNSIGNED_BYTE;
			}
			else
			{
				assert(false);
			}

			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, ifmt, size.x, size.y, 0, fmt, type, nullptr);
			glBindTexture(GL_TEXTURE_2D, 0);

			UpdateFilterState();
			UpdateWrap();
		}
		void SetFromFrameBuffer(Vector2i pos = { 0, 0 }) override
		{
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glReadBuffer(GL_BACK);
			glBindTexture(GL_TEXTURE_2D, m_texture);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pos.x, pos.y, m_size.x, m_size.y);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		void SetData(Vector2i size, void* pData) override
		{
			m_format = TextureFormat::RGBA8;
			m_size = size;
			m_data = pData;
			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_data);
			glBindTexture(GL_TEXTURE_2D, 0);

			UpdateFilterState();
			UpdateWrap();
		}
		void UpdateFilterState()
		{
			glBindTexture(GL_TEXTURE_2D, m_texture);
			if(!m_mipmaps)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
			}
			else
			{
				if(m_mipFilter)
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
				else
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
			}
			#ifndef EMBEDDED
			if(GL_TEXTURE_MAX_ANISOTROPY_EXT)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_anisotropic);
			}
			#endif
			glBindTexture(GL_TEXTURE_2D, 0);
		}
		void SetFilter(bool enabled, bool mipFiltering, float anisotropic) override
		{
			m_mipFilter = mipFiltering;
			m_filter = enabled;
			m_anisotropic = anisotropic;
			assert(m_anisotropic >= 1.0f && m_anisotropic <= 16.0f);
			UpdateFilterState();
		}
		void SetMipmaps(bool enabled) override
		{
			if(enabled)
			{
				glBindTexture(GL_TEXTURE_2D, m_texture);
				glGenerateMipmap(GL_TEXTURE_2D);
				glBindTexture(GL_TEXTURE_2D, 0);
			}
			m_mipmaps = enabled;
			UpdateFilterState();
		}
		const Vector2i& GetSize() const override
		{
			return m_size;
		}
		void Bind(uint32 index) override
		{
			glActiveTexture(GL_TEXTURE0 + index);
			glBindTexture(GL_TEXTURE_2D, m_texture);

		}
		uint32 Handle() override
		{
			return m_texture;
		}

		void SetWrap(TextureWrap u, TextureWrap v) override
		{
			m_wmode[0] = u;
			m_wmode[1] = v;
			UpdateWrap();
		}

		void UpdateWrap()
		{
			uint32 wmode[] = {
				GL_REPEAT,
				GL_MIRRORED_REPEAT,
				GL_CLAMP_TO_EDGE,
			};

			glBindTexture(GL_TEXTURE_2D, m_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode[(size_t)m_wmode[0]]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode[(size_t)m_wmode[1]]);
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		TextureFormat GetFormat() const override
		{
			return m_format;
		}
	};

	Texture TextureRes::Create(OpenGL* gl)
	{
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if(pImpl->Init())
		{
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl; pImpl = nullptr;
		}
		return Texture();
	}
	Texture TextureRes::Create(OpenGL* gl, Image image)
	{
		if(!image)
			return Texture();
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if(pImpl->Init())
		{
			pImpl->SetData(image->GetSize(), image->GetBits());
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl;
			return Texture();
		}
	}

	Texture TextureRes::CreateFromFrameBuffer(class OpenGL* gl, const Vector2i& resolution)
	{
		Texture_Impl* pImpl = new Texture_Impl(gl);
		if (pImpl->Init())
		{
			pImpl->Init(resolution, TextureFormat::RGBA8);
			pImpl->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
			pImpl->SetFromFrameBuffer();
			return GetResourceManager<ResourceType::Texture>().Register(pImpl);
		}
		else
		{
			delete pImpl; pImpl = nullptr;
		}
		return Texture();
	}

	float TextureRes::CalculateHeight(float width)
	{
		Vector2 size = GetSize();
		float aspect = size.y / size.x;
		return aspect * width;
	}

	float TextureRes::CalculateWidth(float height)
	{
		Vector2 size = GetSize();
		float aspect = size.x / size.y;
		return aspect * height;
	}



}
