#ifdef HAS_WINDOW

#include <global_root.h>
#include "guiElements.h"
#include <engine/display.h>
#include "../ExtRenderer/DirectX11.h"
#include "../DX11/texture_font.h"
#include <container/buffer.h>
#include "surfaceRenderer.h"
#include "lineRenderer.h"
#include <engine/aspect.h>
#include "../../shared/objects.h"

namespace DX11 = Engine::DX11; 

extern Engine::Textout<DX11::TextureFont>	textout;
extern Engine::Display<Engine::DirectX11>	display;


namespace GUI
{
	index_t windowResolutionVersion = 0;

	namespace Detail
	{
		Buffer0<GUI::PWindow,Swap>		visibleWindows;

		void OnInput()
		{
			if (visibleWindows.IsNotEmpty())
				visibleWindows.last()->DetectEnabledState();
		}


	}



	PComponent	mousePressedComponent;

	/*static*/ DX11::BaseInstructionBlock	Image::texturedQuad;

	static UINT ViewPortWidth()
	{
		return std::max(100U,display.clientWidth());
	}
	static UINT ViewPortHeight()
	{
		return std::max(100U,display.clientHeight());
	}

	static float2 ViewPortSizef()
	{
		return float2(ViewPortWidth(),ViewPortHeight());
	}


	PWindow				GetTopMostWindow()
	{
		if (Detail::visibleWindows.IsNotEmpty())
			return Detail::visibleWindows.last();
		return PWindow();
	}


	index_t	FindWindowIndex(const Window*window)
	{
		foreach (Detail::visibleWindows,_w)
			if (_w->get() == window)
			{
				return _w - Detail::visibleWindows.pointer();
			}
		return InvalidIndex;
	}


	bool	Hide(const PWindow&window)
	{
		return Hide(window.get());
	}

	bool	Hide(Window*window)
	{
		VerifyIntegrity(CLOCATION);
		index_t index = FindWindowIndex(window);
		if (index == InvalidIndex)
			return false;

		window->OnHide();
		if (index+1 == Detail::visibleWindows.Count())
		{
			window->Deactivate();
			if (Detail::visibleWindows.Count() > 1)
				Detail::visibleWindows.fromEnd(1)->Activate();
		}
		Detail::visibleWindows.Erase(index);	//should be last. window is not guaranteed to be valid past this point

		VerifyIntegrity(CLOCATION);

		SignalSituationChange();

		return true;

	}

	
	bool	IsVisible(const PWindow&window)
	{
		return Detail::visibleWindows.Contains(window);
	}

