#include "stdafx.h"
#include "OpenGL.hpp"
#include "ParticleSystem.hpp"
#include "Mesh.hpp"
#include "VertexFormat.hpp"
#include <Graphics/ResourceManagers.hpp>

namespace Graphics
{
	struct ParticleVertex : VertexFormat<Vector3, Vector4, Vector4>
	{
		ParticleVertex(Vector3 pos, Color color, Vector4 params) : pos(pos), color(color), params(params) {};
		Vector3 pos;
		Color color;
		// X = scale
		// Y = rotation
		// Z = animation frame
		Vector4 params;
	};

	class ParticleSystem_Impl : public ParticleSystemRes
	{
		friend class ParticleEmitter;
		Vector<Ref<ParticleEmitter>> m_emitters;

	public:
		OpenGL* gl;

	public:
		virtual void Render(const class RenderState& rs, float deltaTime) override
		{
			// Enable blending for all particles
			glEnable(GL_BLEND);

			// Tick all emitters and remove old ones
			for(auto it = m_emitters.begin(); it != m_emitters.end();)
			{
				(*it)->Render(rs, deltaTime);

				if(it->use_count() == 1)
				{
					if((*it)->HasFinished())
					{
						// Remove unreferenced and finished emitters
						it = m_emitters.erase(it);
						continue;
					}
					else if((*it)->loops == 0)
					{
						// Deactivate unreferenced infinte duration emitters
						(*it)->Deactivate();
					}
				}

				it++;
			}
		}
		Ref<ParticleEmitter> AddEmitter() override
		{
			Ref<ParticleEmitter> newEmitter = Utility::MakeRef<ParticleEmitter>(new ParticleEmitter(this));
			m_emitters.Add(newEmitter);
			return newEmitter;
		}
		void Reset() override
		{
			for(auto em : m_emitters)
			{
				em.reset();
			}
			m_emitters.clear();
		}
	};

	Ref<ParticleSystemRes> ParticleSystemRes::Create(class OpenGL* gl)
	{
		ParticleSystem_Impl* impl = new ParticleSystem_Impl();
		impl->gl = gl;
		return GetResourceManager<ResourceType::ParticleSystem>().Register(impl);
	}


	// Particle instance class
	class Particle
	{
	public:
		float life = 0.0f;
		float maxLife = 0.0f;
		float rotation = 0.0f;
		float startSize = 0.0f;
		Color startColor;
		Vector3 pos;
		Vector3 velocity;
		float scale;
		float fade;
		float drag;
		

		bool IsAlive() const
		{
			return life > 0.0f;
		}
		inline void Init(ParticleEmitter* emitter)
		{
			const float& et = emitter->m_emitterRate;
			life = maxLife = emitter->m_param_Lifetime->Init(et);
			pos = emitter->m_param_StartPosition->Init(et) * emitter->scale;

			// Velocity of startvelocity and spawn offset scale
			velocity = emitter->m_param_StartVelocity->Init(et) * emitter->scale;
			float spawnVelScale = emitter->m_param_SpawnVelocityScale->Init(et);
			if(spawnVelScale > 0)
				velocity += pos.Normalized() * spawnVelScale  * emitter->scale;

			// Add emitter offset to location
			pos += emitter->position;

			startColor = emitter->m_param_StartColor->Init(et);
			rotation = emitter->m_param_StartRotation->Init(et);
			startSize = emitter->m_param_StartSize->Init(et) * emitter->scale;
			drag = emitter->m_param_StartDrag->Init(et);
		}
		inline void Simulate(ParticleEmitter* emitter, float deltaTime)
		{
			float c = 1 - life / maxLife;

			// Add gravity
			velocity += emitter->m_param_Gravity->Sample(emitter->m_emitterTime) * deltaTime * emitter->scale;
			pos += velocity * deltaTime;

			// Add drag
			velocity += -velocity * deltaTime * drag;

			fade = emitter->m_param_FadeOverTime->Sample(c);
			scale = emitter->m_param_ScaleOverTime->Sample(c);
			life -= deltaTime;
		}
	};

