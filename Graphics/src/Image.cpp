#include "stdafx.h"
#include "Image.hpp"
#include <Graphics/ResourceManagers.hpp>
#include "ImageLoader.hpp"
#include "OpenGL.hpp"

#ifdef __APPLE__
#include "libpng16/png.h"
#else
#include "png.h"
#endif

namespace Graphics
{
	class Image_Impl : public ImageRes
	{
		Vector2i m_size;
		Colori* m_pData = nullptr;
		size_t m_nDataLength;
	public:
		Image_Impl()
		{

		}
		~Image_Impl()
		{
			Clear();
		}
		void Clear()
		{
			if (m_pData)
				delete[] m_pData;
			m_pData = nullptr;
		}
		void Allocate()
		{
			m_nDataLength = m_size.x * m_size.y;
			if (m_nDataLength == 0)
				return;
			m_pData = new Colori[m_nDataLength];
		}

		void SetSize(Vector2i size)
		{
			m_size = size;
			Clear();
			Allocate();
		}
		void ReSize(Vector2i size)
		{
			size_t new_DataLength = size.x * size.y;
			if (new_DataLength == 0) {
				return;
			}
			Colori* new_pData = new Colori[new_DataLength];

			for (int32 ix = 0; ix < size.x; ++ix) {
				for (int32 iy = 0; iy < size.y; ++iy) {
					int32 sampledX = ix * ((double)m_size.x / (double)size.x);
					int32 sampledY = iy * ((double)m_size.y / (double)size.y);
					new_pData[size.x * iy + ix] = m_pData[m_size.x * sampledY + sampledX];
				}
			}

			delete[] m_pData;
			m_pData = new_pData;
			m_size = size;
			m_nDataLength = m_size.x * m_size.y;
		}
		bool Screenshot(OpenGL* gl, Vector2i pos)
		{
			GLuint texture;
			GLenum err;
			while ((err = glGetError()) != GL_NO_ERROR) //Clear out preexisting errors
			{
				Logf("OpenGL Error: 0x%p", Logger::Severity::Debug, err);
			}

			//Create texture
			glGenTextures(1, &texture);
			glBindTexture(GL_TEXTURE_2D, texture);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_size.x, m_size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
			glBindTexture(GL_TEXTURE_2D, 0);
			glFinish();

			while ((err = glGetError()) != GL_NO_ERROR)
			{
				Logf("OpenGL Error: 0x%p", Logger::Severity::Error, err);
				return false;
			}

			//Set texture from buffer
			glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
			glReadBuffer(GL_BACK);
			glBindTexture(GL_TEXTURE_2D, texture);
			glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, pos.x, pos.y, m_size.x, m_size.y);

			while ((err = glGetError()) != GL_NO_ERROR)
			{
				Logf("OpenGL Error: 0x%p", Logger::Severity::Error, err);
				return false;
			}
			glFinish();


			//Copy texture contents to image pData
#ifdef EMBEDDED
			glReadPixels(0, 0, m_size.x, m_size.y, GL_RGBA, GL_UNSIGNED_BYTE, m_pData);
#else
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, m_pData);
#endif
			glBindTexture(GL_TEXTURE_2D, 0);
			glFinish();

			while ((err = glGetError()) != GL_NO_ERROR)
			{
				Logf("OpenGL Error: 0x%p", Logger::Severity::Error, err);
				return false;
			}

			//Delete texture
			glDeleteTextures(1, &texture);
			return true;
		}
		void SavePNG(const String& file)
		{
			///TODO: Use shared/File.hpp instead?
			File pngfile;
			pngfile.OpenWrite(Path::Normalize(file));

			png_structp png_ptr = NULL;
			png_infop info_ptr = NULL;
			png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
			info_ptr = png_create_info_struct(png_ptr);
			if (setjmp(png_jmpbuf(png_ptr)))
			{
				/* If we get here, we had a problem writing the file */
				pngfile.Close();
				png_destroy_write_struct(&png_ptr, &info_ptr);
				assert(false);
			}
			png_set_IHDR(png_ptr, info_ptr, m_size.x, m_size.y,
				8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
				PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

			png_bytep* row_pointers = (png_bytep*)malloc(sizeof(png_bytep) * m_size.y);
			for (int i = 0; i < m_size.y; ++i) {
				row_pointers[m_size.y - i - 1] = (png_bytep)(m_pData + i * m_size.x);
			}

			png_set_write_fn(png_ptr, &pngfile, pngfile_write_data, pngfile_flush);
			png_set_rows(png_ptr, info_ptr, row_pointers);
			png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);
			pngfile.Close();
			free(row_pointers);
		}
		Vector2i GetSize() const
		{
			return m_size;
		}
		Colori* GetBits()
		{
			return m_pData;
		}
		const Colori* GetBits() const
		{
			return m_pData;
		}
	private:
		static void pngfile_write_data(png_structp png_ptr, png_bytep data, png_size_t length)
		{
			File* pngFile = (File*)png_get_io_ptr(png_ptr);
			pngFile->Write(data, length);
		}
		static void pngfile_flush(png_structp png_ptr)
		{
		}
	};

	Image ImageRes::Create(Vector2i size)
	{
		Image_Impl* pImpl = new Image_Impl();
		pImpl->SetSize(size);
		return GetResourceManager<ResourceType::Image>().Register(pImpl);
	}
	Ref<ImageRes> ImageRes::Create(Buffer& b)
	{
		Image_Impl* pImpl = new Image_Impl();
		if (ImageLoader::Load(pImpl, b))
		{
			return GetResourceManager<ResourceType::Image>().Register(pImpl);
		}
		else
		{
			delete pImpl;
			pImpl = nullptr;
		}
		return Image();
	}
	Image ImageRes::Create(const String& assetPath)
	{
		Image_Impl* pImpl = new Image_Impl();
		if (ImageLoader::Load(pImpl, assetPath))
		{
			return GetResourceManager<ResourceType::Image>().Register(pImpl);
		}
		else
		{
			delete pImpl;
			pImpl = nullptr;
		}
		return Image();
	}
	Image ImageRes::Screenshot(OpenGL* gl, Vector2i size, Vector2i pos)
	{
		Image_Impl* pImpl = new Image_Impl();
		pImpl->SetSize(size);
		if (!pImpl->Screenshot(gl, pos))
		{
			delete pImpl;
			pImpl = nullptr;
		}
		else
		{
			return GetResourceManager<ResourceType::Image>().Register(pImpl);
		}
		return Image();
	}
}
