#include <global_root.h>
#include "resourceGauges.h"


#include "surfaceRenderer.h"
#include "../display.h"
#include "../objectSelection.h"

#include <image/converter/magic.h>
#include <general/random.h>

namespace HUD 
{
namespace ResourceGauges 
{

	namespace DX11 = Engine::DX11; //!< Shortcut



	//#ifdef HAS_WINDOW
	//	/*static*/ index_t			Hangar::storeOffset;
	//	/*static*/ Command::PBase	Hangar::cmdLeft,
	//								Hangar::cmdRight;
	//	/*static*/ int2				Hangar::cmdLeftCoords,
	//								Hangar::cmdRightCoords;

	//#endif



	void	IconItem::LoadTexture(const String&name)
	{
		Image image;
		Magic::LoadFromFile(image,PathString("resources/icons/"+name+".png"));
		texture.Load(image);
	}
		
	void IconItem::RenderIcon(float size/*=0.05f*/)
	{
		iconColor.Update(float4(color*0.75f + 0.25f,size));
		iconShader.quickSlots.First() = texture;
		display.RenderVertices(iconShader,0,1);
	}





	void	Gauge::Set(int current, int max, int changePerRound, int localCurrent, int localChange, bool enabled)
	{

		//max = std::max(10.f,max);
		if (current == this->current && max == this->max && changePerRound == this->changePerRound
			&& localCurrent == this->localCurrent && localChange == this->localChange && enabled == this->enabled)
			return;
		top0 = std::max(current,current+changePerRound);
		top1 = std::max(max,top0);
		top = std::max(abs(current+localChange), top1);
		this->current = current;
		this->max = max;
		this->changePerRound = changePerRound;
		this->localCurrent = localCurrent;
		this->localChange = localChange;
		this->enabled = enabled;
		ftop = std::max(1,top);
		hScale = height / float(ftop);
		#ifndef NO_HIBERNATION
			if (isTraffic)
			{
				hScale *= 1.5f;
				float toRed = std::min(float(current+changePerRound)/float(max),1.f);
				Vec::HSL2RGB(float3(0.5f - 0.5f * toRed,toRed*0.5f,1.f - toRed*0.5f),color);
			}
		#endif
		changed = true;
	}




