#include "spriteRenderer.h"
#include <io/file_system.h>
#include <image/converter/magic.h>
#include <container/queue.h>
#include <container/array2d.h>

using namespace DeltaWorks;
#undef RGB

namespace Objects
{
	namespace Details
	{



		typedef float (*FLoad)(const BYTE*);

		float GrayAlpha(const BYTE* color)
		{
			return float(color[0]) * float(color[1]) / 65535.f;
		}
		float PlainCast(const BYTE* color)
		{
			return float(color[0]) / 255.f;
		}

		float RGB(const BYTE*color)
		{
			return (float(color[0])*0.3f + float(color[1])*0.59f + float(color[2]) * 0.11f) / (255.f);
		}

		float RGBA(const BYTE*color)
		{
			return RGB(color) * PlainCast(color+3);
		}

		bool IsOpaque(const float*v)
		{
			return (*v) > 0.f;
		}

		void EncodeQuadraticDistance(float d, BYTE classification, float edgeScale, BYTE*rsPixels)
		{
			float fs = std::min(sqrt(d)/edgeScale*4,127.f);
			if (!classification)
				fs = -fs;
			rsPixels[0] = (BYTE)(128 + fs);
		}

		void EncodeQuadraticDistance(float d, BYTE classification, float edgeScale, uint16_t*rsPixels)
		{
			float fs = std::min(sqrt(d)/edgeScale*4*256,32767.f);
			if (!classification)
				fs = -fs;
			rsPixels[0] = (uint16_t)(32768 + fs);
		}
	
	

		struct TDistanceSample
		{
			float d= std::numeric_limits<float>::max();
			int2 origin;
			bool	isset = false;

			static TDistanceSample Self(int2 coords) {return {0,coords,false};}
		};





		bool created = false;
		Engine::DX11::BaseInstructionBlock	blendedInstruction, clippedInstruction;
		Engine::DX11::StreamVBO	instanceBuffer;

	}


	void	SpriteRenderer::Load(const D::PathString&source)
	{
		Image16 img;
		png.LoadFromFile16(img,source);
		texture.Load(img);
	}

	void	SpriteRenderer::CreateOrLoadFromIcon(const D::PathString&source, const D::PathString&cacheTo, bool forceRenew, float edgeScale/*=1.f*/)
	{
		if (!forceRenew && (FileSystem::IsFile(cacheTo) /*&& FileSystem::GetModificationTime(cacheTo) >= FileSystem::GetModificationTime(source)*/))
		{
			Load(cacheTo);
			return;
		}
		using namespace Details;

		Timer::Time t0 = timer.Now();

		Image src;
		Magic::LoadFromFile(src,source);

		FloatImage loaded (src.GetWidth(),src.GetHeight(),1);
		FLoad loader = PlainCast;

		switch (src.GetChannels())
		{
			case 1:
			break;
			case 2:
				loader = GrayAlpha;
			break;
			case 3:
				loader = RGB;
			break;
			case 4:
				loader = RGBA;
			break;
			default:
				FATAL__("Unsupported number of channels");
		}
		Concurrency::parallel_for(index_t(0),src.CountPixels(),[&loaded,loader,&src](index_t idx)
		{
			loaded.GetLinear(idx)[0] = loader(src.GetLinear(idx));
		});
		loaded.TruncateToOpaque(IsOpaque);
		loaded.ScaleDouble();
		loaded.ScaleDouble();


		Queue<index_t> queue;
		Image classified{loaded.GetWidth(),loaded.GetHeight(),1};

		Concurrency::parallel_for(index_t(0),loaded.CountPixels(),[&loaded,loader,&classified](index_t idx)
		{
			classified.GetLinear(idx)[0] = loaded.GetLinear(idx)[0] > 0.5f ? 255 : 0;
		});

		Array2D<TDistanceSample> distanceImage{classified.GetWidth(),classified.GetHeight()};

		for (UINT32 x = 1; x+1 < classified.GetWidth(); x++)
			for (UINT32 y = 1; y+1 < classified.GetHeight(); y++)
			{
				if (classified.Get(x,y)[0])
				{
					for (UINT32 x1 = x-1; x1 <= x+1; x1++)
						for (UINT32 y1 = y-1; y1 <= y+1; y1++)
							if (!classified.Get(x1,y1)[0])
							{
								distanceImage.Get(x,y) = TDistanceSample::Self(int2(x,y));
								queue.Push(distanceImage.ToLinearIndex(x,y));
								break;
							}
				}
			}

		count_t numProcessed = 0;
		index_t at;
		while (queue >> at)
		{
			auto&v = distanceImage[at];
			if (v.isset)
				continue;
			numProcessed++;
			v.isset = true;
			const auto c = distanceImage.ToVectorIndex(at);
			c.IterateNeighborhood([&distanceImage,v,&queue](const auto&vidx)
			{
				float d= Vec::quadraticDistance(int2(vidx.x,vidx.y),v.origin);
				auto&v1 = distanceImage.Get(vidx);
				if (d >= v1.d)
					return;
				v1.isset = false;
				v1.d = d;
				v1.origin = v.origin;
				queue << distanceImage.ToLinearIndex(vidx);
			},distanceImage.GetSize());
		}
		ASSERT_GREATER_OR_EQUAL__(numProcessed,distanceImage.Count());

		Image16 result{classified.GetWidth(),classified.GetHeight(),1};
		Concurrency::parallel_for(index_t(0),classified.CountPixels(),[&distanceImage,&result,&classified,edgeScale](index_t idx)
		{
			EncodeQuadraticDistance(distanceImage[idx].d,classified.GetLinear(idx)[0],edgeScale,result.GetLinear(idx));
		});
		result.ScaleHalf();
		result.ScaleHalf();
		png.SaveToFile16(result,cacheTo,9);

		//ShowMessage(timer.GetSecondsSince(t0));


		texture.Load(result);
	}