	void	Show(const PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.last() == window)
		{
			DBG_ASSERT__(window->IsActive());
			if (!window->IsActive())
				window->Activate();
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.first() == window)
		//	return;
		Detail::visibleWindows.FindAndErase(window);
		if (Detail::visibleWindows.IsNotEmpty())
			Detail::visibleWindows.last()->Deactivate();
		Detail::visibleWindows << window;
		window->Activate();
		SignalSituationChange();
	}



	void	ShowBottom(const PWindow&window)
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.first() == window)
		{
			if (Detail::visibleWindows.Count() == 1)
			{
				DBG_ASSERT__(window->IsActive());
				if (!window->IsActive())
					window->Activate();
			}
			return;
		}
		//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.first() == window)
		//	return;

		index_t at = Detail::visibleWindows.GetIndexOf(window);
		if (at != InvalidIndex)
		{
			if (at+1 == Detail::visibleWindows.Count())	//ist last
			{
				window->Deactivate();
				Detail::visibleWindows.fromEnd(1)->Activate();
			}
			Detail::visibleWindows.Erase(at);
		} 
		Detail::visibleWindows.Insert(0,window);
		if (Detail::visibleWindows.Count() == 1)
			window->Activate();

		SignalSituationChange();

	}




	bool	CloseAllModal()
	{
		VerifyIntegrity(CLOCATION);
		if (Detail::visibleWindows.IsNotEmpty())
		{
			bool anyNonClosable = false;
			foreach (Detail::visibleWindows,w)
				if (!(*w)->canBeImplicitlyClosed)
				{
					anyNonClosable = true;
					break;
				}

			if (anyNonClosable)
			{
				bool rs = false;
				for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
				{
					GUI::PWindow w = Detail::visibleWindows[i];
					if (w->canBeImplicitlyClosed)
					{
						Hide(w);
						i--;
						rs = true;
					}
				}
				GUI::VerifyIntegrity(CLOCATION);
				return rs;
			}
			else
			{
				foreach (Detail::visibleWindows,_d)
				{
					PWindow d = *_d;
					d->OnHide();
				}
				Detail::visibleWindows.last()->Deactivate();
				Detail::visibleWindows.Clear();
			}
			GUI::VerifyIntegrity(CLOCATION);
			return true;
		}
		VerifyIntegrity(CLOCATION);
		return false;
	}


	bool	IsModal()
	{
		foreach (Detail::visibleWindows,w)
			if ((*w)->isModal)
				return true;
		return false;
	}

	bool	HandleMouseDown()
	{
		for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
		{
			PWindow w = Detail::visibleWindows[i];
			mousePressedComponent = w->HandleMouseDown();
			if (mousePressedComponent || w->isModal)
				return true;
		}
		mousePressedComponent.reset();
		return false;
	}

	void	HandleMouseUp()
	{
		if (mousePressedComponent)
		{
			mousePressedComponent->SignalMouseUp();
			mousePressedComponent.reset();
		}

	}


	void	SignalSituationChange()
	{
		VerifyIntegrity(CLOCATION);
		for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
		{
			const PWindow&d = Detail::visibleWindows[i];
			if (!d->SignalChange())
			{
				d->OnHide();
				if (i+1 == Detail::visibleWindows.Count())
				{
					d->Deactivate();
					if (Detail::visibleWindows.Count() > 1)
						Detail::visibleWindows.fromEnd(1)->Activate();
				}
				Detail::visibleWindows.Erase(i);
				i--;
			}
			else
				d->DetectEnabledState();
		}
		VerifyIntegrity(CLOCATION);
	}


	void	VerifyIntegrity(const TCodeLocation&loc)
	{
		if (Detail::visibleWindows.IsEmpty())
			return;


		for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
		{
			bool shouldBeActive = (i+1 == Detail::visibleWindows.Count());
			GUI::PWindow w = Detail::visibleWindows[i];
			if (w->IsActive() != shouldBeActive)
			{
				String msg = String(loc.ToString())+ ": Activity mismatch for window "+String(i+1)+"/"+String(Detail::visibleWindows.Count())+" (active="+String(w->IsActive())+", should be "+String(shouldBeActive)+")";
				LogEvent(EventClass::Preset::error,msg);
				DBG_FATAL__(msg);
				if (shouldBeActive)
					w->Activate();
				else
					w->Deactivate();
			}
		}
	}


	void			TAnchor::SetSomeOf(const PPanel&p, float distance, const std::function<bool(const PComponent&)>&callback, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p || !p->HasComponents())
			return;
		type = Fixed;
		distanceFromReference = distance;
		foreach (p->GetComponents(),c)
			if (callback(*c))
				references.Append(*c);
		distanceType = dtype;

	}


	void			TAnchor::SetAllOf(const PPanel&p, float distance, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p || !p->HasComponents())
			return;
		type = Fixed;
		distanceFromReference = distance;
		foreach (p->GetComponents(),c)
			references.Append(*c);
		distanceType = dtype;
	}

	void			TAnchor::SetLastOf(const PPanel&p, float distance, distanceType_t dtype/*=RelativeToParentComponent*/)
	{
		if (!p || !p->HasComponents())
			return;
		type = Fixed;
		distanceFromReference = distance;
		references.Append(p->GetComponents().last());
		distanceType = dtype;
	}


	float			TAnchor::ToScreenDistance(float parentSize, bool isHorizontal) const
	{
		switch (distanceType)
		{
			case RelativeToParentComponent:
				return distanceFromReference * parentSize;

			case RelativeToScreen:
				return distanceFromReference;
			case AbsolutePixels:
				return distanceFromReference / float(isHorizontal ? ViewPortWidth() : ViewPortHeight());
		}
		FATAL__("Unexpected distance type");
		return 0.f;

	}


	/*virtual override*/ void	Panel::Render()
	{
		Super::Render(); 
		RenderChildren();
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
				const auto&bottom = (*c)->anchorConfig.Bottom();
				const auto&top = (*c)->anchorConfig.Top();
				switch (bottom.type)
				{
					case TAnchor::Disabled:
						lowest = std::min(lowest, (*c)->GetLocation().y.min);
					break;
					case TAnchor::Fixed:
						if (bottom.references.IsEmpty() && top.type == TAnchor::Disabled)
						{
							float dist = bottom.distanceFromReference;
							if (bottom.distanceType != TAnchor::RelativeToParentComponent)
								lowest = std::min(lowest,rect.y.max - (*c)->GetLocation().y.extend() - bottom.ToScreenDistance(0.f,false));
						}
					break;
					case TAnchor::Auto:
						if (top.type == TAnchor::Auto)
						{
							lowest = std::min(lowest,rect.y.max - (*c)->GetLocation().y.extend());
						}
					break;
				}
			}
			rs |= Set(rect.y.min,lowest - autoHeightSpacing);
		}

		return rs;
	}


	/*static*/ void				Component::RenderArea(const Rect<>&rect, bool lit, bool flip, const float3&backgroundTone)
	{
		float low = 0.55f;  //0.3;
		float high = lit ? 0.89f : 0.77f;   //0.8f : 0.6;
		if (flip)
			std::swap(low,high);

		//low = sqrt(low);
		float4 lowColor(low,low,low,low);
		Vec::stretch(lowColor.rgb,backgroundTone,lowColor.rgb);
		//high = sqrt(high);
		float4 highColor(high,high,high,high);
		Vec::stretch(highColor.rgb,backgroundTone,highColor.rgb);

		SurfaceRenderer::Rasterize(float2(rect.x.min,rect.y.min), lowColor, float2(rect.x.max,rect.y.min),lowColor, float2(rect.x.max,rect.y.max), highColor, float2(rect.x.min,rect.y.max),highColor);
		SurfaceRenderer::Flush();
		

		float4 lineLowColor(backgroundTone,low*low);
		float4 lineHighColor(backgroundTone,high*high);
	

		LineRenderer::Rasterize(float3(rect.x.min,rect.y.min,0.f), lineLowColor, float3(rect.x.max,rect.y.min,0.f),lineLowColor);
		LineRenderer::Rasterize(float3(rect.x.max,rect.y.max,0.f), lineHighColor, float3(rect.x.min,rect.y.max,0.f),lineHighColor);
		LineRenderer::Rasterize(float3(rect.x.max,rect.y.min,0.f),lineLowColor, float3(rect.x.max,rect.y.max,0.f), lineHighColor);
		LineRenderer::Rasterize(float3(rect.x.min,rect.y.max,0.f),lineHighColor, float3(rect.x.min,rect.y.min,0.f), lineLowColor);
		LineRenderer::Flush();
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


	static const float textZoom = 1.5f;

	bool			Area::UpdateHeightIfNecessary()
	{
		if (this->sizeVersion == windowResolutionVersion)
			return false;
		return UpdateHeight();
	}



	/*virtual override*/ PComponent	Panel::HandleMouseDown()
	{
		for (index_t i = components.Count()-1; i < components.Count(); --i)
		{
			PComponent c = components[i]->HandleMouseDown();
			if (c)
				return c;
		}
		return Super::HandleMouseDown();
	}


	void			Panel::AnchorBelowPrevious(const PComponent&newComponent, float distanceFromSides /*= 0.015f*/,  float distanceToPrevious/* = 0.015f*/, TAnchor::distanceType_t distanceType/*=TAnchor::RelativeToScreen*/)
	{
		if (components.IsNotEmpty())
			newComponent->anchorConfig.Top().Set(components.last(), distanceToPrevious,distanceType);
		else
			newComponent->anchorConfig.Top().Set(distanceToPrevious,distanceType);
		
		newComponent->anchorConfig.Left().Set(distanceFromSides,distanceType);
		newComponent->anchorConfig.Right().Set(distanceFromSides,distanceType);
		components.Append(newComponent);
	}


	void				Area::BaseRender(bool lit, bool flipped)
	{
		RenderArea(rect,lit,flipped,backgroundTone);
	}

	static const float CellSize = 0.01f;

	bool			Area::UpdateHeight()
	{
		this->sizeVersion = windowResolutionVersion;
		
		return Super::UpdateHeight();
	}

	/*virtual override*/ bool			Panel::DetectEnabledState()
	{
		bool change = Super::DetectEnabledState();
		foreach (components,c)
			change |= (*c)->DetectEnabledState();
		return change;
	};

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



	bool			BaseInput::UpdateHeight()
	{
		textout.pushState();
			textout.scale(textZoom,textZoom);

			float newMin = this->rect.y.max - textout.getScaledHeight() * 2.5f;
			bool same = Math::similar(newMin,this->rect.y.min);
			this->rect.y.min = newMin;

		textout.popState();
		return Super::UpdateHeight() || !same;
	}



	void			Input::SetCurrent(const String&current)
	{
		if (IsActive())
			Engine::keyboard.FillInput(current);
		else
			backup.SetTo(current);
	}

	static Engine::InputProfile	profile;

	void			Input::SetAllowed(const String&allowed)
	{
		this->allowed = allowed;
		if (Super::IsActive())
			Engine::keyboard.Filter(allowed.c_str());
	}

	/*static*/ void BaseInput::OnCharHandled(char)
	{
		Detail::OnInput();
	}

	void			BaseInput::ActivateInput()
	{
		Engine::input.pushProfile();
		//profile = *Engine::input.currentProfile();//this copies over a shitload of commands we don't want
		//let's copy some over, however...
		profile.key[Key::Escape] = Engine::input.currentProfile()->key[Key::Escape];
		profile.key[Key::Enter] = Engine::input.currentProfile()->key[Key::Enter];
		profile.key[Key::Return] = Engine::input.currentProfile()->key[Key::Return];
		profile.key[Key::MouseButton0] = Engine::input.currentProfile()->key[Key::MouseButton0];
		profile.key[Key::MouseButton1] = Engine::input.currentProfile()->key[Key::MouseButton1];
		profile.key[Key::MouseButton2] = Engine::input.currentProfile()->key[Key::MouseButton2];
		profile.key[Key::MouseButton3] = Engine::input.currentProfile()->key[Key::MouseButton3];
		profile.key[Key::MouseButton4] = Engine::input.currentProfile()->key[Key::MouseButton4];
		Engine::input.bindProfile(profile);
		
		//Engine::input.resetKeys();
		Engine::input.bind(Key::Backspace,[](){Engine::keyboard.DropLastCharacter(); Detail::OnInput();});
		Engine::input.bind(Key::Return,onEnter);
		Engine::input.bind(Key::Enter,onEnter);
		Engine::input.bindCtrl(Key::V,[](){
			char buffer[0x100];
			memset(buffer,0,sizeof(buffer));
			if (System::getFromClipboardIfText(Engine::context.getWindow(),buffer,sizeof(buffer)-1))
			{
				Engine::keyboard.FilterAppend(buffer);	
			}
		});

		const char*filter = GetAllowed();
		if (filter)
			Engine::keyboard.Filter(filter);
		else
			Engine::keyboard.ResetTranslation();
		Engine::keyboard.onCharHandled = BaseInput::OnCharHandled;
		Engine::keyboard.read = true;
		Engine::keyboard.ImportBuffer(backup);
	}


	void			BaseInput::DeactivateInput()
	{
		Engine::keyboard.ExportBuffer(backup);
		Engine::input.popProfile();
		Engine::keyboard.read = false;
		Engine::keyboard.onCharHandled =  NULL;
	}

	String			Input::GetInput()
	{
		if (IsActive())
			return String(Engine::keyboard.GetInput(),Engine::keyboard.GetInputLen());
		return String(backup.GetCharacters(),backup.GetLength());
	}

	const char*		PasswordInput::GetInput()
	{
		if (IsActive())
			return Engine::keyboard.GetInput();
		return backup.GetCharacters();
	}

	void			PasswordInput::WipeInput()
	{
		if (IsActive())
			Engine::keyboard.FlushInput();
		backup.Wipe();
	}





	/*virtual*/ StringRef BaseInput::GetText()
	{
		const char*input;
		size_t len;
		bool active = IsActive();
		input = active ? Engine::keyboard.GetInput() : backup.GetCharacters();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();
		return StringRef(input,len);
	}


	/*static*/ StringBuffer	PasswordInput::placeholder;

	/*virtual override*/ StringRef PasswordInput::GetText()
	{
		size_t len;
		bool active = IsActive();
		len = active ? Engine::keyboard.GetInputLen() : backup.GetLength();

		while (placeholder.length() < len)
		{
//			placeholder << "********";
			placeholder << "........";
		}
		return StringRef(placeholder.pointer(),len);
	}


	/*virtual override*/ void			BaseInput::Render()
	{
		core = rect;
		textout.pushState();
		textout.scale(textZoom,textZoom);
		core.Expand(-textout.getScaledHeight()*0.4f);
		//core.Scale(0.97f);

		if (fillBackground)
			RenderArea(core,false,true,backgroundTone);

		StringRef text = GetText();


		static const char cursorChar = '<';
		float cursorLen = textout.getScaledWidth(&cursorChar,1);
		float visualLen = textout.getScaledWidth(text) + cursorLen;


		SurfaceRenderer::Flush();


		textout.locate(core.x.min,core.y.max - textout.getScaledHeight() * 0.4f);


		if (visualLen < core.x.extend())
		{
		//	textout.write(input,len);
			textout << text;
		}
		else
		{
			while (text.DropFirstChar())
			{
				visualLen = textout.getScaledWidth(text) + cursorLen;
				if (visualLen < core.x.extend())
				{
					textout << text;
					//textout.write(input,len);
					break;
				}
			}
		}
		if (((UINT64)(Engine::timing.now*2.0))%2)
			textout << cursorChar;


		textout.popState();


	}



	void		Create()
	{
		Image::StaticCreate();
	}

	/*static*/ void Image::StaticDestroy()
	{
		texturedQuad.Clear();
	}

	/*static*/ void Image::StaticCreate()
	{
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


	//	Area::StaticCreate();
	
	}

	void		Destroy()
	{
		Image::StaticDestroy();
		//Area::StaticDestroy();
		Detail::visibleWindows.Clear();
	}




	void	Render()
	{
		foreach (Detail::visibleWindows,d)
		{
			(*d)->Render();
		}
	}


	bool			TextField::UpdateIdealWidth()
	{
		textout.pushState();
		textout.scale(textZoom*zoom,textZoom*zoom);
		float w = 0.f;

		foreach (rawText,t)
		{
			float w1 = 0.f;
			foreach (*t,c)
				w1 += CharWidth(*c);
			w = std::max(w,w1);
		}

		bool changed = Set(rect.x.max, rect.x.min + w / 0.85f);

		textout.popState();
		return changed;
	}

	void			TextField::SetIdealWidth()
	{

		if (UpdateIdealWidth())
			InvalidateSize();
	}




	bool		TextField::UpdateHeight()
	{
		bool changed = false;
		if (widthLockedToIdeal)
			changed = UpdateIdealWidth();

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
		textHeight = textout.getScaledHeight() * (float(wrappedText.Count()) + extraSpacing*2.f);

		changed |= Set(this->rect.y.min,rect.y.max - textHeight);

		textout.popState();
		return Super::UpdateHeight() || changed;
	}



	/*virtual override*/ void			TextField::Render()
	{
		UpdateHeightIfNecessary();
	
	
		
		bool lit = IsLit();

		if (fillBackground)
		{
			Super::BaseRender(lit,false);
		}
		RenderText(lit);
	}

	void TextField::RenderText(bool lit)
	{
		textout.pushState();
		textout.scale(textZoom*zoom,textZoom*zoom);

		//bool hovering = !Super::sensitiveToMouse || (enabled && ContainsMouse());

		SurfaceRenderer::Flush();

		//textout.locate(rect.x.min + rect.x.GetExtend()*0.05f,rect.y.max - textout.getScaledHeight()*1.5f);
		//float lineHeight = textout.getScaledHeight();
		float top = rect.y.max;


		float4 baseColor;
		if (IsEnabled())
			baseColor = (lit ? float4(1) : float4(0.75,0.75,0.75,1));
		else
			baseColor = float4(0.5,0.5,0.5,1);

		textout.color(baseColor & this->color);
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

		float wantHeight = resf.y / float(ViewPortHeight());




		float2 size = float2(rect.x.extend(),rect.y.extend());
		size.y = wantHeight;
		float2 pixelDim = size & ViewPortSizef();
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

		bool lit = IsLit();
	
		TMatrix4<> matrix = Matrix<>::eye4;
		
		Resolution3D res = texture.GetResolution();
		float2 resf = float2(res.width,res.height);

		float2 size = float2(rect.x.extend(),rect.y.extend());
		float2 pixelDim = size & ViewPortSizef();
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
		bool rs = UpdateHeight();
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

			#ifdef _DEBUG
				if (numRounds > 900)
					ShowMessage("Debug: GUI will crash shortly. Attach now and debug.");

			#endif

			if (numRounds > 1000)	//might be linear to the number of contained components, depending on order
				FATAL__("Layout infinite loop detected");
		}

	}


	bool			Component::LayoutAxis(int axis, const Rect<>&context)
	{
		const Axial<TAnchor>&anchor = anchorConfig.axis[axis];
		TFloatRange<>&a = rect.axis[axis];

		bool rs = false;
		//if (anchor.center.IsActive())
		//{
		//	ASSERT__(!anchor.min.IsActive());
		//	ASSERT__(!anchor.max.IsActive());
		//	const TAnchor&anc = anchor.center;
		//	const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].extend() : anc.distanceFromReference;
		//	if (anc.references.IsNotEmpty())
		//	{
		//		float avg = 0.f;
		//		count_t cnt = 0;
		//		foreach (anc.references,ref0)
		//		{
		//			PComponent ref = ref0->lock();
		//			if (ref)
		//			{
		//				avg += ref->GetLocation().axis[axis].center();
		//				cnt++;
		//			}
		//		}
		//		if (cnt > 0)
		//		{
		//			avg /= cnt;
		//			rs |= SetCenter(a, avg + dist);
		//		}
		//	}
		//	else
		//		rs |= SetCenter(a, context.axis[axis].center() + dist);

		//	return rs;
		//}

		if (anchor.min.IsInactive() && anchor.max.IsInactive())
			return false;
		if (anchor.min.IsAuto() && anchor.max.IsAuto())
		{
			{
				const TAnchor&anc = anchor.min;

				if (anc.references.IsNotEmpty())
				{
					FATAL__("Auto not applicable on set references");
					return false;
					//float highest = -std::numeric_limits<float>::max();
					//foreach (anc.references,ref0)
					//{
					//	PComponent ref = ref0->lock();
					//	highest = std::max(highest,ref->GetLocation().axis[axis].max);
					//}
					//if (highest == -std::numeric_limits<float>::max())
					//	return false;
					//bottom = highest;
				}
			}
			{
				const TAnchor&anc = anchor.max;

				if (anc.references.IsNotEmpty())
				{
					FATAL__("Auto not applicable on set references");
					return false;
				}
			}
			const TFloatRange<>&ctx = context.axis[axis];
			float ext = a.GetExtend();
			float center = ctx.GetCenter();
			float top = std::min(ctx.max,center + ext*0.5f);
			bool changed = Set(a.max,top);
			a.min = a.max - ext;
			return changed;
		}

		if (anchor.min.IsAuto() || anchor.max.IsAuto())
		{
			FATAL__("Both top and bottom anchors must be set to auto");
			return false;
		}


		float extend = a.GetExtend();

		if (anchor.min.IsFixed())
		{
			const TAnchor&anc = anchor.min;

			const float dist = anc.ToScreenDistance(context.axis[axis].extend(),axis==0);

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
		if (anchor.max.IsFixed())
		{
			const TAnchor&anc = anchor.max;

			const float dist = anc.ToScreenDistance(context.axis[axis].extend(),axis==0);
//			const float dist = anc.distanceIsRelative ? anc.distanceFromReference * context.axis[axis].extend() : anc.distanceFromReference;

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

		if (!anchor.min.IsFixed())
			rs |= Set(a.min,a.max - extend);


		return rs;
	}

	void			Component::SetCenteredWidth(float w, TAnchor::distanceType_t type)
	{
		float extra = 1.f - w;
		if (type == TAnchor::AbsolutePixels)
			extra = float(ViewPortWidth()) - w;
		anchorConfig.Left().Set(extra*0.5f,type);
		anchorConfig.Right().Set(extra*0.5f,type);
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
