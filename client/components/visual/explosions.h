#pragma once

#ifdef HAS_WINDOW

#include <math/vclasses.h>
#include <image/img_generators.h>
#include <al/openal.h>
#include <container/buffer.h>

#include "../../ExtRenderer/DirectX11.h"

namespace Explosions
{
	namespace DX11 = Engine::DX11;
	namespace M = DeltaWorks::Math;
	namespace Container = DeltaWorks::Container;

	using namespace DeltaWorks;

	struct PackedParticle
	{
		M::TVec4<>				positionSize,
								color;
	};

	struct Particle
	{
		float					ttl,
								maxTTL,
								size,
								z;
		const static float		BaseTTL;//=0.5f;

		float3				origin,
								inertia,
								normalizedUp,
								world;
		float4				color;


		bool					operator<(const Particle&other) const
		{
			return z > other.z;
		}
	};



	class Particles : public Container::Vector0<Particle>
	{
	public:
		M::TVec3<>				particleColor;

		/**/					Particles(const M::TVec3<>&particleColor):particleColor(particleColor)	{}
		/**/					Particles(float particleColorR, float particleColorG, float particleColorB){M::Vec::def(particleColor,particleColorR,particleColorG,particleColorB);}

		void					Update(const float delta, Particles*shiftDustTo, const count_t maxParticleCount);
	};

	class ExplosionParticles : public Particles
	{
	public:
		/**/					ExplosionParticles():Particles(2.5f,1.5f,1.f)	{}
	};
	
	class EMPParticles : public Particles
	{
	public:
		/**/					EMPParticles():Particles(1.f,1.5f,2.5f)	{}
	};

	class Explosion
	{
	public:
		static DX11::InstructionBlock16		ringInstruction;
		static DX11::ObjectBuffer<float4>ringColor;


		//Engine::Light				*light_source;
		float						progress,
									size,	//=25
									executionTime;
		float3						location,
									normal1,
									color;
		Engine::OpenAL::Source		source,auxiliarySources[4];

		Timer::Time					lastAudioUpdate = 0;

		static const float          ParticleSpeed,//=0.7f,
									SmallerDelay,//=0.1f,
									/**
									Relative time (to executionTime) that must pass before any particles are spawned
									*/
									RelativeExplosionDelay;//=0.05f,

		/**
		Relative time (to executionTime) that the explosion keeps spewing new particles.
		Actual particle numbers per second start high and linearly scale down to 0
		*/
		float						relativeParticleTime = 0.4f;
									//NormalizedExecutionTime;//=1.3f,

		static constexpr const float		ExplosionParticles = 100;

		std::function<void()>		onQuarterDone;

		void						swap(Explosion&other)
									{
									//	swp(light_source,other.light_source);
										swp(progress,other.progress);
										swp(size,other.size);
										swp(location,other.location);
										swp(normal1,other.normal1);
										swp(color,other.color);
										swp(executionTime,other.executionTime);
										source.swap(other.source);
										for (index_t i = 0; i < ARRAYSIZE(auxiliarySources); i++)
											auxiliarySources[i].swap(other.auxiliarySources[i]);
										onQuarterDone.swap(other.onQuarterDone);
									}

									Explosion():/*light_source(NULL),*/progress(0),color(1,0.9,0.8)
									{
										normal1.x = (float)rand()/RAND_MAX*2-1;
										normal1.y = (float)rand()/RAND_MAX*2-1;
										normal1.z = (float)rand()/RAND_MAX*2-1;

										M::Vec::normalize0(normal1);
									}
		void						Create(const M::TVec3<>&position, float size_, float duration, bool isEMP);
		bool						Update(Particles&particles, const float delta, const count_t maxParticleCount);
		void						RenderRings(const float3&color);
	};


	//extern ExplosionParticles			glowParticles,dustParticles;
	//extern EXMP
	extern DX11::Texture				ringTexture,
										particleTexture;
	extern Container::Vector0<Explosion,Swappable>	
										explosions;
	extern DX11::BaseInstructionBlock	particleInstruction;
	extern DX11::StreamVBO				particleBuffer;
	extern Engine::OpenAL::Wave			explosionSound,empExplosionSound;

	/**
	Controls the maximum number of concurrently active particles in any one particle container.
	Also controls the maximum number of particles that can be newly created per second and explosion.
	The default value is 2000.
	Can be lowered to 500 on lower end systems, and increased to 100000 and more on higher end systems.
	*/
	extern count_t		maxConcurrentParticles;


	void				Create();
	void				Destroy();
	void				FlushData();
	
	count_t				CountEMPParticles();
	count_t				CountGlowParticles();
	count_t				CountDustParticles();
	count_t				CountParticles();

	void				Render(const Engine::Aspect<>&);

	inline bool			NoneActive() {return explosions.IsEmpty();}

	void				InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size=25.f, float duration = 1.3f);
	void				InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size, float duration, const std::function<void()>&onQuarterDone);
}








#endif
