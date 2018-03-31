#ifdef HAS_WINDOW

#include <global_root.h>
#include <engine/display.h>
#include "explosions.h"
#include <objects.h>
//#include "../../version.h"
#include "../../clientRuntimeConfig.h"
#include "../../audio/audioInterface.h"
#include "../../clientEvents.h"
//#include <Nx/physics.h>

extern Engine::Display<Engine::DirectX11>	display;


namespace Explosions
{
	count_t				maxConcurrentParticles = 2000;
	

//	const float			Particle::MaxTTL=1.0f;
	const float         Explosion::ParticleSpeed=1.4f,
						Explosion::SmallerDelay=0.1f,
						Explosion::RelativeExplosionDelay=0.05f;
						//Explosion::ExecuteLength=1.3f;

	/*static*/ DX11::InstructionBlock16		Explosion::ringInstruction;
	/*static*/ DX11::ObjectBuffer<float4>	Explosion::ringColor;

	ExplosionParticles			glowParticles,dustParticles;
	EMPParticles				empParticles;
	DX11::Texture				ringTexture,
								particleTexture;
	Container::Vector0<Explosion,Swappable>	
								explosions,empExplosions;
	DX11::BaseInstructionBlock	particleInstruction;
	DX11::StreamVBO				particleBuffer;
	Engine::OpenAL::Wave		explosionSound,empExplosionSound;






	static String		particle_shader_source = 
						"[vertex]\n"
							"void main()\n"
							"{\n"
								"vec4	position = gl_ModelViewMatrix*gl_Vertex;\n"
								"gl_TexCoord[0] = gl_MultiTexCoord0;\n"
								"gl_Position = gl_ProjectionMatrix*position;\n"
								"vec3	normal = (gl_NormalMatrix*gl_Normal);\n"
								"gl_FrontColor = gl_Color;\n"
							"}\n"
						"[geometry]\n"
							"#version 120 \n"
							"#extension GL_EXT_geometry_shader4 : enable	\n"
							"void main()\n"
							"{\n"
								"vec4 x = gl_ProjectionMatrix[0]*gl_TexCoordIn[0][0].x,\n"
									" y = gl_ProjectionMatrix[1]*gl_TexCoordIn[0][0].y;\n"
								"gl_Position = gl_PositionIn[0] +x -y;\n"
								"gl_FrontColor = gl_FrontColorIn[0];\n"
								"gl_TexCoord[0] = vec4(0,0,0,1);\n"
								"EmitVertex();\n"
								"gl_Position = gl_PositionIn[0] +x +y;\n"
								"gl_FrontColor = gl_FrontColorIn[0];\n"
								"gl_TexCoord[0] = vec4(1,0,0,1);\n"
								"EmitVertex();\n"
								"gl_Position = gl_PositionIn[0] -x -y;\n"
								"gl_FrontColor = gl_FrontColorIn[0];\n"
								"gl_TexCoord[0] = vec4(0,1,0,1);\n"
								"EmitVertex();\n"
								"gl_Position = gl_PositionIn[0] - x + y;\n"
								"gl_FrontColor = gl_FrontColorIn[0];\n"
								"gl_TexCoord[0] = vec4(1,1,0,1);\n"
								"EmitVertex();\n"
								"EndPrimitive();\n"
							"}\n"
						"[fragment]\n"
							"uniform sampler2D texture;\n"
							"void main()\n"
							"{\n"
								"vec4 sampled = texture2D(texture,gl_TexCoord[0].xy);\n"
								"gl_FragColor.rgba = gl_Color*sampled;\n"
							"}";






	void Explosion::RenderRings(const float3&color)
	{
		const float relative = progress/executionTime;

		float3 up = location.Normalized0();



		M::TMatrix4<>	ringMatrix;

		ringColor.Update(float4(color,1.f - M::Sqr(relative)));

		Mat::makeAxisSystem(location,up,2,ringMatrix);
		ringMatrix.x.xyz *= size*relative;
		ringMatrix.y.xyz *= size*relative;
		ringMatrix.z.xyz *= size*relative;
		display.SetObjectMatrix(ringMatrix);
		display.Render(ringInstruction);

		if (relative > SmallerDelay)
		{
			Mat::makeAxisSystem(location,normal1,2,ringMatrix);
			ringMatrix.x.xyz *= size*(relative-SmallerDelay);
			ringMatrix.y.xyz *= size*(relative-SmallerDelay);
			ringMatrix.z.xyz *= size*(relative-SmallerDelay);

			display.SetObjectMatrix(ringMatrix);
			display.Render(ringInstruction);
		}
	}


