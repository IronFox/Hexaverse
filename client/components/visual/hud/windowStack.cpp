#include "windowStack.h"

#include "componentDetails.h"
#include "../display.h"
#include <container/buffer.h>
#include "../../clientEvents.h"
#include <logEvent.h>

namespace HUD
{


	namespace WindowStack
	{
		using namespace DeltaWorks;

		typedef Component::PWindow PWindow;
		typedef Component::Window Window;
		typedef Component::WheelRs WheelRs;

		namespace Detail
		{
			struct TWindowEntry
			{
				PWindow	window;
				float	opacity = 0;
				bool	fadingIn = true;

				void	swap(TWindowEntry&other)
				{
					window.swap(other.window);
					using std::swap;
					swap(opacity,other.opacity);
					swap(fadingIn,other.fadingIn);
				}

				void	SignalWindowLayoutChange()
				{
					window->SignalWindowLayoutChange();
				}

				bool	IsVisible() const
				{
					return fadingIn;
				}


			};


			Buffer0<TWindowEntry,Swap>		visibleWindows;

			//void	HideNow(index_t index)
			//{
			//	auto&e = visibleWindows[index];
			//	Detail::visibleWindows.Erase(index);	//should be last. window is not guaranteed to be valid past this point

			//	VerifyIntegrity(CLOCATION);


			//}


			index_t	FindWindowIndex(const Window*window)
			{
				foreach (Detail::visibleWindows,_w)
					if (_w->window.get() == window)
					{
						return _w - Detail::visibleWindows.pointer();
					}
				return InvalidIndex;
			}

			bool UndoFadeOut(const PWindow&window)
			{
				bool changed = false;
				const index_t at = FindWindowIndex(window.get());
				if (at != InvalidIndex)	//first make visible again
				{
					auto&existing = Detail::visibleWindows[at];
					if (!existing.fadingIn)
					{
						auto prevTop = GetTopMostWindow();
						existing.fadingIn= true;
						changed = true;
						if (GetTopMostWindow() == window)
						{
							if (prevTop)
								prevTop->SetActive(false);
							window->OnShow();
						}
					}
				}
				return changed;
			}

			void OnInput()
			{
				auto w = GetTopMostWindow();

				if (w)
					w->DetectEnabledState();
			}
		}


		void SignalScreenResize()
		{
			Component::SignalScreenResize();
			foreach (Detail::visibleWindows,w)
				w->window->OnScreenSizeOrUIScaleChange();
		}


		PWindow				GetTopMostWindow()
		{
			for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
			{
				const auto&e = Detail::visibleWindows[i];
				if (e.IsVisible())
					return e.window;
			}
			return nullptr;
		}

		PWindow				GetBottomMostWindow()
		{
			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				const auto&e = Detail::visibleWindows[i];
				if (e.IsVisible())
					return e.window;
			}
			return nullptr;
		}


		

		
		void		Destroy()
		{
			Detail::visibleWindows.Clear();
		}


		void		Create()
		{
			Component::Image::StaticCreate();

			GlobalEvents::uiScaleChanged += []()
			{
				foreach (Detail::visibleWindows,w)
					(*w).SignalWindowLayoutChange();
				SignalScreenResize();
			};

			GlobalEvents::applicationTerminate << Destroy;

		}


		bool	Hide(const PWindow&window)
		{
			return Hide(window.get());
		}

		bool	Hide(Window*window)
		{
			VerifyIntegrity(CLOCATION);
			index_t index = Detail::FindWindowIndex(window);
			if (index == InvalidIndex)
				return false;
			auto&e = Detail::visibleWindows[index];
			if (!e.fadingIn)
				return false;
			#ifdef _DEBUG
				if (window->breakOnHide)
					__debugbreak();
			#endif
			e.window->OnHide();
			bool activateTop = false;
			if (GetTopMostWindow().get() == window)
			{
				window->SetActive(false);
				activateTop = true;
			}
			e.fadingIn= false;

			if (activateTop)
			{
				auto top = GetTopMostWindow();
				if (top)
					top->SetActive(true);
			}


			VerifyIntegrity(CLOCATION);

			SignalSituationChange();
			OverrideSignalMouseMotion();


			return true;
		}



	
		bool	IsVisible(const PWindow&window)
		{
			index_t at = Detail::FindWindowIndex(window.get());
			return at != InvalidIndex && Detail::visibleWindows[at].IsVisible();
		}