	ParticleEmitter::ParticleEmitter(ParticleSystem_Impl* sys) : m_system(sys)
	{
		m_mesh = MeshRes::Create(m_system->gl);
		m_mesh->SetPrimitiveType(PrimitiveType::PointList);

		// Set parameter defaults
#define PARTICLE_DEFAULT(__name, __value)\
	Set##__name(__value);
#include "ParticleParameters.hpp"
	}
	ParticleEmitter::~ParticleEmitter()
	{
		// Cleanup particle parameters
#define PARTICLE_PARAMETER(__name, __type)\
	if(m_param_##__name){\
		delete m_param_##__name; m_param_##__name = nullptr; }
#include "ParticleParameters.hpp"

		if(m_particles)
		{
			delete[] m_particles;
		}
	}

	void ParticleEmitter::m_ReallocatePool(uint32 newCapacity)
	{
		Particle* oldParticles = m_particles;
		uint32 oldSize = m_poolSize;

		// Create new pool
		m_poolSize = newCapacity;
		if(newCapacity > 0)
		{
			m_particles = new Particle[m_poolSize];
			for (size_t i = 0; i < m_poolSize; i++)
			{
				m_particles[i] = Particle();
			}
			
		}
		else
		{
			m_particles = nullptr;
		}

		if(oldParticles && m_particles)
		{
			memcpy(m_particles, oldParticles, Math::Min(oldSize, m_poolSize) * sizeof(Particle));
		}

		if(oldParticles)
			delete[] oldParticles;
	}
	void ParticleEmitter::Render(const class RenderState& rs, float deltaTime)
	{
		if(m_finished)
			return;

		uint32 maxDuration = (uint32)ceilf(m_param_Lifetime->GetMax());
		uint32 maxSpawns = (uint32)ceilf(m_param_SpawnRate->GetMax());
		uint32 maxParticles = maxSpawns * maxDuration;
		// Round up to 64
		maxParticles = (uint32)ceil((float)maxParticles / 64.0f) * 64;

		if(maxParticles > m_poolSize)
			m_ReallocatePool(maxParticles);

		// Resulting vertex bufffer
		Vector<ParticleVertex> verts;

		// Increment emitter time
		m_emitterTime += deltaTime;
		while(m_emitterTime > duration)
		{
			m_emitterTime -= duration;
			m_emitterLoopIndex++;
		}
		m_emitterRate = m_emitterTime / duration;

		// Increment spawn counter
		m_spawnCounter += deltaTime * m_param_SpawnRate->Sample(m_emitterRate);

		uint32 numSpawns = 0;
		float spawnTimeOffset = 0.0f;
		float spawnTimeOffsetStep = 0;
		if(loops > 0 && m_emitterLoopIndex >= loops) // Should spawn particles ?
			m_deactivated = true;

		if(!m_deactivated)
		{
			// Calculate number of new particles to spawn
			float spawnsf;
			m_spawnCounter = modff(m_spawnCounter, &spawnsf);
			numSpawns = (uint32)spawnsf;
			spawnTimeOffsetStep = deltaTime / spawnsf;
		}

		bool updatedSomething = false;
		for(uint32 i = 0; i < m_poolSize; i++)
		{
			Particle& p = m_particles[i];

			bool render = false;
			if(!m_particles[i].IsAlive())
			{
				// Try to spawn a new particle in this slot
				if(numSpawns > 0)
				{
					p.Init(this);
					p.Simulate(this, spawnTimeOffset);
					spawnTimeOffset += spawnTimeOffsetStep;
					numSpawns--;
					render = true;
				}
			}
			else
			{
				p.Simulate(this, deltaTime);
				render = true;
				updatedSomething = true;
			}

			if(render)
			{
				verts.Add({ p.pos, p.startColor.WithAlpha(p.fade), Vector4(p.startSize * p.scale, p.rotation, 0, 0) });
			}
		}

		if(m_deactivated)
		{
			m_finished = !updatedSomething;
		}

		MaterialParameterSet params;
		if(texture)
		{
			params.SetParameter("mainTex", texture);
		}
		material->Bind(rs, params);

		// Select blending mode based on material
		switch(material->blendMode)
		{
		case MaterialBlendMode::Normal:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case MaterialBlendMode::Additive:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		case MaterialBlendMode::Multiply:
			glBlendFunc(GL_SRC_ALPHA, GL_SRC_COLOR);
			break;
		}

		m_mesh->SetData(verts);	
		m_mesh->Draw();
	}

	void ParticleEmitter::Reset()
	{
		m_deactivated = false;
		m_finished = false;
		delete[] m_particles;
		m_particles = nullptr;
		m_emitterLoopIndex = 0;
		m_emitterTime = 0;
		m_spawnCounter = 0;
		m_poolSize = 0;
	}

	void ParticleEmitter::Deactivate()
	{
		m_deactivated = true;
	}
}
