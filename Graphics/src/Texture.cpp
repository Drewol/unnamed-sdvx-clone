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

	public:
		Texture_Impl()
		{
		}
		~Texture_Impl()
		{
			if(m_texture)
				glDeleteTextures(1, &m_texture);
		}
		bool Init()
		{
			#ifdef __APPLE__
			glGenTextures(1, &m_texture);
			#else
			if(glCreateTextures)
				glCreateTextures(GL_TEXTURE_2D, 1, &m_texture);
			else
				glGenTextures(1, &m_texture);
			#endif

			if(m_texture == 0)
				return false;
			return true;
		}
		virtual void Init(Vector2i size, TextureFormat format)
		{
			m_format = format;
			m_size = size;

			uint32 ifmt = -1;
			uint32 fmt = -1;
			uint32 type = -1;
			if(format == TextureFormat::D32)
			{
				ifmt = GL_DEPTH_COMPONENT32;
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

			#ifdef __APPLE__
			Bind(m_texture);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, size.x, size.y);
			#else
			if(glTextureStorage2D)
				glTextureStorage2D(m_texture, 1, GL_RGBA8, size.x, size.y);
			else
				glTextureImage2DEXT(m_texture, GL_TEXTURE_2D, 0, ifmt, size.x, size.y, 0, fmt, type, nullptr);
			#endif

			UpdateFilterState();
			UpdateWrap();
		}
		virtual void SetData(Vector2i size, void* pData)
		{
			m_format = TextureFormat::RGBA8;
			m_size = size;
			m_data = pData;

			#ifdef __APPLE__
			Bind(m_texture);
			glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, m_size.x, m_size.y);
			glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, pData);
			#else
			if (glTextureStorage2D)
			{
				glTextureStorage2D(m_texture, 1, GL_RGBA8, m_size.x, m_size.y);
				glTextureSubImage2D(m_texture, 0, 0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, pData);
			}
			else
			{
				glTextureImage2DEXT(m_texture, GL_TEXTURE_2D, 0, GL_RGBA8, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pData);
			}
			#endif

			UpdateFilterState();
			UpdateWrap();
		}
		void UpdateFilterState()
		{
			#ifdef __APPLE__
			Bind(m_texture);
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
			if(GL_TEXTURE_MAX_ANISOTROPY_EXT)
			{
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_anisotropic);
			}
			#else
			if(glTextureParameteri)
			{
				if(!m_mipmaps)
				{
					glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
					glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				}
				else
				{
					if(m_mipFilter)
						glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
					else
						glTextureParameteri(m_texture, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
					glTextureParameteri(m_texture, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				}
				if(GL_TEXTURE_MAX_ANISOTROPY_EXT)
				{
					glTextureParameteri(m_texture, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_anisotropic);
				}
			}
			else /* Sigh */
			{
				if (!m_mipmaps)
				{
					glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
					glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				}
				else
				{
					if (m_mipFilter)
						glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_LINEAR);
					else
						glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filter ? GL_LINEAR_MIPMAP_NEAREST : GL_NEAREST_MIPMAP_NEAREST);
					glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filter ? GL_LINEAR : GL_NEAREST);
				}
				if (GL_TEXTURE_MAX_ANISOTROPY_EXT)
				{
					glTextureParameterfEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, m_anisotropic);
				}
			}
			#endif
		}
		virtual void SetFilter(bool enabled, bool mipFiltering, float anisotropic)
		{
			m_mipFilter = mipFiltering;
			m_filter = enabled;
			m_anisotropic = anisotropic;
			assert(m_anisotropic >= 1.0f && m_anisotropic <= 16.0f);
			UpdateFilterState();
		}
		virtual void SetMipmaps(bool enabled)
		{
			if(enabled)
			{
				#ifdef __APPLE__
				Bind(m_texture);
				glGenerateMipmap(GL_TEXTURE_2D);
				#else
				if(glGenerateTextureMipmap)
					glGenerateTextureMipmap(m_texture);
				else
					glGenerateTextureMipmapEXT(m_texture, GL_TEXTURE_2D);
				#endif
			}
			m_mipmaps = enabled;
			UpdateFilterState();
		}
		virtual const Vector2i& GetSize() const
		{
			return m_size;
		}
		virtual void Bind(uint32 index)
		{
			#ifdef __APPLE__
			glBindTexture(GL_TEXTURE_2D, m_texture);
			#else
			if (glBindTextureUnit)
			{
				glBindTextureUnit(index, m_texture);
			}
			else
			{
				glActiveTexture(GL_TEXTURE0 + index);
				glBindTexture(GL_TEXTURE_2D, m_texture);
			}
			#endif
		}
		virtual uint32 Handle()
		{
			return m_texture;
		}

		virtual void SetWrap(TextureWrap u, TextureWrap v) override
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

			#ifdef __APPLE__
			Bind(m_texture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode[(size_t)m_wmode[0]]);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode[(size_t)m_wmode[1]]);
			#else
			if (glTextureParameteri)
			{
				glTextureParameteri(m_texture, GL_TEXTURE_WRAP_S, wmode[(size_t)m_wmode[0]]);
				glTextureParameteri(m_texture, GL_TEXTURE_WRAP_T, wmode[(size_t)m_wmode[1]]);
			}
			else
			{
				glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wmode[(size_t)m_wmode[0]]);
				glTextureParameteriEXT(m_texture, GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmode[(size_t)m_wmode[1]]);
			}
			#endif
		}

		TextureFormat GetFormat() const
		{
			return m_format;
		}
	};

	Texture TextureRes::Create(OpenGL* gl)
	{
		Texture_Impl* pImpl = new Texture_Impl();
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
		Texture_Impl* pImpl = new Texture_Impl();
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
