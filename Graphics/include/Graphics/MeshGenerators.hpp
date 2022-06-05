#pragma once
#include <Graphics/Mesh.hpp>

namespace Graphics
{
	// Generates meshes based on certain parameters
	// generated attributes are always in the following order:
	//	- Position
	//	- Texture Coordinates
	//	- Color
	//	- Normal
	namespace MeshGenerators
	{
		using Shared::Rect3D;
		using Shared::Rect;

		struct SimpleVertex : public VertexFormat<Vector3, Vector2>
		{
			SimpleVertex() = default;
			SimpleVertex(Vector3 pos, Vector2 tex) : pos(pos), tex(tex) {};
			Vector3 pos;
			Vector2 tex;
		};

		Mesh Quad(OpenGL* gl, Vector2 pos, Vector2 size = Vector2(1, 1));

		// Generates vertices for a quad from a given rectangle, with given uv coordinate rectangle
		// the position top = +y
		// the uv has bottom = +y
		// Triangle List
		void GenerateSimpleXYQuad(Rect3D r, Rect uv, Vector<MeshGenerators::SimpleVertex>& out);

		void GenerateSimpleXZQuad(Rect3D r, Rect uv, Vector<MeshGenerators::SimpleVertex>& out);
	}
}
