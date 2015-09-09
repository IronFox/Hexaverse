#ifdef HAS_WINDOW

#include <global_root.h>
#include "guiElements.h"
#include <engine/display.h>
#include "../ExtRenderer/DirectX11.h"
#include "../DX11/texture_font.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include <engine/aspect.h>
#include "../../shared/objects.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace GUIElements
{
	index_t windowResolutionVersion = 0;

	static void				RenderArea(const Rect<>&rect, bool lit, bool flip)
	{
		float low = /*hovering ? 0.5f : */0.3;
		float high = lit ? 0.8f : 0.6;
		if (flip)
			std::swap(low,high);

		SurfaceRenderer::Rasterize(float2(rect.x.min,rect.y.min), float4(1,1,1,low), float2(rect.x.max,rect.y.min),float4(1,1,1,low), float2(rect.x.max,rect.y.max), float4(1,1,1,high), float2(rect.x.min,rect.y.max),float4(1,1,1,high));
		SurfaceRenderer::Flush();
	}
	

	static float CharWidth(char c)
	{
		if ((index_t)c >= Objects::TextoutResourceTagOffset && (index_t)c <= (index_t)Objects::TextoutResourceTagOffset + Objects::Resource::NumberOfPossibilities)
		{
			return textout.getFont().getHeight() * textout.state.scale_x;
		}
		return textout.getFont().getWidth(c) * textout.state.scale_x;
	}

	void OnWindowResize()
	{
		windowResolutionVersion++;
		if (windowResolutionVersion == InvalidIndex)
			windowResolutionVersion++;
	}


	static const float extraSpacing = 0.75f,
						textZoom = 1.5f;

	bool			TextField::UpdateIfNecessary()
	{
		if (this->sizeVersion == windowResolutionVersion)
			return false;
		this->sizeVersion = windowResolutionVersion;
		textout.pushState();
		textout.scale(textZoom*zoom,textZoom*zoom);

		wrappedText.Clear();
		Array<String>	wrapped;
		for (const String&raw : rawText)
		{
			wrapf(raw,rect.width()*0.9f,CharWidth,wrapped);
			wrappedText.MoveAppend(wrapped);
		}
		lineAt.SetSize(wrappedText.Count());
		float lineHeight = textout.getScaledHeight();
		float y = -lineHeight*(extraSpacing);
		for (index_t i = 0; i < lineAt.Count(); i++)
		{
			lineAt[i] =
				float2(
					alignCenter ? 
						rect.x.center() - textout.getScaledWidth(wrappedText[i])*0.5f
						:
						rect.x.min,
				y - lineHeight * (i));
		}
		this->rect.y.min = this->rect.y.max - textout.getScaledHeight() * (float(wrappedText.Count()) + extraSpacing*2.f);
		textout.popState();
		Super::UpdateNow();

		return true;
	}




	void				Area::Render(bool lit)
	{
		RenderArea(rect,lit,false);
	}

	static const float CellSize = 0.01f;

	void			Area::UpdateNow()
	{
	//	hexCenters.Clear();

	//	const float aspect = display.pixelAspect();
	//	const float hexHeight = (0.5f + 1.f) * CellSize;
	//	const float hexWidth = 0.86602540378443864676372317075294f * CellSize * 0.5f / aspect;

	//	Rect<> source = Super::rect;
	//	//std::swap(source.y.max,source.y.min);
	//	//source.y.min = 1.f - source.y.min;
	//	//source.y.max = 1.f - source.y.max;
	//	//source.y.Translate(-0.5f);

	//	Rect<int>	range = Rect<int>(
	//	source.x.min / hexWidth,
	//	source.y.min / hexHeight, 
	//	ceil(source.x.max / hexWidth),
	//	ceil(source.y.max / hexHeight)
	//	);

	//	
	//	for (int x = range.x.min; x <= range.x.max; x++)
	//		for (int y = range.y.min; y <= range.y.max; y++)
	//		{
	//			float3 center = float3(hexWidth * x, (hexHeight * y + hexHeight * 0.5f * (x%2)),aspect);
	//			hexCenters << center;
	//		}


	//	if (instruction.shader.IsEmpty())
	//	{
	//		instruction = shader;
	//	}
	//	instruction.vbo.CreateFromStructs(hexCenters);
	//
	}

	//DX11::BaseInstructionBlock	Area::shader;


	//void		Area::Render(bool lit)
	//{
	//	display.RenderVertices(instruction);
	//
	//}


	//void		Area::StaticCreate()
	//{
	//	shader.shader.LoadFromFile("shaders/hexBackground.hlsl","vsMain","gsMain", "psMain");
	//	shader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
	//	shader.UpdateLayout();
	//	shader.SetPoints();
	//}

	//void		Area::StaticDestroy()
	//{
	//	shader.Clear();
	//
	//}



	bool			Input::UpdateIfNecessary()
	{
		if (this->sizeVersion == windowResolutionVersion)
			return false;
		this->sizeVersion = windowResolutionVersion;

		textout.pushState();
			textout.scale(textZoom,textZoom);

			this->rect.y.max = this->rect.y.min + textout.getScaledHeight() * 2.5f;

			

		textout.popState();

		return true;
	}

	/*static*/ std::function<void()>	Input::onChange;
	/*static*/ std::function<void()>	Input::onDone;



	void			Input::SetCurrent(const String&current)
	{
		backup = current;
		if (Engine::keyboard.read)
			Engine::keyboard.fillInput(current);
	}

	static Engine::InputProfile	profile;

	void			Input::SetAllowed(const String&allowed)
	{
		this->allowed = allowed;
		if (isActive)
			Engine::keyboard.filter(allowed.c_str());
	}

	void			Input::ActivateInput()
	{
		Engine::input.pushProfile();
		profile = *Engine::input.currentProfile();
		Engine::input.bindProfile(profile);
		//Engine::input.resetKeys();
		Engine::input.bind(Key::Backspace,[](){Engine::keyboard.dropLastCharacter(); if (onChange) onChange();});
		Engine::input.bind(Key::Return,onDone);
		Engine::input.bind(Key::Enter,onDone);

		Engine::keyboard.fillInput(backup);
		Engine::keyboard.filter(allowed.c_str());
		if (onChange)
			Engine::keyboard.onCharHandled = OnCharHandled;
		Engine::keyboard.read = true;
		isActive = true;

	}
	void			Input::DeactivateInput()
	{
		backup = GetInput();
		Engine::input.popProfile();
		Engine::keyboard.read = false;
		Engine::keyboard.onCharHandled =  NULL;
		isActive = false;
	}

	String			Input::GetInput()
	{
		return String(Engine::keyboard.getInput(),Engine::keyboard.getInputLen());
	}


	void			Input::Render(bool enabled, bool sensitiveToMouse /*= true*/, bool renderRect /*=true*/)
	{
		core = rect;
		textout.pushState();
		textout.scale(textZoom,textZoom);
		core.Expand(-textout.getScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (renderRect)
			RenderArea(core,false,true);

		const char*input = Engine::keyboard.getInput();
		size_t len = Engine::keyboard.getInputLen();

		float visualLen = textout.getScaledWidth(input,len);


		textout.locate(core.x.min,core.y.max - textout.getScaledHeight() * 0.4f);


		if (visualLen < rect.x.extend())
		{
		//	textout.write(input,len);
			textout << StringRef(input,len);
		}
		else
		{
			while (len > 1)
			{
				input++;
				len--;
				visualLen = textout.getScaledWidth(input,len);
				if (visualLen < rect.x.extend())
				{
					textout << StringRef(input,len);
					//textout.write(input,len);
					break;
				}
			}
		}
		if (((UINT64)(Engine::timing.now*2.0))%2)
			textout << '<';


		textout.popState();


	}



	void		Create()
	{
	//	Area::StaticCreate();
	
	}

	void		Destroy()
	{
		//Area::StaticDestroy();
	
	}



	void			TextField::Render(bool enabled, bool sensitiveToMouse /*= true*/, bool renderRect /*=true*/)
	{
		UpdateIfNecessary();
	
		textout.pushState();
		textout.scale(textZoom*zoom,textZoom*zoom);

		bool hovering = !sensitiveToMouse || (enabled && ContainsMouse());
	
		

		if (renderRect)
		{
			Super::Render(hovering);
		}
		//textout.locate(rect.x.min + rect.x.GetExtend()*0.05f,rect.y.max - textout.getScaledHeight()*1.5f);
		//float lineHeight = textout.getScaledHeight();
		float top = rect.y.max;
		if (enabled)
			textout.color(hovering ? float4(1) : float4(0.75,0.75,0.75,1));
		else
			textout.color(float4(0.5,0.5,0.5,1));
		for (index_t i = 0; i < wrappedText.Count(); i++)
		{
			textout.locate(lineAt[i].x,lineAt[i].y+top);
			textout << wrappedText[i];
		}
		textout.popState();
	}

	void			ImageField::Render(bool enabled, bool sensitiveToMouse /*= true*/)
	{
		if (texture.IsEmpty())
			return;

		bool hovering = !sensitiveToMouse || (enabled && ContainsMouse());
	
		static DX11::BaseInstructionBlock	texturedQuad;
		if (texturedQuad.quickSlots.IsEmpty())
		{
			texturedQuad.quickSlots.SetSize(1);
			Engine::VertexBinding vtx;
			vtx.vertex.set(0,2);
			vtx.normal.unset();
			vtx.floats_per_vertex = 2;
			texturedQuad.binding = vtx;

			DX11::Shader::Binding bindings[] = {
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::VisualEnum::TextureFilter::Trilinear,1,"Sampler"),
			};


			texturedQuad.shader.LoadFromFile("shaders/guiImageField.hlsl","vsMain","psMain",bindings,ARRAYSIZE(bindings) );
			texturedQuad.UpdateLayout();
			texturedQuad.SetTriangles();


			float2 vertices[6] = 
			{
				float2(0,0),
				float2(1,0),
				float2(1,1),
				float2(0,0),
				float2(1,1),
				float2(0,1)
			};

			texturedQuad.vbo.CreateFromStructs(vertices,6);
		}

		TMatrix4<> matrix = Matrix<>::eye4;
		
		Resolution3D res = texture.GetResolution();
		float2 resf = float2(res.width,res.height);

		float2 size = float2(rect.x.extend(),rect.y.extend());
		float2 pixelDim = size & float2(display.clientWidth(),display.clientHeight());
		float aspect = pixelDim.x / pixelDim.y;
		float wantAspect = resf.x / resf.y;
		if (aspect < wantAspect)
		{
			//image too broad
			size.y *= (aspect/wantAspect);
		}
		else
		{
			//image too heigh
			size.x *= (wantAspect / aspect);
		}

		matrix.x.x = size.x;
		matrix.y.y = size.y;

		matrix.w.xy = float2(rect.x.center() - size.x*0.5f,rect.y.center() - size.y*0.5f);

		display.SetObjectMatrix(matrix);
		texturedQuad.quickSlots.first() = texture;

		display.RenderVertices(texturedQuad);
	}

	bool			Field::ContainsMouse() const
	{
		return rect.Contains(Engine::mouse.location.windowRelative);
	}

	void			Field::MoveTopTo(float h)
	{
		float delta = h - rect.y.max;
		rect.y.Translate(delta);
	}


	static bool CheckHovering(const Rect<>&area)
	{
		return area.Contains(Engine::mouse.location.windowRelative);
	}


	void	Darken(const Rect<>&rect, float alpha/*=0.9f*/, bool flush/*=false*/)
	{
		SurfaceRenderer::SetColor(float4(0,0,0,alpha));
		SurfaceRenderer::Rasterize(rect);
		if (flush)
			SurfaceRenderer::Flush();
	
	}
}

#endif
