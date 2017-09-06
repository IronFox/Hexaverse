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

		M::float3				origin,
								inertia,
								normalizedUp,
								world;
		M::float4				color;


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

		void					Update(float delta, Particles*shiftDustTo)
								{
									#define SMOKE_BARRIER               0.5f
									#define SMOKE_UPDRIFT               3.0f

									static const float threshold = 0.8f;
									float friction = pow(0.1f,delta);
									bool any_died = false;
									for (index_t i = 0; i < count(); i++)
									{
										Particle&p = at(i);
										p.ttl -= delta;
										if (p.ttl <= 0 || (shiftDustTo && p.ttl < p.maxTTL * threshold))
											any_died = true;
									}
									if (any_died)
									{
										static Buffer<Particle>	updated_particles;
										updated_particles.reset();
										for (index_t i = 0; i < count(); i++)
											if (at(i).ttl > 0 && (!shiftDustTo || at(i).ttl >= at(i).maxTTL * threshold))
												updated_particles << at(i);
											else
												if (shiftDustTo)
													(*shiftDustTo) << at(i);
										this->swap(updated_particles);
									}

									for (index_t i = 0; i < count(); i++)
									{
										Particle&p = at(i);
										float relative = p.ttl/p.maxTTL;
									
										M::Vec::mult(p.inertia,M::Clamp01(friction+relative));
										M::Vec::mad(p.world,p.inertia,delta);//*clamped(particle->ttl/(particle->max_ttl/2),0,1));
										
										using M::sqr;

										M::float3 pdelta = p.world - p.origin;
										float d = M::Vec::dot(pdelta,p.normalizedUp);
										M::float3 i2 = pdelta - p.normalizedUp * d;
										p.inertia += p.normalizedUp * delta*SMOKE_UPDRIFT*5 * (1.f - relative);
										p.inertia -= i2 * delta * SMOKE_UPDRIFT * 0.3f * relative;// * (1.f - relative);

								//        particle->position[1] += particle->inertia[1]*eveTiming.delta;

										p.color.alpha = sqr(relative)*0.1;
										float r = sqr(sqr(relative));
										//if (shiftDustTo)
										M::Vec::mult(particleColor,r,p.color.rgb);
											//Vec::def(p.color.rgb,r*particleColor.r,r*particleColor.g,r*particleColor.b);
										//else
											//Vec::set(p.color.rgb,0);
										//Vec::def(p.color.rgb,r,r,r);
										/*_set(particle->color[0],sqrt(relative)*0.5+0.5);
										_set(particle->color[1],sqrt(relative)*0.5+0.5);
										_set(particle->color[2],sqrt(relative)*0.5+0.5);
										_set(particle->color[3],sqrt(relative)*0.5+0.5);*/
									}
								}


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
		static DX11::ObjectBuffer<M::float4>ringColor;


		//Engine::Light				*light_source;
		float						progress,
									size,	//=25
									executionTime;
		M::float3					location,
									normal1,
									color;
		Engine::OpenAL::Source		source,auxiliarySources[6];

		static const float          ParticleSpeed,//=0.7f,
									SmallerDelay,//=0.1f,
									ExplosionDelay,//=0.05f,
									ParticleTime;//=0.2f,
									//NormalizedExecutionTime;//=1.3f,

		static const count_t		ExplosionParticles = 2;

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
		bool						Update(Particles&particles, float delta);
		void						RenderRings(const M::float3&color);
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


	void				Create();
	void				Destroy();
	void				FlushData();
	

	void				Render(const Engine::Aspect<>&);

	inline bool			NoneActive() {return explosions.IsEmpty();}

	void				InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size=25.f, float duration = 1.3f);
	void				InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size, float duration, const std::function<void()>&onQuarterDone);
}








#endif
