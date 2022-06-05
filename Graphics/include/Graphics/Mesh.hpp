#pragma once
#include <Graphics/ResourceTypes.hpp>
#include <Graphics/VertexFormat.hpp>

namespace Graphics
{
	/* The type that tells how a mesh is drawn */
	enum class PrimitiveType
	{
		TriangleList = 0,
		TriangleStrip,
		TriangleFan,
		LineList,
		LineStrip,
		PointList,
	};

	/*
		Simple mesh object
	*/
	class MeshRes
	{
	public:
		virtual ~MeshRes() = default;
		static Ref<MeshRes> Create(class OpenGL* gl);
	public:
		// Sets the vertex point data for this mesh
		// must be set before drawing
		// the vertex type must inherit from VertexFormat to automatically detect the correct format
		template<typename T>
		void SetData(const Vector<T>& verts)
		{
			SetData(verts.data(), verts.size(), T::GetDescriptors());
		}

		// Sets how the point data is interpreted and drawn
		// must be set before drawing
		virtual void SetPrimitiveType(PrimitiveType pt) = 0;
		virtual PrimitiveType GetPrimitiveType() const = 0;
		// Draws the mesh
		virtual void Draw() = 0;
		// Draws the mesh after if has already been drawn once, reuse of bound objects
		virtual void Redraw() = 0;

	private:
		virtual void SetData(const void* pData, size_t vertexCount, const VertexFormatList& desc) = 0;
	};

	typedef Ref<MeshRes> Mesh;

	DEFINE_RESOURCE_TYPE(Mesh, MeshRes)
}