	bool Explosion::Update(Particles&particles, const float delta, const count_t maxParticleCount)
	{
		bool wasBeforeQuarter = progress < executionTime * 0.25f;
		progress+=delta;
		if (onQuarterDone && wasBeforeQuarter && progress >= executionTime * 0.25f)
		{
			onQuarterDone();
		}

		if (progress >= executionTime)
			return false;
	

		float relative = progress/executionTime;
		if (AudioInterface::isActive && timer.CheckIfOutdatedAndUpdate(lastAudioUpdate,0.2f))
		{
			const float newVolume = 1.f *(1.f-relative);
			source.SetVolume(newVolume);
			for (index_t i = 0; i < ARRAYSIZE(auxiliarySources); i++)
				auxiliarySources[i].SetVolume(newVolume);
		}
		//if (particles.Count() >= maxParticleCount)
		//	return true;
		if (relative > RelativeExplosionDelay && relative <= RelativeExplosionDelay+relativeParticleTime)
		{
			const count_t maxNewParticles = maxConcurrentParticles*delta;
				//maxParticleCount - particles.Count();
			const float fc = size;
			//const float fc = 10;
			count_t newParticles = 
				std::min<count_t>(
					ExplosionParticles*fc*(1.0f-(relative-RelativeExplosionDelay)/relativeParticleTime),
					maxNewParticles
				);
			//
			//const count_t newParticleCount = particles.Count() + newParticles;
			//const count_t threshold = maxParticleCount / 2;
			//if (newParticleCount > threshold)
			//{
			//	float fc = 1.f / (1.f + float(newParticleCount - threshold)*0.1f);
			//		1.f - float(newParticleCount - threshold) / float(maxParticleCount - threshold)*0.9f;
			//	newParticles *= fc;
			//}
		
			Particle*field = particles.AppendRow(newParticles);
			for (index_t i = 0; i < newParticles; i++)
			{
				float3 dir;
				dir.x= (float)rand()/RAND_MAX*2-1;
				dir.y = (float)rand()/RAND_MAX*2-1;
				dir.z = (float)rand()/RAND_MAX*2-1;
				field[i].origin = dir;
			}
			
			Concurrency::parallel_for(index_t(0),newParticles,[this,field,relative](index_t i)
			{
				float3 dir = field[i].origin;
				Vec::normalize0(dir);
				Vec::mult(dir,static_cast<float>(ParticleSpeed)*sqrt(size)*(1.0f-relative));

				Particle&particle = field[i];
				Vec::copy(location,particle.world);
				particle.origin = location;
				particle.normalizedUp = particle.world.Normalized0();
				Vec::copy(dir,particle.inertia);
				particle.maxTTL = particle.ttl = executionTime / 1.3f;
				particle.size = 0.08*size;
				particle.color.alpha = 0.8;
			});
		}
		return true;
	}

	void						Explosion::Create(const M::TVec3<>&position, float size_, float duration, bool isEMP)
	{
		static Random random;

		location = position;
		size = size_;
		executionTime = duration;
		relativeParticleTime = std::min(0.4f, 1.f / duration);
		if (AudioInterface::isActive)
		{
			source.Create();
			source.SetPosition(position);
			source.SetWaveBuffer(isEMP?empExplosionSound : explosionSound);
			source.SetLooping(false);
			source.SetReferenceDistance(sqrt(size)*2.f);
			source.SetVolume(1.f);
			//const float basePitch = 1.f/executionTime;
			const float basePitch = M::Max(0.25f,1.f/executionTime) * (float)pow(1.1f,random.NextFloat(-1.f,1.f));
			source.SetPitch(basePitch);
			source.Play();
			//for (int i = 0; i < ARRAYSIZE(auxiliarySources); i++)
			//{
			//	volatile float pitch = (float)(pow(1.5f,i+1));
			//	volatile float at = basePitch * pitch;
			//	if (at > 1)
			//		break;
			//	Engine::OpenAL::Source&source = auxiliarySources[i];
			//	source.Create();
			//	source.SetPosition(position);
			//	source.SetWaveBuffer(isEMP?empExplosionSound : explosionSound);
			//	source.SetLooping(false);
			//	source.Play();
			//	source.SetVolume(1.f);
			//	source.SetPitch(basePitch * pitch);
			//	source.SetReferenceDistance(20.f);
			//	this->source.SetReferenceDistance(20.f);
			//}

		}
	}

	static const count_t	MaxParticlesPerRow = 512;


	void	Update();

