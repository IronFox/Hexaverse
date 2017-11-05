#pragma once

#ifdef HAS_WINDOW

#include "dialog.h"
#include <io/config.h>
#include <global_string.h>

namespace HUD
{

namespace Menu
{
	class Menu;
	typedef std::shared_ptr<Menu>	PMenu;


	class Menu : public Component::Window
	{
		typedef Window	Super;

		String		myTitle;
		HUD::Component::PLabel	hintLabel;

		friend class Super;
	protected:
		/**/		Menu(){Super::logFullAutoLayout = true;}
	public:
		void		Setup(float initialAbsoluteWidth, const String&title, bool escapeCloses);

		static PMenu		Create()
		{
			return Super::CreateDerivative<Menu>();;
		}


		virtual void				OnScreenSizeOrUIScaleChange() override;

		HUD::Component::PButton		AddCommand(const String&name, const std::function<void()>&execute, const std::function<bool()>&check = std::function<bool()>(), bool autoHide=true);


	};



	extern PMenu	main,help,login;

	namespace Configuration
	{
		void	Assign(::DeltaWorks::Configuration&, const ::DeltaWorks::PathString&userConfigPath);
	}


	void					Create();
};
}

#endif
