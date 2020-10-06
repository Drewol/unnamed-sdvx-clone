#include "stdafx.h"
#include "Material.hpp"
#include "OpenGL.hpp"
#include <Graphics/ResourceManagers.hpp>
#include "RenderQueue.hpp"

namespace Graphics
{
	// Defines build in shader variables
	enum BuiltInShaderVariable
	{
		SV_World = 0,
		SV_Proj,
		SV_Camera,
		SV_BillboardMatrix,
		SV_Viewport,
		SV_AspectRatio,
		SV_Time,
		SV__BuiltInEnd,
		SV_User = 0x100, // Start defining user variables here
	};
	const char* builtInShaderVariableNames[] =
	{
		"world",
		"proj",
		"camera",
		"billboard",
		"viewport",
		"aspectRatio",
		"time",
	};
	class BuiltInShaderVariableMap : public Map<String, BuiltInShaderVariable>
	{
	public:
		BuiltInShaderVariableMap()
		{
			for(int32 i = 0; i < SV__BuiltInEnd; i++)
			{
				Add(builtInShaderVariableNames[i], (BuiltInShaderVariable)i);
			}
		}
	};
	BuiltInShaderVariableMap builtInShaderVariableMap;

	struct BoundParameterInfo
	{
		BoundParameterInfo(ShaderType shaderType, uint32 paramType, uint32 location)
			:shaderType(shaderType), paramType(paramType), location(location)
		{
		}

		ShaderType shaderType;
		uint32 paramType;
		uint32 location;
	};
	struct BoundParameterList : public Vector<BoundParameterInfo>
	{
	};

	// Defined in Shader.cpp
	extern uint32 shaderStageMap[];

	class Material_Impl : public MaterialRes
	{
	public:
		OpenGL* m_gl;
		Shader m_shaders[3];
#if _DEBUG
		String m_debugNames[3];
#endif
#ifdef EMBEDDED
		uint32 m_program;
#else
		uint32 m_pipeline;
#endif
		Map<uint32, BoundParameterList> m_boundParameters;
		Map<String, uint32> m_mappedParameters;
		Map<String, uint32> m_textureIDs;
		uint32 m_userID = SV_User;
		uint32 m_textureID = 0;
		Set<String> m_uniforms;

		Material_Impl(OpenGL* gl) : m_gl(gl)
		{
#ifdef EMBEDDED
			m_program = glCreateProgram();
#else
			glGenProgramPipelines(1, &m_pipeline);
#endif
		}
		~Material_Impl()
		{
			#ifdef EMBEDDED
			if (glIsProgram(m_program))
				glDeleteProgram(m_program);
			#else
			glDeleteProgramPipelines(1, &m_pipeline);
			#endif
		}
		void AssignShader(ShaderType t, Shader shader) override
		{
			m_shaders[(size_t)t] = shader;

			if (shader.get() == nullptr)
				return;

			uint32 handle = shader->Handle();

#ifdef _DEBUG
			Logf("Listing shader uniforms for %s", Logger::Severity::Info, shader->GetOriginalName());
#endif // _DEBUG

			int32 numUniforms;
#ifdef EMBEDDED
			glAttachShader(m_program, handle);
			glLinkProgram(m_program);
			
			glGetProgramiv(m_program, GL_ACTIVE_UNIFORMS, &numUniforms);
#else
			glGetProgramiv(handle, GL_ACTIVE_UNIFORMS, &numUniforms);
#endif
			
			for(int32 i = 0; i < numUniforms; i++)
			{
				char name[64];
				int32 nameLen, size;
				uint32 type;
				#ifdef EMBEDDED
				glGetActiveUniform(m_program, i, sizeof(name), &nameLen, &size, &type, name);
				uint32 loc = glGetUniformLocation(m_program, name);
				#else
				glGetActiveUniform(handle, i, sizeof(name), &nameLen, &size, &type, name);
				uint32 loc = glGetUniformLocation(handle, name);
				#endif
				m_uniforms.Add(name);
				// Select type
				uint32 textureID = 0;
				String typeName = "Unknown";
				if(type == GL_SAMPLER_2D)
				{
					typeName = "Sampler2D";
					if(!m_textureIDs.Contains(name))
						m_textureIDs.Add(name, m_textureID++);
				}
				else if(type == GL_FLOAT_MAT4)
				{
					typeName = "Transform";
				}
				else if(type == GL_FLOAT_VEC4)
				{
					typeName = "Vector4";
				}
				else if(type == GL_FLOAT_VEC3)
				{
					typeName = "Vector3";
				}
				else if(type == GL_FLOAT_VEC2)
				{
					typeName = "Vector2";
				}
				else if(type == GL_FLOAT)
				{
					typeName = "Float";
				}

				// Built in variable?
				uint32 targetID = 0;
				if(builtInShaderVariableMap.Contains(name))
				{
					targetID = builtInShaderVariableMap[name];
				}
				else
				{
					if(m_mappedParameters.Contains(name))
						targetID = m_mappedParameters[name];
					else
						targetID = m_mappedParameters.Add(name, m_userID++);
				}

				BoundParameterInfo& param = m_boundParameters.FindOrAdd(targetID).Add(BoundParameterInfo(t, type, loc));

#ifdef _DEBUG
				Logf("Uniform [%d, loc=%d, %s] = %s", Logger::Severity::Info,
					i, loc, Utility::Sprintf("Unknown [%d]", type), name);
#endif // _DEBUG
			}
#ifndef EMBEDDED
			glUseProgramStages(m_pipeline, shaderStageMap[(size_t)t], shader->Handle());
#endif
		}

