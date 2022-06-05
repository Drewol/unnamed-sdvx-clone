#pragma once
#include <Graphics/Material.hpp>
#include <Graphics/Texture.hpp>
#include <Graphics/Mesh.hpp>
#include <Graphics/ParticleParameter.hpp>

namespace Graphics
{
	/*
		Particle Emitter, which is a component of a particle system that handles the emission of particles together with the properties of the emitter particles
	*/
	class ParticleEmitter
	{
	public:
		ParticleEmitter() = default;
		~ParticleEmitter();

		// Material used for the particle
		Material material;

		// Texture to use for the particle
		Texture texture;

		// Emitter location
		Vector3 position;

		// Emitter duration
		float duration = 5.0f;

		float scale = 1.0f;

		// Amount of loops to make
		// 0 = forever
		uint32 loops = 0;

		// Particle parameter accessors
#define PARTICLE_PARAMETER(__name, __type)\
	void Set##__name(const IParticleParameter<__type>& param)\
	{\
		IParticleParameter<__type>* p = static_cast<IParticleParameter<__type>*>(m_param_##__name);\
		if(p)\
			delete p;\
		m_param_##__name = param.Duplicate();\
	}
#include <Graphics/ParticleParameters.hpp>

	// True after all loops are done playing
		bool HasFinished() const { return m_finished; }

		// Restarts a particle emitter
		void Reset();

		// Stop spawning any particles
		void Deactivate();

	private:
		// Constructed by particle system
		ParticleEmitter(class ParticleSystem_Impl* sys);
		void Render(const class RenderState& rs, float deltaTime);
		void m_ReallocatePool(uint32 newCapacity);

		float m_spawnCounter = 0;
		float m_emitterTime = 0;
		float m_emitterRate;
		bool m_deactivated = false;
		bool m_finished = false;
		uint32 m_emitterLoopIndex = 0;
		Mesh m_mesh;
		friend class ParticleSystem_Impl;
		friend class Particle;
		ParticleSystem_Impl* m_system;

		class Particle* m_particles = nullptr;
		uint32 m_poolSize = 0;

		// Particle parameters private
#define PARTICLE_PARAMETER(__name, __type)\
	IParticleParameter<__type>* m_param_##__name = nullptr;
#include <Graphics/ParticleParameters.hpp>
	};
}
