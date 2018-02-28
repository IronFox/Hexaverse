#include "componentDetails.h"

#include "../display.h"
#include <logEvent.h>

namespace HUD{
namespace Component{
namespace Detail
	{


		Engine::OrthographicAspect<>	aspect;
		Resolution						lastClientSpace;


		Buffer0<Viewport>	viewportStack;
		float2	totalOffset;

		void	LoadView(const M::Rect<>&view, const float2&offset)
		{
			if (!view.IsValid())
				FATAL__("HUD: Trying to load invalid view: "+ToString(view));
			LineRenderer::Flush();
			SurfaceRenderer::Flush();
			M::Rect<> view2 = view;
			view2.Translate(offset);
			aspect.UpdateProjection(view2,-1,1);
			aspect.region = view;
			display.Pick(aspect);
		}

		void	PushLoadView(const M::Rect<>&view, const float2&offset/*=float2()*/)
		{

			viewportStack.Append().view = view;
			viewportStack.Last().offset = offset+totalOffset;
			viewportStack.Last().relativeOffset = offset;
			if (viewportStack.Count() > 1)
			{
				viewportStack.Last().view.Translate(-totalOffset);
				viewportStack.Last().view.ConstrainBy(viewportStack.GetFromEnd(1).view);
				//viewportStack.Last().second += viewportStack.GetFromEnd(1).second;
			}
			totalOffset += offset;
			LoadView(viewportStack.Last().view,viewportStack.Last().offset);
		}

		void	PopLoadView()
		{
			totalOffset -= viewportStack.Last().relativeOffset;
			viewportStack.EraseLast();
			if (viewportStack.IsNotEmpty())
				LoadView(viewportStack.Last().view,viewportStack.Last().offset);
			else
			{
				LoadView(M::Rect<>(0,0,1,1),float2());
				totalOffset = float2();
			}
		}

		void OnCharInput(char)
		{
			OnInput();
		}

		Buffer<Component*>	allComponents;
		Engine::InputProfile	profile;
		bool	inputIsActive = false;

		bool	InputIsActive()
		{
			return inputIsActive;
		}

		void	Fail(const String&message)
		{
			LogEvent(EventClass::Preset::error,message);
			DBG_FATAL__(message);
		}


		void ActivateInput(const char*allowed, const std::function<void(Key::Name)>&onSpecialKey, Engine::Keyboard::CharacterBuffer&sourceBufferContent)
		{
			if (InputIsActive())
			{
				Fail("Double input activation registered");
				return;
			}


			Engine::input.PushProfile();
			//profile = *Engine::input.currentProfile();//this copies over a shitload of commands we don't want
			//let's copy some over, however...
			const auto current = Engine::input.GetCurrentProfile();
			profile.key[Key::Escape] = current->key[Key::Escape];
			//profile.key[Key::Enter] = current->key[Key::Enter];
			//profile.key[Key::Return] = current->key[Key::Return];
			profile.key[Key::MouseButton0] = current->key[Key::MouseButton0];
			profile.key[Key::MouseButton1] = current->key[Key::MouseButton1];
			profile.key[Key::MouseButton2] = current->key[Key::MouseButton2];
			profile.key[Key::MouseButton3] = current->key[Key::MouseButton3];
			profile.key[Key::MouseButton4] = current->key[Key::MouseButton4];
			Engine::input.BindProfile(profile);
		
			//Engine::input.ClearAllBindings();
			Engine::input.Bind(Key::Tab, onSpecialKey);
			Engine::input.Bind(Key::CapsLock, onSpecialKey);
			Engine::input.Bind(Key::Space, onSpecialKey);
			Engine::input.Bind(Key::Control, onSpecialKey);
			Engine::input.Bind(Key::Alt, onSpecialKey);
			Engine::input.Bind(Key::F1, onSpecialKey);
			Engine::input.Bind(Key::F2, onSpecialKey);
			Engine::input.Bind(Key::F3, onSpecialKey);
			Engine::input.Bind(Key::F4, onSpecialKey);
			Engine::input.Bind(Key::F5, onSpecialKey);
			Engine::input.Bind(Key::F6, onSpecialKey);
			Engine::input.Bind(Key::F7, onSpecialKey);
			Engine::input.Bind(Key::F8, onSpecialKey);
			Engine::input.Bind(Key::F9, onSpecialKey);
			Engine::input.Bind(Key::F10, onSpecialKey);
			Engine::input.Bind(Key::F11, onSpecialKey);
			Engine::input.Bind(Key::F12, onSpecialKey);
			Engine::input.Bind(Key::ArrowLeft, onSpecialKey);
			Engine::input.Bind(Key::ArrowDown, onSpecialKey);
			Engine::input.Bind(Key::ArrowRight, onSpecialKey);
			Engine::input.Bind(Key::ArrowUp, onSpecialKey);
			Engine::input.Bind(Key::Delete, onSpecialKey);
			Engine::input.Bind(Key::Print, onSpecialKey);
			Engine::input.Bind(Key::Home, onSpecialKey);
			Engine::input.Bind(Key::End, onSpecialKey);
			Engine::input.Bind(Key::PageUp, onSpecialKey);
			Engine::input.Bind(Key::PageDown, onSpecialKey);
			



			Engine::input.Bind(Key::Backspace,[onSpecialKey](){Engine::keyboard.DropLastCharacter(); Detail::OnInput(); if(onSpecialKey) onSpecialKey(Key::Backspace);});
			Engine::input.Bind(Key::Return,onSpecialKey);
			Engine::input.Bind(Key::Enter,onSpecialKey);

			Engine::input.BindCtrl(Key::V,[](){
				char buffer[0x100];
				memset(buffer,0,sizeof(buffer));
				if (System::getFromClipboardIfText(Engine::context.getWindow(),buffer,sizeof(buffer)-1))
				{
					Engine::keyboard.FilterAppend(buffer);	
				}
			});

			if (allowed)
				Engine::keyboard.Filter(allowed);
			else
				Engine::keyboard.ResetTranslation();
			Engine::keyboard.onCharHandled = OnCharInput;
			Engine::keyboard.read = true;
			Engine::keyboard.ImportBuffer(sourceBufferContent);
			inputIsActive = true;

		}

		void DeactivateInput(Engine::Keyboard::CharacterBuffer*destBufferContent)
		{
			if (!InputIsActive())
			{
				Fail("Double input deactivation registered");
				return;
			}

			if (destBufferContent)
				Engine::keyboard.ExportBuffer(*destBufferContent);
			else
				Engine::keyboard.FlushInput();
			Engine::input.PopProfile();
			Engine::keyboard.read = false;
			Engine::keyboard.onCharHandled =  NULL;
			inputIsActive = false;
		}


	}

}}