		// Bind render state and params and shaders to context
		virtual void Bind(const RenderState& rs, const MaterialParameterSet& params) override
		{
#if _DEBUG
			bool reloadedShaders = false;
			for(uint32 i = 0; i < 3; i++)
			{
				if(m_shaders[i] && m_shaders[i]->UpdateHotReload())
				{
					reloadedShaders = true;
				}
			}

			// Regenerate parameter map
			if(reloadedShaders)
			{
				Log("Reloading material", Logger::Severity::Info);
				m_boundParameters.clear();
				m_textureIDs.clear();
				m_mappedParameters.clear();
				m_userID = SV_User;
				m_textureID = 0;
				for(uint32 i = 0; i < 3; i++)
				{
					if(m_shaders[i])
						AssignShader(ShaderType(i), m_shaders[i]);
					#ifdef EMBEDDED
					glLinkProgram(m_program);
					#endif
				}
			}
#endif
			#ifdef EMBEDDED
			BindToContext();
			#endif
			// Bind renderstate variables
			BindAll(SV_Proj, rs.projectionTransform);
			BindAll(SV_Camera, rs.cameraTransform);
			BindAll(SV_Viewport, rs.viewportSize);
			BindAll(SV_AspectRatio, rs.aspectRatio);
			Transform billboard = CameraMatrix::BillboardMatrix(rs.cameraTransform);
			BindAll(SV_BillboardMatrix, billboard);
			BindAll(SV_Time, rs.time);
			
			// Bind parameters
			BindParameters(params, rs.worldTransform);
			#ifndef EMBEDDED
			BindToContext();
			#endif
		}

		// Bind only parameters
		void BindParameters(const MaterialParameterSet& params, const Transform& worldTransform) override
		{
			BindAll(SV_World, worldTransform);
			for(auto p : params)
			{
				switch(p.second.parameterType)
				{
				case GL_INT:
					BindAll(p.first, p.second.Get<int>());
					break;
				case GL_FLOAT:
					BindAll(p.first, p.second.Get<float>());
					break;
				case GL_INT_VEC2:
					BindAll(p.first, p.second.Get<Vector2i>());
					break;
				case GL_INT_VEC3:
					BindAll(p.first, p.second.Get<Vector3i>());
					break;
				case GL_INT_VEC4:
					BindAll(p.first, p.second.Get<Vector4i>());
					break;
				case GL_FLOAT_VEC2:
					BindAll(p.first, p.second.Get<Vector2>());
					break;
				case GL_FLOAT_VEC3:
					BindAll(p.first, p.second.Get<Vector3>());
					break;
				case GL_FLOAT_VEC4:
					BindAll(p.first, p.second.Get<Vector4>());
					break;
				case GL_FLOAT_MAT4:
					BindAll(p.first, p.second.Get<Transform>());
					break;
				case GL_SAMPLER_2D:
				{
					uint32* textureUnit = m_textureIDs.Find(p.first);
					if(!textureUnit)
					{
						/// TODO: Add print once mechanism for these kind of errors
						//Logf("Texture not found \"%s\"", Logger::Warning, p.first);
						break;
					}
					Ref<TextureRes> texture = p.second.Get<Ref<TextureRes>>();

					// Bind the texture
					texture->Bind(*textureUnit);


					// Bind sampler
					BindAll<int32>(p.first, *textureUnit);
					break;
				}
				default:
					assert(false);
				}
			}
		}

