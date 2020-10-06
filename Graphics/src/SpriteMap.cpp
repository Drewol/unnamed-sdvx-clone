#include "stdafx.h"
#include "Image.hpp"
#include "Texture.hpp"
#include <Graphics/ResourceManagers.hpp>
#include <set>

namespace Graphics
{
	using Shared::Recti;

	static uint32 maxHeight = 1024;
	static uint32 border = 2;

	struct Category
	{
		uint32 width = 0;
		Vector2i offset;
		Vector<uint32> segments;
	};
	struct Segment
	{
		Recti coords;
	};

	class SpriteMap_Impl : public SpriteMapRes
	{
		friend class ImageRes;

		// The image that contains the current data
		Image m_image;

		// Used size over the X axis
		int32 m_usedSize = 0;

		// Linear index of al segements
		Vector<Segment*> m_segments;
		Vector<Category> m_widths;
		std::multimap<uint32, uint32> m_categoryByWidth;
	public:
		SpriteMap_Impl()
		{
			m_image = ImageRes::Create();
		}
		~SpriteMap_Impl()
		{
			Clear();
		}
		void Clear() override
		{
			for(auto s : m_segments)
			{
				delete s;
			}
			m_segments.clear();
			m_widths.clear();
			m_categoryByWidth.clear();
			m_usedSize = 0;
			m_image->SetSize(Vector2i(0));
		}

		virtual Vector2i GetSize() const
		{
			return m_image->GetSize();
		}
		virtual Colori* GetBits()
		{
			return m_image->GetBits();
		}
		virtual const Colori* GetBits() const
		{
			return m_image->GetBits();
		}

		// Returns the category that has space for the requested size
		Category& AssignCategory(Vector2i requestedSize)
		{
			Category* dstCat = nullptr;

			while(true)
			{
				auto range = m_categoryByWidth.equal_range(requestedSize.x);
				// Find a suitable category first
				for(auto it = range.first; it != range.second; it++)
				{
					Category& cat = m_widths[it->second];
					int32 remainingY = m_image->GetSize().y - cat.offset.y;
					if(remainingY > requestedSize.y)
					{
						// This category is OK
						dstCat = &cat;
						break;
					}
				}

				// Create a new category if required
				if(!dstCat)
				{
					int32 remainingX = m_image->GetSize().x - m_usedSize;
					// Use horizontal space to add another collumn
					//	if height of image is big enough
					if(m_image->GetSize().y >= requestedSize.y && remainingX >= requestedSize.x)
					{
						Category& cat = m_widths.Add();
						cat.width = requestedSize.x;
						cat.offset = Vector2i(m_usedSize, 0);
						m_categoryByWidth.insert(std::make_pair(cat.width, (uint32)m_widths.size()-1));
						m_usedSize += cat.width + 1;
					}
					else
					{
						int32 remainingX = (m_image->GetSize().x - m_usedSize);

						// Resize image
						int32 largestDim = Math::Max(m_usedSize + requestedSize.x,
							Math::Max(m_image->GetSize().y, requestedSize.y));
						int32 targetSize = (int32)pow(2, ceil(log(largestDim) / log(2)));
						if(m_image->GetSize().x != targetSize)
						{
							// Resize image
							Image newImage = ImageRes::Create(Vector2i(targetSize));
							// Copy old image into new image
							if(m_image->GetSize().x > 0)
								CopySubImage(newImage, m_image, Vector2i());
							m_image = newImage;
						}
					}
				}
				else
				{
					break;
				}
			}
			
			return *dstCat;
		}
		uint32 AddSegment(Image image) override
		{
			// Create a new segment
			uint32 nI = (uint32)m_segments.size();
			Segment* pCurrentSegment = m_segments.Add(new Segment());
			pCurrentSegment->coords.size = image->GetSize();

			// Get a category that has space
			Category& cat = AssignCategory(image->GetSize());

			// Set offset for current segment
			pCurrentSegment->coords.pos = cat.offset;
			// Add size offset in category
			cat.offset.y += pCurrentSegment->coords.size.y + 1;

			// Copy image data
			CopySubImage(m_image, image, pCurrentSegment->coords.pos);

			// Add segment to this category
			cat.segments.push_back(nI);
			return nI;
		}

		void CopySubImage(Image dst, Image src, Vector2i dstPos)
		{
			Vector2i srcSize = src->GetSize();

			Colori* pSrc = src->GetBits();
			uint32 nDstPitch = dst->GetSize().x;
			Colori* pDst = dst->GetBits() + dstPos.x + dstPos.y * nDstPitch;
			for(uint32 y = 0; y < (uint32)srcSize.y; y++)
			{
				for(uint32 x = 0; x < (uint32)srcSize.x; x++)
				{
					*pDst = *pSrc;
					pSrc++;
					pDst++;
				}
				pDst += (nDstPitch - srcSize.x);
			}
		}
		Recti GetCoords(uint32 nIndex) override
		{
			assert(nIndex < m_segments.size());
			return m_segments[nIndex]->coords;
		}
		Ref<ImageRes> GetImage() override
		{
			return m_image;
		}
		Texture GenerateTexture(OpenGL* gl) override
		{
			Texture tex = TextureRes::Create(gl, m_image);
			tex->SetWrap(TextureWrap::Clamp, TextureWrap::Clamp);
			return tex;
		}
	};

	SpriteMap SpriteMapRes::Create()
	{
		SpriteMap_Impl* pImpl = new SpriteMap_Impl();
		return GetResourceManager<ResourceType::SpriteMap>().Register(pImpl);
	}
}