	/*static*/ void Gauge::StaticCreate()
	{
		shader.shader.LoadFromFile("shaders/gauge.hlsl","vsMain", "psMain");
		shader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
		shader.binding.Append("NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT);
		shader.binding.Append("COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
		shader.UpdateLayout();
		shader.SetTriangles();

		//iconShader.shader.LoadFromFile("shaders/gaugeIcon.hlsl","vsMain", "psMain");
		//
		//iconShader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
		//shader.binding.Append("NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT);
		//shader.binding.Append("COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
		//shader.UpdateLayout();
		//shader.SetTriangles();

		//iconCenter.Create(float4(0));
		iconColor.Create(float4(0));

		iconShader.quickSlots.SetSize(1);

		DX11::Shader::Binding bindings[] =
		{
			DX11::Shader::Binding(0,"Texture"),
			DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler"),
//				DX11::Shader::Binding(iconCenter,"Center"),
			DX11::Shader::Binding(iconColor,"Color"),
		};

		count_t numSet = DX11::Shader::macroDefinitions.Count();
		//DX11::Shader::RedefineMacro(DX11::Shader::Macro("ICON_SIZE",0.1f));
		iconShader.shader.LoadFromFile("shaders/gaugeIcon.hlsl","vsMain", "gsMain", "psMain",bindings, ARRAYSIZE(bindings));

		DX11::Shader::macroDefinitions.Truncate(numSet);
		iconShader.UpdateLayout();
		iconShader.SetPoints();

	}

	/*static*/ void Gauge::StaticDestroy()
	{
		shader.Clear();
		iconShader.Clear();
		iconColor.Clear();
		//foreach (gauges,g)
		//	g->texture.Clear();

	}


	void	Gauge::UpdateTextProjection(const Engine::Aspect<>&camera)
	{
		//camera.Project(base + float3(0,0,float(top) / ftop * height * 1.1f),projectedTop);
		camera.ProjectToRegion(base + float3(-0.0125f * log(std::max(1,current))/log(10.f),0.f,0.f) ,projectedBottom);
		//projectedBottom.x -= 0.0125f * log(std::max(1,current))/log(10.f);
		camera.ProjectToRegion(base + float3(0,0,float(current + changePerRound) * hScale),projectedDelta);
		camera.ProjectToRegion(base + float3(0,0,float(max) * hScale),projectedMax);
		camera.ProjectToRegion(base + float3(0,0,float(std::max(max, current+changePerRound)) * hScale),projectedTop);
	}

	void	Gauge::Render(BYTE target)
	{
		if (changed)
		{
			Construct();
		}
		M::TMatrix4<> system = M::Matrix<>::eye4;
		system.w.xyz = base;
		system.z.xyz *= hScale;
		#ifndef NO_HIBERNATION
			if (isTraffic)
			{
				system.x.xyz *= 0.75f;
				system.y.xyz *= 0.75f;
			}
		#endif

		system.x.xyz *= 1.5f;
		system.y.xyz *= 1.5f;

		display.SetObjectMatrix(system);

		display.Render(instruction[target]);


		//display.SetObjectMatrix()
		//if (!target)
		//{
		//	textout.PushState();
		//	textout.SetScale(0.2,0.2);
		//	display.SetRasterizer(true,false);
		//	textout.locate(0,0,1);
		//	textout.SetColor(1,1,1);
		//	textout << "test";
		//	textout.PopState();

		//}

	}


	static void Write(int value)
	{
		if (value < 0)
		{
			textout << '-';
			Write(-value);
			return;
		}
		if (value < 2000)
		{
			textout << value;
			return;
		}
		if (value < 2000000)
		{
			int v = (int)round(float(value)/1000.f);
			textout << v << 'k';
			return;
		}
		int v = (int)round(float(value)/1000000.f);
		textout << v << 'm';
		return;
	}

	void	Gauge::RenderListText(int y)
	{
		textout.PushState();
		//textout.locate(projectedTop);
		textout.MoveTo(-1.f + 0.01f,-1.f + textout.GetScaledHeight()*1.2f*(y+5));

		//textout.SetLine(-1);
		textout << myTag;
			
			
		textout.SetColor(color * 0.5f + float3(0.5f));
		if (changePerRound > 0)
		{
			textout << "+";
			Write(changePerRound);
		}
		else
			if (changePerRound < 0)
			{
				textout.SetColor(1,0.6,0.5);
				Write(changePerRound);
				textout.SetColor(color * 0.5f + float3(0.5f));
			}
		if (max > 0)
			textout.Scale(1.2f,1.2f);
		if (max > 0)
		{
			textout << ' ';
			Write(current);
			textout.Scale(0.6f,0.6f);
			textout <<'/';
			Write(max);
			textout.Scale(1.f/0.6f,1.f/0.6f);
		}
		//textout << max;
		textout.PopState();


	}

	void	Gauge::RenderText()
	{
		//textout.SetScale(0.2,0.2);	//leave it
		//display.SetRasterizer(true,false);
		{
			textout.PushState();
			textout.SetColor(color * 0.5f + float3(0.5f));
			textout.MoveTo(projectedBottom);
			textout.SetLine(-1);
			textout << current << nl;
			textout.PopState();
		}
		if (max)
		{
			textout.PushState();
			textout.MoveTo(projectedMax);
			textout.Scale(0.6f,0.6f);
			textout.SetLine(-1);
			textout << max;
			textout.PopState();
		}

		if (changePerRound != 0)
		{
			textout.PushState();
			textout.MoveTo(projectedDelta.xy);
			textout.Scale(0.8f,0.8f);
			textout.SetLine(-1);
			if (changePerRound > 0)
			{
				textout.SetColor(color);
				textout << '+';
			}
			else
				textout.SetColor(1,0.6,0.5);
			textout << changePerRound;
			textout.PopState();
		}
			
		{
			textout.PushState();
			//textout.locate(projectedTop);
			textout.Scale(0.6f,0.6f);
			textout.MoveTo(projectedTop.x,projectedTop.y + textout.GetScaledHeight()*2.5f);

			//textout.SetLine(-1);
			textout << myTag;
			//textout << max;
			textout.PopState();
		}
			
		//RenderIcon(projectedTop);
	}


	void	Gauge::ConstructPart(float radius, float lower, float upper, const float4&lowerColor, const float4&upperColor, Objects::Geometry&geo)
	{
		for (int i = 0; i < 6; i++)
		{
			float a = float(i)/6.f * 2.f * M_PIF;
			float b = float(i+1)/6.f * 2.f * M_PIF;
			float ab = (a + b)/2.f;

			geo.SetVertexOffsetToCurrent();

			float3 normal(cos(ab),-sin(ab),0);
			float2 av(cos(a),-sin(a));
			av *= radius;
			float2 bv(cos(b),-sin(b));
			bv *= radius;

			float3	na = float3(bv,0),//  normal,
					nb = float3(av,0);//	normal;

			geo.MakeVertex(float3(av,lower),na,lowerColor);
			geo.MakeVertex(float3(bv,lower),nb,lowerColor);
			geo.MakeVertex(float3(bv,upper),nb,upperColor);
			geo.MakeVertex(float3(av,upper),na,upperColor);

			geo.MakeQuadInv(0,1,2,3);
		}
			
		geo.SetVertexOffsetToCurrent();

		for (int i = 0; i < 6; i++)
		{
			float a = float(i)/6.f * 2.f * M_PIF;
			geo.MakeVertex(float3(cos(a) * radius,-sin(a) * radius,upper),float3(0,0,1),upperColor);
		}
		for (int i = 2; i < 6; i++)
			geo.MakeTriangleInv(0,i-1,i);
	}

	void	Gauge::Construct()
	{
		changed = false;

		geo[0].Clear();
		geo[1].Clear();
		for (int i = 0; i < 6; i++)
		{
			float a = float(i)/6.f * 2.f * M_PIF;
			geo[0].MakeVertex(float3(cos(a),-sin(a),0),float3(0,0,-1),color);
		}
		for (int i = 2; i < 6; i++)
			geo[0].MakeTriangle(0,i-1,i);

		//float cap = std::min(max,current + std::max(0.f,changePerRound));
		//float h = cap / max * height;

		if (changePerRound >= 0)
		{
			if (localCurrent > 0)
			{
				ConstructPart(1.f,0,localCurrent,float4(1,0,0,0.95f),geo[0]);
				ConstructPart(1.f,localCurrent,current,float4(color,0.95f),geo[0]);
			}
			else
				ConstructPart(1.f,0,current,float4(color,0.95f),geo[0]);
			if (changePerRound > 0)
			{
				int top = current + std::max(0,changePerRound - (enabled ? localChange : 0));
				//int top1 = std::min(max,top);
				ConstructPart(0.8f,current,top,float4(color,0.f),float4(color,0.6f),geo[0]);

				//if (top != top1)
					//ConstructPart(0.8f,top1,top,float4(color,0.f),float4(color,0.6f),geo[0]);
			}
		}
		else
		{
			if (localCurrent > 0)
			{
				int step = std::min(localCurrent,std::max(0,(current+changePerRound)));
				ConstructPart(1.f,0,step,float4(1,0,0,0.95f),geo[0]);

				ConstructPart(1.f,step,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
			}
			else
			{
				ConstructPart(1.f,0,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
			}
			ConstructPart(1.f,std::max(0,(current+changePerRound)),current,float4((color + float3(1,0.5,0.5))/1.5f,0.95f),geo[0]);
		}

		if (localChange != 0)
		{
			float t = current+changePerRound;
			if (localChange > 0)
			{
				if (enabled)
					ConstructPart(0.6f,t- localChange,t,float4(1,0,0,0.f),float4(1,0,0,0.6f),geo[0]);
				else
					ConstructPart(0.6f,t,t + localChange,float4(1,0.5f,0,0.f),float4(1,0.5f,0,0.6f),geo[0]);
			}
			else
				if (enabled)
					ConstructPart(0.6f,t,t-localChange,float4(1,0,0,0.6f),float4(1,0,0,0.f),geo[0]);
				else
					ConstructPart(0.6f,t+localChange,t,float4(1,0.5f,0,0.6f),float4(1,0.5f,0,0.f),geo[0]);
				//ConstructPart(0.8f,current,top1,float4(color,0.f),float4(color,0.6f),geo[0]);
		}

		ConstructPart(1.f,0,max,float4(float3(1),0.25),geo[1]);




		for (BYTE k = 0; k < 2; k++)
		{
			instruction[k].SetBase(shader);
			instruction[k].vbo.CreateFromStructs(geo[k].GetLOD(0).vertices);
			instruction[k].ibo.Create(geo[k].GetLOD(0).indices);
		}
	};



	/*static*/ const float Gauge::height = 10.f;

	/*static*/ DX11::BaseInstructionBlock	Gauge::shader,IconItem::iconShader;
	/*static*/ DX11::ObjectBuffer<float4>	/*IconItem::iconCenter,*/IconItem::iconColor;

	Array<Gauge>	gauges;
	#ifndef NO_HIBERNATION
		Gauge		trafficGauge;
	#endif
	IconItem		timeResource;

	Engine::VectorCamera<>	barProjectionCamera;
	Engine::OrthographicAspect<> textoutCamera;
	bool			resourcesAsList = false;





	void Create()
	{
		Gauge::StaticCreate();



		//camera.location = float3(0,-100,0);
		barProjectionCamera.retraction = float3(0,0,-120);
		barProjectionCamera.direction = float3(0,-1,0.3).Normalized();
		barProjectionCamera.upAxis = float3(0,0,1);
		barProjectionCamera.UpdateView();
		barProjectionCamera.UpdateProjection2(1.f,0.1f,1000,35.f);
		barProjectionCamera.depthTest = Engine::NoDepthTest;
		//barProjectionCamera.region.x.max = 1.f / display.GetPixelAspect();
		barProjectionCamera.region.x.max = 0.9f;

		textoutCamera.UpdateProjection(-1,-1,1,1,-1,1);
		textoutCamera.depthTest = Engine::NoDepthTest;



		static const float stepX = (0.5f + 1.f) * 2.2f * 1.2f HIB(/1.2f);
		static const float stepY = 0.86602540378443864676372317075294f * 2.2f;
//			const float3 base(-38,0,-36);
		const float3 base(-33,0,-36);
		Random random(1024);

		gauges.SetSize(Objects::ResourceVector::NumEntries);
		#ifndef NO_HIBERNATION
			trafficGauge.isTraffic = true;
		#endif

		for (index_t i = 0; i < gauges.Count(); i++)
		{
			Gauge&g = gauges[i];
			g.base.x = float(i HIB(+2)) * (stepX );
			g.base.y = (1.f - float(i%2)) * stepY;
			g.base += base;
			g.LoadTexture(Objects::Resource::Reinterpret(i));
			//test.Construct();
		}
		#ifndef NO_HIBERNATION
		{
			Gauge&g = trafficGauge;
			g.base.x = 0;
			g.base.y = stepY;
			g.base += base;

			g.IconItem::LoadTexture("Traffic");
			g.myTag = Objects::TrafficResourceTag();
			g.color = float3(0.1,1,1);
		}
		#endif


		timeResource.color = float3(1);
		timeResource.LoadTexture("time");



		gauges[Objects::Resource::Energy].color = float3(0.2,0.4,1);
		gauges[Objects::Resource::Fuel].color = float3(0.4,1,0.2);
		gauges[Objects::Resource::Minerals].color = float3(0.7,0.7,0.7);
		#ifndef NO_MUNITION
			gauges[Objects::Resource::Munitions].color = float3(1,0.4,0.2);
		#endif
		gauges[Objects::Resource::Uranium].color = float3(1,1,0.1);
		gauges[Objects::Resource::Uranium].alternatingColor = float3(0.1,0.1,0.1);
		gauges[Objects::Resource::Uranium].usesAlternatingColors = true;
	}


	static bool showExtraGauges = false;

	#ifndef NO_HIBERNATION

	
		void	UpdateTraffic(int traffic)
		{
			showExtraGauges = true;
			int gain = Objects::uplinkTraffic - Objects::onlineUplinkTraffic;


			if (Objects::onlineUplinkTraffic*2 > Network::uplinkBandwidth)
				Tutorial::ShowMessage("Uplink Traffic");


			trafficGauge.Set(Objects::onlineUplinkTraffic,Network::uplinkBandwidth,gain,traffic,0,true);
		}
	#endif

	void	UpdateTraffic()
	{
		if (Objects::selectedEntity)
		{
			#ifndef NO_HIBERNATION
			UpdateTraffic(
				Objects::selectedEntity->GetUplinkTraffic()
				);
			#else
				showExtraGauges = true;

			#endif
		}
		else
			HideExtraGauges();
	}

	void	HideExtraGauges()
	{
		showExtraGauges = false;
		#ifndef NO_HIBERNATION
			int gain = Objects::uplinkTraffic - Objects::onlineUplinkTraffic;


			if (Objects::onlineUplinkTraffic*2 > Network::uplinkBandwidth)
				Tutorial::ShowMessage("Uplink Traffic");

			trafficGauge.Set(Objects::onlineUplinkTraffic,Network::uplinkBandwidth,gain,0,0,true);
		#endif
	}

	void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change)
	{
		#ifndef NO_HIBERNATION
			UpdateTraffic(traffic);
		#else
			showExtraGauges = true;
		#endif
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			gauges[i].Set(current.level[i],max.level[i],change.level[i],0,0,true);

	}

	void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max)
	{
		#ifndef NO_HIBERNATION
			UpdateTraffic(traffic);
		#else
			showExtraGauges = true;
		#endif
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
			gauges[i].Set(current.level[i],max.level[i],0,0,0,true);

	}

	void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change,
					const Objects::ResourceVector&localCurrent/*, const Objects::ResourceVector&localChange, bool isEnabled*/)
	{
		#ifndef NO_HIBERNATION
			UpdateTraffic(traffic);
		#else
			showExtraGauges = true;
		#endif
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
//			resources[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],localChange.level[i],isEnabled);
			gauges[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],0,true);
	}