		void BindToContext() override
		{
			// Bind pipeline to context
			#ifdef EMBEDDED
			glUseProgram(m_program);
			#else
			glBindProgramPipeline(m_pipeline);
			#endif
		}

		virtual bool HasUniform(String name) override
		{
			return m_uniforms.Contains(name);
		}

		BoundParameterInfo* GetBoundParameters(const String& name, uint32& count)
		{
			uint32* mappedID = m_mappedParameters.Find(name);
			if(!mappedID)
				return nullptr;
			return GetBoundParameters((BuiltInShaderVariable)*mappedID, count);
		}
		BoundParameterInfo* GetBoundParameters(BuiltInShaderVariable bsv, uint32& count)
		{
			BoundParameterList* l = m_boundParameters.Find(bsv);
			if(!l)
				return nullptr;
			else
			{
				count = (uint32)l->size();
				return l->data();
			}
		}
		template<typename T> void BindAll(const String& name, const T& obj)
		{
			uint32 num = 0;
			#ifdef EMBEDDED
			glUseProgram(m_program);
			#endif
			BoundParameterInfo* bp = GetBoundParameters(name, num);
			for(uint32 i = 0; bp && i < num; i++)
			{
				BindShaderVar<T>(m_shaders[(size_t)bp[i].shaderType]->Handle(), bp[i].location, obj);
			}
		}
		template<typename T> void BindAll(BuiltInShaderVariable bsv, const T& obj)
		{
			uint32 num = 0;
			#ifdef EMBEDDED
			glUseProgram(m_program);
			#endif
			BoundParameterInfo* bp = GetBoundParameters(bsv, num);
			for(uint32 i = 0; bp && i < num; i++)
			{
				BindShaderVar<T>(m_shaders[(size_t)bp[i].shaderType]->Handle(), bp[i].location, obj);
			}
		}

