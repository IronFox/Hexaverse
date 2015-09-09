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

	class Field
	{
	public:
		Rect<>			rect;
	
		void			adoptData(Field&other)
		{
			rect = other.rect;
		}

		bool			ContainsMouse() const;
		void			MoveTopTo(float h);

	
	};


	class ImageField : public Field
	{
		typedef Field	Super;
	public:
		DX11::Texture	texture;

		void			adoptData(ImageField&other)
		{
			Super::adoptData(other);
			texture.swap(other.texture);
		
		}

		void			Render(bool enabled, bool sensitiveToMouse = true);

	
	};

	class Area : public Field
	{
	private:
		//Buffer0<float3>	hexCenters;
		//DX11::BaseInstructionBlock	instruction;
		//static DX11::BaseInstructionBlock	shader;
	public:
		typedef Field	Super;

		index_t			sizeVersion = InvalidIndex;

		void			UpdateNow();
		void			Render(bool lit);

		//static void		StaticCreate();
		//static void		StaticDestroy();

		void			adoptData(Area&other)
		{
			sizeVersion = other.sizeVersion;
			Super::adoptData(other);
		}
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

		bool			UpdateIfNecessary();
		void			Render(bool enabled, bool sensitiveToMouse = true, bool renderRect =true);

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


		bool			UpdateIfNecessary();
		void			Render(bool enabled, bool sensitiveToMouse = true, bool renderRect =true);
	};



	
	void				Darken(const Rect<>&rect, float alpha=0.9f, bool flush=false);
	


	void				OnWindowResize();


	void				Create();
	void				Destroy();




}






#endif
#endif