	void Create()
	{
	//	Loading::postMessage("Preparing explosion resources");
		Image  image(256,256,4);
		igRing(image,0.2,CGColor(1,1,1,0),CGColor(1,1,1,0.2),CGColor(1,1,1,0),CGColor(1,1,1,0));
		ringTexture.Load(image);
		igRadial(image,CGColor(1,0.62,0.6,0.3),CGColor(1,0.62,0.6,0));
		particleTexture.Load(image);

		Engine::VertexBinding	binding;
		binding.vertex.Set(0,2);
		binding.normal.Unset();
		binding.color.Unset();
		binding.texcoords.Clear();
		binding.floats_per_vertex = 2;

		Explosion::ringColor.Create(float4(1));

		Explosion::ringInstruction.binding = binding;
		USHORT indices[6] = {0,1,2, 0,2,3 };
		Explosion::ringInstruction.ibo.Create(indices,6);
		float2 vertices[4] = {float2(-1,-1), float2(1,-1), float2(1,1), float2(-1,1)};
		Explosion::ringInstruction.vbo.Create(vertices,4,sizeof(float)*2);

		Explosion::ringInstruction.SetTriangles();
		{
			DX11::Shader::Binding	bindings[] =
			{
				DX11::Shader::Binding(Explosion::ringColor,"Color"),
				DX11::Shader::Binding(ringTexture,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler")
			};
			Explosion::ringInstruction.shader.LoadFromFile("shaders/explosionRing.hlsl","vsMain","psMain",bindings,ARRAYSIZE(bindings));
		}
		Explosion::ringInstruction.UpdateLayout();


		static_assert(sizeof(PackedParticle) == 8 * sizeof(float), "Please adjust struct alignment");
		particleBuffer.Create(MaxParticlesPerRow,sizeof(PackedParticle));
		
		particleInstruction.vbo = particleBuffer;
		
		particleInstruction.binding.Append("PositionSize",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
		particleInstruction.binding.Append("Color",0,DXGI_FORMAT_R32G32B32A32_FLOAT);

		particleInstruction.SetPoints();
		{
			DX11::Shader::Binding	bindings[] =
			{
				DX11::Shader::Binding(particleTexture,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler")
			};
			particleInstruction.shader.LoadFromFile("shaders/explosionParticle.hlsl","vsMain","gsMain","psMain",bindings,ARRAYSIZE(bindings));
		}
		particleInstruction.UpdateLayout();

		if (AudioInterface::isActive)
		{
			explosionSound = Engine::OpenAL::Load("resources/explode.wav");
			if (explosionSound.IsEmpty())
				ErrMessage("Warning: Failed to load required wave file.");
			empExplosionSound = Engine::OpenAL::Load("resources/empExplode.wav");
			if (empExplosionSound.IsEmpty())
				ErrMessage("Warning: Failed to load required wave file.");
		}


		GlobalEvents::frameBegin << Update;

	}

	void				FlushData()
	{
		glowParticles.Clear();
		dustParticles.Clear();
		explosions.Clear();
		empExplosions.Clear();
	}


	void	Destroy()
	{
		FlushData();

		Explosion::ringInstruction = DX11::InstructionBlock16();
		Explosion::ringColor.Clear();
		ringTexture.Clear();
		particleTexture.Clear();
		particleBuffer.Clear();
		particleInstruction = DX11::BaseInstructionBlock();
		if (AudioInterface::isActive)
		{
			Engine::OpenAL::Discard(explosionSound);
			Engine::OpenAL::Discard(empExplosionSound);
		}
		
	}


	void	Particles::Update(const float delta, Particles*shiftDustTo, const count_t maxParticleCount)
	{
		#define SMOKE_BARRIER               0.5f
		#define SMOKE_UPDRIFT               3.0f

		static const float threshold = 0.8f;
		float friction = pow(0.1f,delta);
		bool any_died = false;

		if (Count() > maxParticleCount)
		{
			const float p = 1.f - float(maxParticleCount) / float(Count());
			static Random random;
			foreach (*this,pa)
			{
				if (random.NextFloat() <= p)
					pa->ttl = 0;
			}
		}

		Concurrency::parallel_for(index_t(0),Count(),[this,&any_died,delta,shiftDustTo](index_t i)
		{
			Particle&p = at(i);
			p.ttl -= delta;
			if (p.ttl <= 0 || (shiftDustTo && p.ttl < p.maxTTL * threshold))
				any_died = true;
		});

		if (any_died)
		{
			static Buffer<Particle>	updated_particles;
			updated_particles.reset();
			for (index_t i = 0; i < Count(); i++)
				if (at(i).ttl > 0 && (!shiftDustTo || at(i).ttl >= at(i).maxTTL * threshold))
					updated_particles << at(i);
				else
					if (shiftDustTo)
						(*shiftDustTo) << at(i);
			this->swap(updated_particles);
		}

		Concurrency::parallel_for(index_t(0),Count(),[this,delta,friction](index_t i)
		{
			Particle&p = at(i);
			float relative = p.ttl/p.maxTTL;
									
			M::Vec::mult(p.inertia,M::Clamp01(friction+relative));
			M::Vec::mad(p.world,p.inertia,delta);//*M::Clamp(particle->ttl/(particle->max_ttl/2),0,1));
										
			using M::Sqr;

			float3 pdelta = p.world - p.origin;
			float d = M::Vec::dot(pdelta,p.normalizedUp);
			float3 i2 = pdelta - p.normalizedUp * d;
			p.inertia += p.normalizedUp * delta*SMOKE_UPDRIFT*5 * (1.f - relative);
			p.inertia -= i2 * delta * SMOKE_UPDRIFT * 0.3f * relative;// * (1.f - relative);

			p.color.alpha = M::Sqr(relative)*0.1;
			float r = M::Sqr(M::Sqr(relative));
			M::Vec::mult(particleColor,r,p.color.rgb);
		});
	}



	void		Update()
	{
		
		const count_t particles = CountParticles();
		const count_t MaxParticles = maxConcurrentParticles;

		const float delta = Engine::timing.delta;


		glowParticles.Update(delta,&dustParticles,MaxParticles);
		dustParticles.Update(delta,nullptr,MaxParticles);
		empParticles.Update(delta,nullptr,MaxParticles);
		
		
		for (index_t i = 0; i < explosions.Count(); i++)
		{
			if (!explosions[i].Update(glowParticles,delta,MaxParticles))
			{
				explosions.Erase(i--);
			}
		}
		for (index_t i = 0; i < empExplosions.Count(); i++)
		{
			if (!empExplosions[i].Update(empParticles,delta,MaxParticles))
			{
				empExplosions.Erase(i--);
			}
		}
	}

	static void RenderParticles(const Particles&particles)
	{
		count_t numRows = particles.Count() / MaxParticlesPerRow;
		if (particles.Count() % MaxParticlesPerRow)
			numRows ++;
		//display.SetBlendMode(Engine::BlendMode::AlphaBlend);	//this creates 'blackhole' effects
		if (numRows)
		{
			PackedParticle packed[MaxParticlesPerRow],*const end = packed + MaxParticlesPerRow, *at = packed;
			foreach (particles,p)
			{
				at->positionSize.xyz = p->world;
				at->positionSize.w = p->size;
				at->color = p->color;
				if (Any(at->color < float4(0)))
					ShowMessage(Vec::toString(at->color));
				at ++;

				if (at == end)
				{
					at = packed;
					particleBuffer.PartialUpdate(packed,MaxParticlesPerRow,sizeof(PackedParticle));
					display.RenderVertices(particleInstruction);
				}
			}
			if (at != packed)
			{
				particleBuffer.PartialUpdate(packed,at - packed,sizeof(PackedParticle));
				display.RenderVertices(particleInstruction,0,at - packed);
			}
		}	
	}

	void		Render(const Engine::Aspect<>&aspect)
	{
		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		display.SetDepthTest(true,false);
		display.SetRasterizer(true,false);
		static const float3 explosionColor(1),
							empExplosionColor(0.5,0.6,1);
		foreach (explosions,ex)
			ex->RenderRings(explosionColor);
		foreach (empExplosions,ex)
			ex->RenderRings(empExplosionColor);

		foreach (dustParticles,p)
		{
			p->z = Vec::quadraticDistance(aspect.GetAbsoluteLocation(),p->world);
		}
		std::sort(dustParticles.begin(),dustParticles.end());

		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		RenderParticles(glowParticles);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		RenderParticles(dustParticles);
		display.SetBlendMode(Engine::BlendMode::FlareBlend);
		RenderParticles(empParticles);


		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		display.SetDepthTest(true,true);
		display.SetRasterizer(true,true);

	}
	
	count_t				CountEMPParticles()
	{
		return empExplosions.Count();
	}

	count_t				CountGlowParticles()
	{
		return glowParticles.Count();
	}

	count_t				CountDustParticles()
	{
		return dustParticles.Count();
	}

	count_t				CountParticles()
	{
		return CountEMPParticles() + CountGlowParticles() + CountDustParticles();
	}


	void		InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size/*=25.f*/, float duration /*= 1.3f*/)
	{
		Explosion&explosion = isEMP?empExplosions.Append(): explosions.Append();
		explosion.Create(coordinates,size,duration,isEMP);

		//Physics::signalExplosion(coordinates,1000000.f);
	}

	void		InitExplosion(const M::TVec3<>&coordinates, bool isEMP, float size, float duration, const std::function<void()>&onQuarterDone)
	{
		Explosion&explosion = isEMP? empExplosions.Append() :  explosions.Append();
		explosion.Create(coordinates,size,duration,isEMP);
		explosion.onQuarterDone = onQuarterDone;
	}



}

#endif