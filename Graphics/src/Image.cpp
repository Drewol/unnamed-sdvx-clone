#include "stdafx.h"
#include "Image.hpp"
#include <Graphics/ResourceManagers.hpp>
#include "ImageLoader.hpp"

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
			if(m_pData)
				delete[] m_pData;
			m_pData = nullptr;
		}
		void Allocate()
		{
			m_nDataLength = m_size.x * m_size.y;
			if(m_nDataLength == 0)
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
			if (new_DataLength == 0){
				return;
			}
			Colori* new_pData = new Colori[new_DataLength];

			for (int32 ix = 0; ix < size.x; ++ix){
				for (int32 iy = 0; iy < size.y; ++iy){
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
	};

	Image ImageRes::Create(Vector2i size)
	{
		Image_Impl* pImpl = new Image_Impl();
		pImpl->SetSize(size);
		return GetResourceManager<ResourceType::Image>().Register(pImpl);
	}
	Image ImageRes::Create(const String& assetPath)
	{
		Image_Impl* pImpl = new Image_Impl();
		if(ImageLoader::Load(pImpl, assetPath))
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
}