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



	/*virtual override*/ void	Panel::Render()
	{
		Super::Render(); 
		foreach (components,c) 
			(*c)->Render();
	}


	/*virtual override*/ bool	Panel::UpdateHeight()
	{
		bool rs = Super::UpdateHeight();
		foreach (components,c)
			rs |= (*c)->UpdateHeight();

		if (autoHeight && components.IsNotEmpty())
		{
			float lowest = std::numeric_limits<float>::max();
			foreach (components,c)
			{
				if ((*c)->anchorConfig.Bottom().isActive)
					continue;
				lowest = std::min(lowest, (*c)->GetLocation().y.min);
			}
			rs |= Set(rect.y.min,lowest - autoHeightSpacing);
		}

		return rs;
	}


	/*static*/ void				Component::RenderArea(const Rect<>&rect, bool lit, bool flip)
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

	bool			Area::UpdateHeightIfNecessary()
	{
		if (this->sizeVersion == windowResolutionVersion)
			return false;
		return UpdateHeight();
	}


	bool		TextField::UpdateHeight()
	{
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
		float newMin = this->rect.y.max - textout.getScaledHeight() * (float(wrappedText.Count()) + extraSpacing*2.f);
		bool same = Math::similar(newMin,this->rect.y.min);
		this->rect.y.min = newMin;
		textout.popState();
		return Super::UpdateHeight() || !same;
	}




	void				Area::BaseRender(bool lit)
	{
		RenderArea(rect,lit,false);
	}

	static const float CellSize = 0.01f;

	bool			Area::UpdateHeight()
	{
		this->sizeVersion = windowResolutionVersion;
		
		return Super::UpdateHeight();
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



	bool			Input::UpdateHeight()
	{
		textout.pushState();
			textout.scale(textZoom,textZoom);

			float newMin = this->rect.y.max - textout.getScaledHeight() * 2.5f;
			bool same = Math::similar(newMin,this->rect.y.min);
			this->rect.y.min = newMin;

		textout.popState();
		return Super::UpdateHeight() || !same;
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


	/*virtual override*/ void			Input::Render()
	{
		core = rect;
		textout.pushState();
		textout.scale(textZoom,textZoom);
		core.Expand(-textout.getScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (fillBackground)
			RenderArea(core,false,true);

		const char*input = Engine::keyboard.getInput();
		size_t len = Engine::keyboard.getInputLen();

		float visualLen = textout.getScaledWidth(input,len);


		SurfaceRenderer::Flush();


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



	/*virtual override*/ void			TextField::Render()
	{
		UpdateHeightIfNecessary();
	
		textout.pushState();
		textout.scale(textZoom*zoom,textZoom*zoom);

		bool hovering = !Super::sensitiveToMouse || (enabled && ContainsMouse());
	
		

		if (fillBackground)
		{
			Super::BaseRender(hovering);
		}

		SurfaceRenderer::Flush();

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

	/*virtual override*/ bool	Image::UpdateHeight()
	{
		if (texture.IsEmpty() || !autoSetHeight)
			return Super::UpdateHeight();
		Resolution3D res = texture.GetResolution();
		float2 resf = float2(res.width,res.height);

		float2 size = float2(rect.x.extend(),rect.y.extend());
		float2 pixelDim = size & float2(display.clientWidth(),display.clientHeight());
		float aspect = pixelDim.x / pixelDim.y;
		float wantAspect = resf.x / resf.y;
		size.y *= (aspect/wantAspect);

		bool change = Set(rect.y.min,rect.y.max - size.y);

		return Super::UpdateHeight() || change;
	}


	/*virtual override*/ void			Image::Render()
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

	bool			Component::ContainsMouse() const
	{
		return rect.Contains(Engine::mouse.location.windowRelative);
	}

	void			Component::MoveTopTo(float h)
	{
		float delta = h - rect.y.max;
		rect.y.Translate(delta);
	}

	bool			Component::FullAutoLayout(const Rect<>&context)
	{
		count_t numRounds = 0;
		bool rs = false;
		for (;;)
		{
			if (!AutoLayout(context))
			{
				//nothing changed
				rs |= UpdateHeightIfNecessary();
				return rs;
			}
			if (!UpdateHeight())
			{
				//layout changed, but heights stayed the same
				return rs;
			}

			rs = true;
			numRounds++;

			if (numRounds > 1000)	//might be linear to the number of contained components, depending on order
				FATAL__("Layout infinite loop detected");
		}

	}


	bool			Component::LayoutAxis(int axis, const Rect<>&context)
	{
		const Axial<TAnchor>&anchor = anchorConfig.axis[axis];
		TFloatRange<>&a = rect.axis[axis];

		bool rs = false;
		if (anchor.center.isActive)
		{
			ASSERT__(!anchor.min.isActive);
			ASSERT__(!anchor.max.isActive);
			const TAnchor&anc = anchor.center;
			const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].extend() : anc.distanceFromReference;
			if (anc.references.IsNotEmpty())
			{
				float avg = 0.f;
				count_t cnt = 0;
				foreach (anc.references,ref0)
				{
					PComponent ref = ref0->lock();
					if (ref)
					{
						avg += ref->GetLocation().axis[axis].center();
						cnt++;
					}
				}
				if (cnt > 0)
				{
					avg /= cnt;
					rs |= SetCenter(a, avg + dist);
				}
			}
			else
				rs |= SetCenter(a, context.axis[axis].center() + dist);

			return rs;
		}

		if (!anchor.min.isActive && !anchor.max.isActive)
			return false;



		float extend = a.GetExtend();

		if (anchor.min.isActive)
		{
			const TAnchor&anc = anchor.min;

			const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].extend() : anc.distanceFromReference;

			if (anc.references.IsNotEmpty())
			{
				float highest = -std::numeric_limits<float>::max();
				foreach (anc.references,ref0)
				{
					PComponent ref = ref0->lock();
					highest = std::max(highest,ref->GetLocation().axis[axis].max);
				}
				if (highest > -std::numeric_limits<float>::max())
					rs |= Set(a.min, highest + dist);
			}
			else
				rs |= Set(a.min, context.axis[axis].min + dist);
		}
		if (anchor.max.isActive)
		{
			const TAnchor&anc = anchor.max;

			const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].extend() : anc.distanceFromReference;

			if (anc.references.IsNotEmpty())
			{
				float lowest = std::numeric_limits<float>::max();
				foreach (anc.references,ref0)
				{
					PComponent ref = ref0->lock();
					lowest = std::min(lowest,ref->GetLocation().axis[axis].min);
				}
				if (lowest < std::numeric_limits<float>::max())
					rs |= Set(a.max, lowest - dist);
			}
			else
				rs |= Set(a.max, context.axis[axis].max - dist);
		}
		else
			rs |= Set(a.max,a.min + extend);

		if (!anchor.min.isActive)
			rs |= Set(a.min,a.max - extend);


		return rs;
	}

	void			Component::SetCenteredAbsoluteWidth(float w)
	{
		anchorConfig.Left().Set(w*0.25f,false);
		anchorConfig.Right().Set(w*0.25f,false);

	}
	void			Component::SetCenteredRelativeWidth(float w)
	{
		anchorConfig.Left().Set(w*0.25f);
		anchorConfig.Right().Set(w*0.25f);
	}




	/*static*/ bool		Component::Set(float&ref, float to)
	{
		bool changed = !Math::similar(ref,to);
		ref = to;
		return changed;
	}

	/*static*/ bool		Component::SetCenter(TFloatRange<>&ref, float to)
	{
		float current = ref.center();

		bool changed = !Math::similar(current,to);
		ref.SetCenter(to);
		return changed;
	}


	/*virtual*/ bool	Component::AutoLayout(const Rect<>&context)
	{
		bool rs = false;
		rs |= LayoutAxis(0,context);
		rs |= LayoutAxis(1,context);
		return rs;
	}


	static bool CheckHovering(const Rect<>&area)
	{
		return area.Contains(Engine::mouse.location.windowRelative);
	}


	/*static*/ void	Component::Darken(const Rect<>&rect, float alpha/*=0.9f*/, bool flush/*=false*/)
	{
		SurfaceRenderer::SetColor(float4(0,0,0,alpha));
		SurfaceRenderer::Rasterize(rect);
		if (flush)
			SurfaceRenderer::Flush();
	
	}
}

#endif
