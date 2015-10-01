#ifdef HAS_WINDOW
#ifndef gameGUIH
#define gameGUIH

#include <global_string.h>
#include <math/vclasses.h>

#include <memory>
#include "../ExtRenderer/DirectX11.h"

namespace GUIElements
{

	namespace DX11 = Engine::DX11;

	class Component
	{
	public:
		Rect<>			rect;
		bool			enabled = true;

		void			adoptData(Component&other)
		{
			rect = other.rect;
			enabled = other.enabled;
		}



		bool			ContainsMouse() const;
		void			MoveTopTo(float h);

		virtual void	Activate() {};
		virtual void	Deactivate() {};
		virtual void	Check() {};
		virtual void	HandleClick() {};
		virtual bool	UpdateIfNecessary()	{return false;}
		virtual void	UpdateNow() {};
		virtual void	Render() {};
	};

	typedef std::shared_ptr<Component> PComponent;


	class Image : public Component
	{
		typedef Component	Super;
	public:
		DX11::Texture	texture;
		bool			sensitiveToMouse = false;


		void			adoptData(Image&other)
		{
			Super::adoptData(other);
			sensitiveToMouse = other.sensitiveToMouse;
			texture.swap(other.texture);
		}

		virtual void	Render() override;
	};
	typedef std::shared_ptr<Image> PImage;




	class Area : public Component
	{
	private:
		//Buffer0<float3>	hexCenters;
		//DX11::BaseInstructionBlock	instruction;
		//static DX11::BaseInstructionBlock	shader;
	protected:
		void			BaseRender(bool lit);
	public:
		typedef Component	Super;

		index_t			sizeVersion = InvalidIndex;
		bool			sensitiveToMouse = true;
		bool			renderRect = true;

		virtual void	UpdateNow() override;

		//static void		StaticCreate();
		//static void		StaticDestroy();

		void			adoptData(Area&other)
		{
			sensitiveToMouse = other.sensitiveToMouse;
			renderRect = other.renderRect;
			sizeVersion = other.sizeVersion;
			Super::adoptData(other);
		}
		virtual void	Render() override	{BaseRender(false);}
	};

	class Input : public Area
	{
		typedef Area	Super;
		String			backup;
		Rect<>			core;
		String			allowed = "abcdefghijklmnopqrstuvwxyzäöüABCDEFGHIJKLMNOPQRSTUVWXYZÄÖÜ0123456789_- ";
		bool			isActive = false;

		static void		OnCharHandled(char)
		{
			if (onChange)
				onChange();
		}
	public:

		static std::function<void()>	onChange;
		static std::function<void()>	onDone;


		void			ActivateInput();
		void			DeactivateInput();
		String			GetInput();
		void			SetCurrent(const String&);

		virtual void	Activate() override {ActivateInput();}
		virtual void	Deactivate() override {DeactivateInput();}


		virtual bool	UpdateIfNecessary() override;

		virtual void	Render() override;

		void			SetAllowed(const String&);

		void			adoptData(Input&other)
		{
			isActive = other.isActive;
			backup.adoptData(other.backup);
			core = other.core;
			allowed.adoptData(other.allowed);
			Super::adoptData(other);
		}
	};
	typedef std::shared_ptr<Input>	PInput;

	class TextField : public Area
	{
	private:
		Array<String>	rawText;
		Buffer0<String>	wrappedText;
		Array<float2>	lineAt;
	public:
		typedef Area	Super;

		bool			alignCenter = true;
		float			zoom = 1.f;


		void			adoptData(TextField&other)
		{
			rawText.adoptData(other.rawText);
			wrappedText.adoptData(other.wrappedText);
			Super::adoptData(other);
			alignCenter= other.alignCenter;
			zoom = other.zoom;
		}

		void			SetText(const String&string)
		{
			if (rawText.GetLength() == 1 && rawText.first() == string)
				return;
			rawText.SetSize(1);
			rawText.first() = string;
			sizeVersion = InvalidIndex;
		}
		void			SetText(const Array<String>&string)
		{
			if (rawText == string)
				return;
			rawText = string;
			sizeVersion = InvalidIndex;
		}


		virtual bool	UpdateIfNecessary() override;

		virtual void	Render() override;
	};
	typedef std::shared_ptr<TextField>	PTextField;


	class Button : public TextField
	{
		typedef TextField	Super;

		std::function<void()>	onPress;
		std::function<bool()>	check;

		String					baseLabel;
	public:

		void			adoptData(Button&other)
		{
			Super::adoptData(other);
			onPress.swap(other.onPress);
			baseLabel.adoptData(other.baseLabel);
		}

		void			SetText(const String&string)
		{
			baseLabel = string;
			Check();
		}


		void			SetOnPress(const std::function<void()>&event)
		{
			onPress = event;
		}

		void			SetCheck(const std::function<bool()>&check)
		{
			this->check = check;
			Check();
		}

		virtual void	Check() override
		{
			String set = baseLabel;
			if (check && !check())
			{
				enabled = false;
				set += " (not right now)";
			}
			else
				enabled = true;

			Super::SetText(set);
		}

		virtual	void			HandleClick() override
		{
			if (enabled && onPress && ContainsMouse())
				onPress();
		}
	};
	typedef std::shared_ptr<Button>	PButton;


	
	void				Darken(const Rect<>&rect, float alpha=0.9f, bool flush=false);
	


	void				OnWindowResize();


	void				Create();
	void				Destroy();




}






#endif
#endif