	void		SpriteRenderer::Render(const Instance&inst)
	{
		instances << inst;
		if (instances.Count() == MaxInstancesPerRow)
			Flush();
	}

	void		SpriteRenderer::Flush()
	{
		if (instances.IsEmpty())
			return;
		using namespace Details;
		instanceBuffer.PartialUpdate(instances.GetPointer(),instances.Count(),sizeof(Instance));
		clippedInstruction.quickSlots.First() = texture;
		blendedInstruction.quickSlots.First() = texture;
		display.SetDepthTest(true,true);
		display.SetBlendMode(Engine::BlendMode::None);
		display.RenderVertices(clippedInstruction,0,(UINT32)instances.Count());
		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		display.RenderVertices(blendedInstruction,0,(UINT32)instances.Count());
		instances.Clear();
	}


	void	SpriteRenderer::Baked::Render() const
	{
		if (clipped.vbo.IsEmpty())
			return;
		display.SetDepthTest(true,true);
		display.SetBlendMode(Engine::BlendMode::None);
		display.RenderVertices(clipped);
		display.SetDepthTest(true,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		display.RenderVertices(blended);
	}

	SpriteRenderer::Baked		SpriteRenderer::Bake(const D::ArrayRef<Instance>&instances) const
	{
		if (instances.IsEmpty())
			return Baked();
		using namespace Details;
		Baked rs;
		rs.blended = blendedInstruction;
		rs.clipped = clippedInstruction;
		rs.blended.quickSlots.First() =
			rs.clipped.quickSlots.First() =
			texture;
		Engine::DX11::VBO vbo;
		vbo.CreateFromStructs(instances);
		rs.blended.vbo = rs.clipped.vbo = vbo;
		return rs;
	}


	/*static*/ void	SpriteRenderer::StaticCreate()
	{
		using namespace Details;
		namespace DX11 = Engine::DX11;
		if (created)
			return;
		created = true;


		static_assert(sizeof(Instance) == 9 * sizeof(float), "Please adjust struct alignment");
		instanceBuffer.Create(MaxInstancesPerRow,sizeof(Instance));

		clippedInstruction.vbo = instanceBuffer;
		clippedInstruction.binding.Append("Position",0,DXGI_FORMAT_R32G32B32_FLOAT);
		clippedInstruction.binding.Append("Size",0,DXGI_FORMAT_R32G32_FLOAT);
		clippedInstruction.binding.Append("Color",0,DXGI_FORMAT_R32G32B32A32_FLOAT);

		clippedInstruction.quickSlots.SetSize(1);
		clippedInstruction.SetPoints();
		{
			DX11::Shader::Binding	bindings[] =
			{
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler")
			};
			DX11::Shader::macroDefinitions << DX11::Shader::Macro("CLIPPED","");
			clippedInstruction.shader.LoadFromFile("shaders/sprite.hlsl","vsMain","gsMain","psMain",bindings,ARRAYSIZE(bindings));
			DX11::Shader::macroDefinitions.EraseLast();
		}
		clippedInstruction.UpdateLayout();

		blendedInstruction = clippedInstruction;
		{
			DX11::Shader::Binding	bindings[] =
			{
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler")
			};
			blendedInstruction.shader.LoadFromFile("shaders/sprite.hlsl","vsMain","gsMain","psMain",bindings,ARRAYSIZE(bindings));
		}
		blendedInstruction.UpdateLayout();


	}

	


}
