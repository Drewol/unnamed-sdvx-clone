#include "stdafx.h"
#include "Mesh.hpp"
#include <Graphics/ResourceManagers.hpp>

namespace Graphics
{
	uint32 primitiveTypeMap[] =
	{
		GL_TRIANGLES,
		GL_TRIANGLE_STRIP,
		GL_TRIANGLE_FAN,
		GL_LINES,
		GL_LINE_STRIP,
		GL_POINTS,
	};
	class Mesh_Impl : public MeshRes
	{
		uint32 m_buffer = 0;
		uint32 m_vao = 0;
		PrimitiveType m_type;
		uint32 m_glType;
		size_t m_vertexCount;
		bool m_bDynamic = true;
	public:
		Mesh_Impl()
		{
		}
		~Mesh_Impl()
		{
			if(m_buffer)
				glDeleteBuffers(1, &m_buffer);
			if(m_vao)
				glDeleteVertexArrays(1, &m_vao);
		}
		bool Init()
		{
			glGenBuffers(1, &m_buffer);
			glGenVertexArrays(1, &m_vao);
			return m_buffer != 0 && m_vao != 0;
		}

		void SetData(const void* pData, size_t vertexCount, const VertexFormatList& desc) override
		{
			glBindVertexArray(m_vao);
			glBindBuffer(GL_ARRAY_BUFFER, m_buffer);

			m_vertexCount = vertexCount;
			size_t totalVertexSize = 0;
			for(auto e : desc)
				totalVertexSize += e.componentSize * e.components;
			size_t index = 0;
			size_t offset = 0;
			for(auto e : desc)
			{
				uint32 type = -1;
				if(!e.isFloat)
				{
					if(e.componentSize == 4)
						type = e.isSigned ? GL_INT : GL_UNSIGNED_INT;
					else if(e.componentSize == 2)
						type = e.isSigned ? GL_SHORT : GL_UNSIGNED_SHORT;
					else if(e.componentSize == 1)
						type = e.isSigned ? GL_BYTE : GL_UNSIGNED_BYTE;
				}
				else
				{
					#ifdef EMBEDDED
					type = GL_FLOAT;
					#else
					if(e.componentSize == 4)
						type = GL_FLOAT;
					else if(e.componentSize == 8)
						type = GL_DOUBLE;
					#endif
				}
				assert(type != (uint32)-1);
				glVertexAttribPointer((int)index, (int)e.components, type, GL_TRUE, (int)totalVertexSize, (void*)offset);
				glEnableVertexAttribArray((int)index);
				offset += e.componentSize * e.components;
				index++;
			}
			glBufferData(GL_ARRAY_BUFFER, totalVertexSize * vertexCount, pData, m_bDynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

			glBindVertexArray(0);
			glBindBuffer(GL_ARRAY_BUFFER, 0);
		}
		
		#ifdef EMBEDDED
		void Draw() override
		{
			glBindVertexArray(m_vao);
			glDrawArrays(m_glType, 0, (int)m_vertexCount);
			glBindVertexArray(0);
		}
		void Redraw() override
		{
			glBindVertexArray(m_vao);
			glDrawArrays(m_glType, 0, (int)m_vertexCount);
			glBindVertexArray(0);
		}
		#else
		void Draw() override
		{
			glBindVertexArray(m_vao);
			glDrawArrays(m_glType, 0, (int)m_vertexCount);
		}
		void Redraw() override
		{
			glDrawArrays(m_glType, 0, (int)m_vertexCount);
		}
		#endif

		void SetPrimitiveType(PrimitiveType pt) override
		{
			m_type = pt;
			m_glType = primitiveTypeMap[(size_t)pt];
		}
		virtual PrimitiveType GetPrimitiveType() const override
		{
			return m_type;
		}
	};

	Mesh MeshRes::Create(class OpenGL* gl)
	{
		Mesh_Impl* pImpl = new Mesh_Impl();
		if(!pImpl->Init())
		{
			delete pImpl;
			return Mesh();
		}
		else
		{
			return GetResourceManager<ResourceType::Mesh>().Register(pImpl);
		}
	}
}