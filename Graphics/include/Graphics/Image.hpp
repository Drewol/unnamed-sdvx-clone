#pragma once
#include <Graphics/ResourceTypes.hpp>

namespace Graphics
{
	/*
		RGBA8 image class
		The bits have the same layout as the Colori class
	*/
	class ImageRes
	{
	public:
		virtual ~ImageRes() = default;
		static Ref<ImageRes> Create(const String& assetPath);
		static Ref<ImageRes> Create(Vector2i size = Vector2i());
		static Ref<ImageRes> Create(Buffer& b);
		static Ref<ImageRes> Screenshot(class OpenGL* gl, Vector2i size = Vector2i(), Vector2i pos = Vector2i());
	public:
		virtual void SetSize(Vector2i size) = 0;
		virtual void ReSize(Vector2i size) = 0;
		virtual Vector2i GetSize() const = 0;
		virtual Colori* GetBits() = 0;
		virtual const Colori* GetBits() const = 0;
		virtual void SavePNG(const String& file) = 0;
	};

	/*
		Sprite map
		Adding images to this will pack the image into a final image that contains all the added images
		After this the UV coordinates of these images can be asked for given and image index

		!! The packing is not optimal as the images are stacked for bottom to top and placed in columns based on their width
	*/
	class TextureRes;
	class SpriteMapRes
	{
	public:
		virtual ~SpriteMapRes() = default;
		static Ref<SpriteMapRes> Create();
	public:
		virtual uint32 AddSegment(Ref<ImageRes> image) = 0;
		virtual void Clear() = 0;
		virtual Ref<ImageRes> GetImage() = 0;
		virtual Ref<class TextureRes> GenerateTexture(class OpenGL* gl) = 0;
		virtual Shared::Recti GetCoords(uint32 nIndex) = 0;
	};

	typedef Ref<ImageRes> Image;
	typedef Ref<SpriteMapRes> SpriteMap;

	DEFINE_RESOURCE_TYPE(Image, ImageRes)
		DEFINE_RESOURCE_TYPE(SpriteMap, SpriteMapRes)
}