		template<typename T> void BindShaderVar(uint32 shader, uint32 loc, const T& obj)
		{
			static_assert(sizeof(T) != 0, "Incompatible shader uniform type");
		}
	};
	
#ifdef EMBEDDED
	template<> void Material_Impl::BindShaderVar<Vector4>(uint32 shader, uint32 loc, const Vector4& obj)
	{
		glUniform4fv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector3>(uint32 shader, uint32 loc, const Vector3& obj)
	{
		glUniform3fv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector2>(uint32 shader, uint32 loc, const Vector2& obj)
	{
		glUniform2fv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<float>(uint32 shader, uint32 loc, const float& obj)
	{
		glUniform1fv(loc, 1, &obj);
	}
	template<> void Material_Impl::BindShaderVar<Colori>(uint32 shader, uint32 loc, const Colori& obj)
	{
		Color c = obj;
		glUniform4fv(loc, 1, &c.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector4i>(uint32 shader, uint32 loc, const Vector4i& obj)
	{
		glUniform4iv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector3i>(uint32 shader, uint32 loc, const Vector3i& obj)
	{
		glUniform3iv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector2i>(uint32 shader, uint32 loc, const Vector2i& obj)
	{
		glUniform2iv(loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<int32>(uint32 shader, uint32 loc, const int32& obj)
	{
		glUniform1iv(loc, 1, &obj);
	}
	template<> void Material_Impl::BindShaderVar<Transform>(uint32 shader, uint32 loc, const Transform& obj)
	{
		glUniformMatrix4fv(loc, 1, GL_FALSE, obj.mat);
	}
#else
	template<> void Material_Impl::BindShaderVar<Vector4>(uint32 shader, uint32 loc, const Vector4& obj)
	{
		glProgramUniform4fv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector3>(uint32 shader, uint32 loc, const Vector3& obj)
	{
		glProgramUniform3fv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector2>(uint32 shader, uint32 loc, const Vector2& obj)
	{
		glProgramUniform2fv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<float>(uint32 shader, uint32 loc, const float& obj)
	{
		glProgramUniform1fv(shader, loc, 1, &obj);
	}
	template<> void Material_Impl::BindShaderVar<Colori>(uint32 shader, uint32 loc, const Colori& obj)
	{
		Color c = obj;
		glProgramUniform4fv(shader, loc, 1, &c.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector4i>(uint32 shader, uint32 loc, const Vector4i& obj)
	{
		glProgramUniform4iv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector3i>(uint32 shader, uint32 loc, const Vector3i& obj)
	{
		glProgramUniform3iv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<Vector2i>(uint32 shader, uint32 loc, const Vector2i& obj)
	{
		glProgramUniform2iv(shader, loc, 1, &obj.x);
	}
	template<> void Material_Impl::BindShaderVar<int32>(uint32 shader, uint32 loc, const int32& obj)
	{
		glProgramUniform1iv(shader, loc, 1, &obj);
	}
	template<> void Material_Impl::BindShaderVar<Transform>(uint32 shader, uint32 loc, const Transform& obj)
	{
		glProgramUniformMatrix4fv(shader, loc, 1, GL_FALSE, obj.mat);
	}
#endif

	Material MaterialRes::Create(OpenGL* gl)
	{
		Material_Impl* impl = new Material_Impl(gl);
		return GetResourceManager<ResourceType::Material>().Register(impl);

	}
	Material MaterialRes::Create(OpenGL* gl, const String& vsPath, const String& fsPath)
	{
		Material_Impl* impl = new Material_Impl(gl);
		impl->AssignShader(ShaderType::Vertex, ShaderRes::Create(gl, ShaderType::Vertex, vsPath));
		impl->AssignShader(ShaderType::Fragment, ShaderRes::Create(gl, ShaderType::Fragment, fsPath));
#if _DEBUG
		impl->m_debugNames[(size_t)ShaderType::Vertex] = vsPath;
		impl->m_debugNames[(size_t)ShaderType::Fragment] = fsPath;
#endif

		if(!impl->m_shaders[(size_t)ShaderType::Vertex])
		{
			Logf("Failed to load vertex shader for material from %s", Logger::Severity::Error, vsPath);
			delete impl;
			return Material();
		}
		if(!impl->m_shaders[(size_t)ShaderType::Fragment])
		{
			Logf("Failed to load fragment shader for material from %s", Logger::Severity::Error, fsPath);
			delete impl;
			return Material();
		}

		return GetResourceManager<ResourceType::Material>().Register(impl);
	}

	void MaterialParameterSet::SetParameter(const String& name, int sc)
	{
		Add(name, MaterialParameter::Create(sc, GL_INT));
	}
	void MaterialParameterSet::SetParameter(const String& name, float sc)
	{
		Add(name, MaterialParameter::Create(sc, GL_FLOAT));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Vector4& vec)
	{
		Add(name, MaterialParameter::Create(vec, GL_FLOAT_VEC4));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Colori& color)
	{
		Add(name, MaterialParameter::Create(Color(color), GL_FLOAT_VEC4));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Vector2& vec2)
	{
		Add(name, MaterialParameter::Create(vec2, GL_FLOAT_VEC2));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Vector3& vec3)
	{
		Add(name, MaterialParameter::Create(vec3, GL_FLOAT_VEC3));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Transform& tf)
	{
		Add(name, MaterialParameter::Create(tf, GL_FLOAT_MAT4));
	}
	void MaterialParameterSet::SetParameter(const String& name, Ref<class TextureRes> tex)
	{
		Add(name, MaterialParameter::Create(tex, GL_SAMPLER_2D));
	}
	void MaterialParameterSet::SetParameter(const String& name, const Vector2i& vec2)
	{
		Add(name, MaterialParameter::Create(vec2, GL_INT_VEC2));
	}
}