		void	Show(const PWindow&window)
		{
			if (!window)
				return;
			VerifyIntegrity(CLOCATION);

			auto top = GetTopMostWindow();
			if (top == window)
			{
				DBG_ASSERT__(window->IsActive());
				if (!window->IsActive())
					window->SetActive(true);
				return;
			}
			{
				if (top)
					top->SetActive(false);
			}

			index_t at = Detail::FindWindowIndex(window.get());

			if (at != InvalidIndex)
			{
				if (at+1 != Detail::visibleWindows.Count())
				{
					auto existing = Detail::visibleWindows[at];
					Detail::visibleWindows.Erase(at);
					Detail::visibleWindows.Append(existing);
					at = Detail::visibleWindows.Count()-1;
				}
				auto&entry = Detail::visibleWindows[at];
				if (!entry.fadingIn)
				{
					entry.fadingIn = true;
					entry.window->OnShow();
					VerifyIntegrity(CLOCATION);
				}
				else
				{
					entry.window->SetActive(true);
					VerifyIntegrity(CLOCATION);
				}

			}
			else
			{
				Detail::visibleWindows.Append().window = window;
				window->OnShow();
				VerifyIntegrity(CLOCATION);
			}
			SignalSituationChange();
			OverrideSignalMouseMotion();
		}



		bool	ShowBottom(const PWindow&window)
		{
			if (!window)
				return false;
			VerifyIntegrity(CLOCATION);

			bool changed = Detail::UndoFadeOut(window);

			auto top = GetTopMostWindow();
			if (GetBottomMostWindow() == window)
			{
				if (top == window)
				{
					DBG_ASSERT__(window->IsActive());
					if (!window->IsActive())
						window->SetActive(true);
				}
				return changed;
			}

			index_t at = Detail::FindWindowIndex(window.get());
			if (at != InvalidIndex)
			{
				if (at == 0)
					return changed;
				{
					auto existing = Detail::visibleWindows[at];
					Detail::visibleWindows.Erase(at);
					Detail::visibleWindows.Insert(0,existing);
				}

				if (top == window)	//was old top, must elevate other window
				{
					top = GetTopMostWindow();
					if (top)
					{
						window->SetActive(false);
						top->SetActive(true);
						changed = true;
					}
				}
			} 
			else
			{
				auto&e = Detail::visibleWindows.Insert(0);
				e.window = window;
				if (GetTopMostWindow() == window)
					window->SetActive(true);
				changed = true;
			}
			VerifyIntegrity(CLOCATION);
			SignalSituationChange();
			OverrideSignalMouseMotion();
			return changed;
		}


		bool	CloseTop()
		{
			VerifyIntegrity(CLOCATION);
			PWindow w = GetTopMostWindow();
			if (!w || !w->canBeImplicitlyClosed)
				return false;
			Hide(w);
			return true;
		}

		bool	CloseAllUpTo(const PWindow&w)
		{
			VerifyIntegrity(CLOCATION);
			index_t at = Detail::FindWindowIndex(w.get());
			if (at == InvalidIndex)
				return false;

			for (index_t i = 0; i <= at; i++)
			{
				auto&entry = Detail::visibleWindows[i];
				if (!entry.IsVisible())
					continue;
				if (entry.window->canBeImplicitlyClosed)
					Hide(entry.window);
			}

			return true;
		}


