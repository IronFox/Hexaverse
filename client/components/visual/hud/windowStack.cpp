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
			Buffer0<PWindow,Swap>		visibleWindows;

			void OnInput()
			{
				if (visibleWindows.IsNotEmpty())
					visibleWindows.last()->DetectEnabledState();
			}
		}


		void SignalScreenResize()
		{
			Component::SignalScreenResize();
			foreach (Detail::visibleWindows,w)
				(*w)->OnScreenSizeOrUIScaleChange();
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
					(*w)->InvalidateAutoLayout();
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
			index_t index = FindWindowIndex(window);
			if (index == InvalidIndex)
				return false;

			#ifdef _DEBUG
				if (window->breakOnHide)
					__debugbreak();
			#endif

			window->OnHide();
			if (index+1 == Detail::visibleWindows.Count())
			{
				window->SetActive(false);
				if (Detail::visibleWindows.Count() > 1)
					Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
			}
			Detail::visibleWindows.Erase(index);	//should be last. window is not guaranteed to be valid past this point

			VerifyIntegrity(CLOCATION);

			SignalSituationChange();
			OverrideSignalMouseMotion();
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
					window->SetActive(true);
				return;
			}
			//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.First() == window)
			//	return;
			Detail::visibleWindows.FindAndErase(window);
			if (Detail::visibleWindows.IsNotEmpty())
				Detail::visibleWindows.last()->SetActive(false);
			Detail::visibleWindows << window;
			window->OnShow();
			SignalSituationChange();
			OverrideSignalMouseMotion();
		}



		bool	ShowBottom(const PWindow&window)
		{
			if (!window)
				return false;
			VerifyIntegrity(CLOCATION);
			if (Detail::visibleWindows.IsNotEmpty() && Detail::visibleWindows.First() == window)
			{
				if (Detail::visibleWindows.Count() == 1)
				{
					DBG_ASSERT__(window->IsActive());
					if (!window->IsActive())
						window->SetActive(true);
				}
				return false;
			}
			//if (Detail::visibleWindows.Count() == 1 && Detail::visibleWindows.First() == window)
			//	return;
			bool rs = true;
			index_t at = Detail::visibleWindows.GetIndexOf(window);
			if (at != InvalidIndex)
			{
				if (at+1 == Detail::visibleWindows.Count())	//ist last
				{
					window->SetActive(false);
					Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
				}
				Detail::visibleWindows.Erase(at);
				rs = false;
			} 
			Detail::visibleWindows.Insert(0,window);
			if (Detail::visibleWindows.Count() == 1)
				window->SetActive(true);

			SignalSituationChange();
			OverrideSignalMouseMotion();
			return rs;
		}


		bool	CloseTopModal()
		{
			VerifyIntegrity(CLOCATION);
			if (Detail::visibleWindows.IsEmpty())
				return false;
			PWindow w = Detail::visibleWindows.Last();
			if (!w->canBeImplicitlyClosed)
				return false;
			Hide(w);
			return true;
		}

		bool	CloseAllModalBeneath(const PWindow&w)
		{
			VerifyIntegrity(CLOCATION);
			index_t at = Detail::visibleWindows.GetIndexOf(w);
			if (at == InvalidIndex)
				return false;

			for (index_t i = 0; i <= at; i++)
			{
				PWindow w = Detail::visibleWindows[i];
				if (w->canBeImplicitlyClosed)
				{
					Hide(w);
					i--;
					if (at == 0)
						return true;
					at--;
				}
			}

			return true;
		}


		bool	CloseAllModal()
		{
			VerifyIntegrity(CLOCATION);
			if (Detail::visibleWindows.IsEmpty())
				return false;

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
					PWindow w = Detail::visibleWindows[i];
					if (w->canBeImplicitlyClosed)
					{
						Hide(w);
						i--;
						rs = true;
					}
				}
				VerifyIntegrity(CLOCATION);
				return rs;
			}
			else
			{
				foreach (Detail::visibleWindows,_d)
				{
					PWindow d = *_d;
					d->OnHide();
				}
				Detail::visibleWindows.last()->SetActive(false);
				Detail::visibleWindows.Clear();
			}
			VerifyIntegrity(CLOCATION);
			return true;
		}


		bool	IsModal()
		{
			foreach (Detail::visibleWindows,w)
				if ((*w)->isModal)
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
				wnd->OnMouseMove(lastMouseLocation,true);
				if (wnd->isModal)
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
				PWindow w = Detail::visibleWindows[i];
				WheelRs rs = w->OnMouseWheel(delta);
				if (w->isModal && rs == WheelRs::NotContained)
					rs = WheelRs::Ignored;
				if (rs != WheelRs::NotContained)
				{
					if (rs == WheelRs::Processed)
						w->OnMouseMove(Engine::mouse.location.windowRelative,true);
					
					return rs;
				}
			}
			return WheelRs::NotContained;
		}

		bool	OnMouseDown()
		{
			for (index_t i = Detail::visibleWindows.Count()-1; i < Detail::visibleWindows.Count(); i--)
			{
				PWindow w = Detail::visibleWindows[i];
				Component::mousePressedComponent = w->OnMouseDown();
				if (Component::mousePressedComponent || w->isModal)
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
				PWindow d = Detail::visibleWindows[i];	//make copy to allow SignalChange() to create new windows
				if (!d->SignalChange())
				{
					d->OnHide();
					if (i+1 == Detail::visibleWindows.Count())
					{
						d->SetActive(false);
						if (Detail::visibleWindows.Count() > 1)
							Detail::visibleWindows.GetFromEnd(1)->SetActive(true);
					}
					Detail::visibleWindows.Erase(i);
					i--;
				}
				else
					d->DetectEnabledState();
			}
			VerifyIntegrity(CLOCATION);
			checking = false;
		}


		void	VerifyIntegrity(const TCodeLocation&loc)
		{
			if (Detail::visibleWindows.IsEmpty())
				return;


			for (index_t i = 0; i < Detail::visibleWindows.Count(); i++)
			{
				bool shouldBeActive = (i+1 == Detail::visibleWindows.Count());
				PWindow w = Detail::visibleWindows[i];
				if (w->IsActive() != shouldBeActive)
				{
					String msg = String(loc)+ ": Activity mismatch for window "+String(i+1)+"/"+String(Detail::visibleWindows.Count())+" (active="+String(w->IsActive())+", should be "+String(shouldBeActive)+")";
					LogEvent(EventClass::Preset::error,msg);
					DBG_FATAL__(msg);
					if (shouldBeActive)
						w->SetActive(true);
					else
						w->SetActive(false);
				}
			}
		}




		void	RenderNonModal()
		{
			if (Detail::visibleWindows.IsNotEmpty())
			{
				CheckMouseMotion();
			}


			Component::Detail::LoadView(M::Rect<>(0,0,1,1),float2());
		
			foreach (Detail::visibleWindows,d)
			{
				if ((*d)->RelayoutRequired())
					(*d)->FullAutoLayout(CLOCATION);
				if ((*d)->isModal)
					break;
				(*d)->Render();
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
			foreach (Detail::visibleWindows,d)
			{
				if ((*d)->isModal)
					nonModal = false;
				if (!nonModal)
					(*d)->Render();
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