	void	Render()
	{
		if (!showControlsAndContacts /*|| !Objects::selectedEntity*/)
			return;
		display.StoreRendererState();
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);
		display.SetRasterizer(true,true);
		display.SetDepthTest(false,false);


		if (resourcesAsList)
		{
			display.Pick(textoutCamera);
				textout.PushState();
				textout.Scale(2.5f,2.5f);
				float lineHeight = textout.GetScaledHeight()*1.2f;
				SurfaceRenderer::SetColor(float4(0,0,0,0.6f));
				float w = 0.15f / display.GetPixelAspect();
				//textout.locate(-1.f + 0.01f,-1.f + textout.GetScaledHeight()*1.2f*(y+6));
				M::Rect<> t(-1,lineHeight*5.f-1.f,w*2.f - 1.f,lineHeight*11.5f - 1.f);
				SurfaceRenderer::Rasterize(
					float2(t.x.min,t.y.min),float4(0,0,0,0.8f),
					float2(t.x.max,t.y.min),float4(0,0,0,0.2f),
					float2(t.x.max,t.y.max),float4(0,0,0,0.2f),
					float2(t.x.min,t.y.max),float4(0,0,0,0.8f)
				);
				SurfaceRenderer::Flush();
				//textout.Scale(4.f,4.f);
				//textout.SetColor(1,1,1);

				#ifndef NO_HIBERNATION
					trafficGauge.RenderListText(0);	//bottom
				#endif
				if (showExtraGauges)
				{
					index_t at = 2;
					foreach (gauges,res)
					{
						res->RenderListText((int)(at++));
					}
				}

				textout.PopState();
		}
		else
		{

			barProjectionCamera.region.x.max = 1.f / display.GetPixelAspect();
			//(sin(Engine::timing.now)*0.5f+0.5f) + 0.5f;

			display.Pick(barProjectionCamera);



			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			#ifndef NO_HIBERNATION
				trafficGauge.Render(0);
			#endif
			if (showExtraGauges)
			{
				for (index_t i = 0; i < gauges.Count(); i+=2)
					gauges[i].Render(0);
				for (index_t i = 1; i < gauges.Count(); i+=2)
					gauges[i].Render(0);
			}

			display.SetBlendMode(Engine::BlendMode::FlareBlend);
			#ifndef NO_HIBERNATION
				trafficGauge.Render(1);
			#endif

			if (showExtraGauges)
			{
				for (index_t i = 0; i < gauges.Count(); i+=2)
					gauges[i].Render(1);
				for (index_t i = 1; i < gauges.Count(); i+=2)
					gauges[i].Render(1);
			}


			display.SetBlendMode(Engine::BlendMode::AlphaBlend);

			#ifndef NO_HIBERNATION
				trafficGauge.UpdateTextProjection(barProjectionCamera);
			#endif
			if (showExtraGauges)
				foreach (gauges,res)
				{
					res->UpdateTextProjection(barProjectionCamera);
				}

			display.Pick(textoutCamera);
				textout.PushState();
				textout.Scale(4.f,4.f);
				textout.SetColor(1,1,1);

				#ifndef NO_HIBERNATION
					trafficGauge.RenderText();
				#endif
				if (showExtraGauges)
					foreach (gauges,res)
					{
						res->RenderText();
					}

				textout.PopState();
			


		}

		display.RestoreRendererState();

	}



	void Destroy()
	{
		Gauge::StaticDestroy();
		gauges.Clear();
		#ifndef NO_HIBERNATION
			trafficGauge.Destroy();
		#endif
		timeResource.texture.Clear();


	}




		


	
}
}