		bool	CloseAll()
		{
			VerifyIntegrity(CLOCATION);
			if (Detail::visibleWindows.IsEmpty())
				return false;

			bool rs = false;
			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				auto&entry = Detail::visibleWindows[i];
				if (entry.IsVisible() && entry.window->canBeImplicitlyClosed)
				{
					Hide(entry.window);
					rs = true;
				}
			}
			VerifyIntegrity(CLOCATION);
			return rs;
		}

		count_t				CountVisibleWindows()
		{
			count_t rs = 0;
			foreach (Detail::visibleWindows,w)
				if (w->IsVisible())
					rs++;
			return rs;
		}


		bool	IsModal()
		{
			foreach (Detail::visibleWindows,w)
				if (w->IsVisible() && w->window->isModal)
					return true;
			return false;
		}


		static float2 lastMouseLocation;

		void	OverrideSignalMouseMotion()
		{
			lastMouseLocation = Engine::mouse.location.windowRelative;
			for (index_t i = Detail::visibleWindows.Count()-1; i< Detail::visibleWindows.Count(); i--)
			{
				const auto&wnd = Detail::visibleWindows[i];
				if (!wnd.IsVisible())
					continue;
				wnd.window->OnMouseMove(lastMouseLocation,true);
				if (wnd.window->isModal)
					break;
			}
		}

		static void CheckMouseMotion()
		{
			if (lastMouseLocation != Engine::mouse.location.windowRelative)
			{
				OverrideSignalMouseMotion();
			}
		}

		WheelRs	OnMouseWheel(short delta)
		{
			for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
			{
				auto&w = Detail::visibleWindows[i];
				if (!w.IsVisible())
					continue;
				WheelRs rs = w.window->OnMouseWheel(delta);
				if (w.window->isModal && rs == WheelRs::NotContained)
					rs = WheelRs::Ignored;
				if (rs != WheelRs::NotContained)
				{
					if (rs == WheelRs::Processed)
						w.window->OnMouseMove(Engine::mouse.location.windowRelative,true);
					
					return rs;
				}
			}
			return WheelRs::NotContained;
		}

		bool	OnMouseDown()
		{
			for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
			{
				auto&w = Detail::visibleWindows[i];
				if (!w.IsVisible())
					continue;
				Component::mousePressedComponent = w.window->OnMouseDown();
				if (Component::mousePressedComponent || w.window->isModal)
					return true;
			}
			Component::mousePressedComponent.reset();
			return false;
		}

		void	HandleMouseUp()
		{
			if (Component::mousePressedComponent)
			{
				auto up = Component::mousePressedComponent;
				Component::mousePressedComponent.reset();
				up->OnMouseUp();
			}

		}


		void	SignalSituationChange()
		{
			static bool checking = false;
			if (checking)
				return;
			checking = true;
			VerifyIntegrity(CLOCATION);
			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				auto&d = Detail::visibleWindows[i];	//make copy to allow SignalChange() to create new windows
				if (!d.IsVisible())
					continue;
				if (!d.window->SignalChange())
					Hide(d.window);
				else
					d.window->DetectEnabledState();
			}
			VerifyIntegrity(CLOCATION);
			checking = false;
		}


		void	VerifyIntegrity( const TCodeLocation&loc)
		{
			if (Detail::visibleWindows.IsEmpty())
				return;

			bool isTop = true;
			for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
			{
				auto&w = Detail::visibleWindows[i];
				bool shouldBeActive = isTop && w.IsVisible();
				if (w.window->IsActive() != shouldBeActive)
				{
					String msg = String(loc)+ ": Activity mismatch for window "+String(i+1)+"/"+String(Detail::visibleWindows.Count())+" (active="+String(w.window->IsActive())+", should be "+String(shouldBeActive)+")";
					LogEvent(EventClass::Preset::error,msg);
					DBG_FATAL__(msg);
					w.window->SetActive(shouldBeActive);
				}

				if (w.IsVisible())
					isTop = false;
			}

			if (Detail::visibleWindows.IsEmpty() && HUD::Component::Detail::InputIsActive())
			{
				String msg = String(loc)+ ": Input should be inactive";
				LogEvent(EventClass::Preset::error,msg);
				DBG_FATAL__(msg);
				HUD::Component::Detail::DeactivateInput(nullptr);
			}
				
		}


		static const float FadeInSpeed = 4;
		static const float FadeOutSpeed = 4;

		void	RenderNonModal()
		{
			if (Detail::visibleWindows.IsNotEmpty())
			{
				CheckMouseMotion();
			}


			Component::Detail::LoadView(M::Rect<>(0,0,1,1),float2());
		
			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				auto&d = Detail::visibleWindows[i];
	
				const auto&w = d.window;


				if (w->RelayoutRequired())
				{
					w->FullAutoLayout(CLOCATION);
					w->OnMouseMove(lastMouseLocation,true);
				}
				if (w->isModal && d.IsVisible())
					break;
				if (d.opacity > 0)
					w->Render(d.opacity * d.opacity);
			}
		}

		void	RenderModal()
		{
			if (Detail::visibleWindows.IsNotEmpty())
			{
				CheckMouseMotion();
			}


			Component::Detail::LoadView(M::Rect<>(0,0,1,1),float2());
		
			bool nonModal = true;
			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				auto&d = Detail::visibleWindows[i];
				if (d.fadingIn)
				{
					d.opacity += Engine::timing.delta * FadeInSpeed;
					if (d.opacity > 1)
						d.opacity = 1;
				}
				else
				{
					d.opacity -= Engine::timing.delta * FadeOutSpeed;
					if (d.opacity <= 0)
					{
						d.opacity = 0;
						Detail::visibleWindows.Erase(i--);
						continue;
					}
				}

				const auto&w = d.window;
				if (w->isModal && d.IsVisible())
					nonModal = false;
				if (!nonModal && d.opacity > 0)
				{
					if (w->RelayoutRequired())
					{
						w->FullAutoLayout(CLOCATION);
						w->OnMouseMove(lastMouseLocation,true);
					}

					w->Render(d.opacity * d.opacity);
				}
			}
		}
	}



	namespace Component
	{
		namespace Detail
		{	
			void OnInput()
			{
				WindowStack::Detail::OnInput();
			}
		}
	}

}
