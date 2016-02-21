#ifndef visualCommandsH
#define visualCommandsH

#ifdef HAS_WINDOW


#include <global_string.h>
#include <math/vclasses.h>
#include "../../client/ExtRenderer/DirectX11.h"
#include "cursorMode.h"
#include "types.h"

namespace Objects
{
	namespace DX11 = Engine::DX11;




	void				RefreshCommands(bool controlEnabled);





	class BaseCommand
	{
	public:
		enum class Flags
		{
			None = 0,
			IsEnabled=0x1,
			IsPermaActive=0x2,
			IsFlashing=0x4
		};


		String			name;
		Flags			state = Flags::IsEnabled;
		int2			coords;
		DX11::Texture	texture;

		static const int2 coordOffset;

		/**/			BaseCommand(int2 coords, const String&name):name(name),coords(coords+coordOffset)
		{}

		virtual void	Execute() = 0;
		virtual	void	Refresh(bool controlEnabled) = 0;

		#undef LoadIcon
		static GenericHashTable<PathString, DX11::Texture>	textureMap;

		virtual void	RasterizeImage()	{}

		void			LoadIcon();
		void			LoadIcon(const String&name);


		class Label
		{
		public:
			Buffer0<String>		lines;
			count_t				briefEndAt = InvalidIndex;


			void				Clear()
			{
				briefEndAt = InvalidIndex;
				lines.Clear();
			}

			void				MarkBriefEnd()
			{
				briefEndAt = std::min( lines.Count(), briefEndAt);	//don't extend once set
			}
			/**
			Wraps text as necessary
			*/
			void				WrapAppendText(const String&multiLineText)
			{
				if (multiLineText.length() <= 60)
				{
					lines << multiLineText;
					return;
				}
				Array<String>	split;
				wrap(multiLineText,60,split);
				lines.Append(split);
			}

			Label&				operator<<(const String&line)
			{
				WrapAppendText(line);
				//lines << line;
				return *this;
			}




		};



		virtual void	CompileLabel(const PWorldEntity&, Label&outResult)
		{
			outResult << name;
		}

	};

	inline BaseCommand::Flags operator|(BaseCommand::Flags a, BaseCommand::Flags b)
	{
		using T = std::underlying_type_t <BaseCommand::Flags>;
		return (BaseCommand::Flags)(((T)a)|((T)b));
	}
	
	inline void operator|=(BaseCommand::Flags&a, BaseCommand::Flags b)
	{
		using T = std::underlying_type_t <BaseCommand::Flags>;
		a = (BaseCommand::Flags)(((T)a)|((T)b));
	}

	inline bool operator&(BaseCommand::Flags a, BaseCommand::Flags b)
	{
		using T = std::underlying_type_t <BaseCommand::Flags>;
		return (((T)a)&((T)b)) != 0;
	}

	typedef std::shared_ptr<BaseCommand>	PBaseCommand;


	template <class Target>
		class TemplateCommand : public BaseCommand
		{
		public:
			typedef std::function<void(Target)>	OnExecute;
			typedef std::function<Flags(Target, bool)>	Check;

			OnExecute	execute;
			Check		check;

			TFloatRect	screenRect;

			/**/		TemplateCommand():BaseCommand(int2(0),""){}
			/**/		TemplateCommand(int2 coords, const String&name, OnExecute f, Check c) :BaseCommand(coords,name), execute(f), check(c)
			{}

		};





	class EntityCommand : public TemplateCommand<PWorldEntity const&>
	{
	public:
		typedef TemplateCommand<const PWorldEntity&>	Super;
		typedef EntityCommand	This;

		/**/				This()	{}
		/**/				This(int2 coords, const String&name, OnExecute f, Check c) : Super(coords,name,f,c)
		{}

		virtual void		Refresh(bool controlEnabled) override;
		virtual void		Execute() override;
	};

	typedef std::function<void(EntityCommand&cmd)>	FEntityCommandVisitor;

	class IconCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;
		typedef IconCommand	This;

		typedef std::function<void(const PWorldEntity&e, int&mineralCostOut, int&timeCostOut)>	GetCosts;

		GetCosts			getCosts;

		/**/				This()	{}
		/**/				This(int2 coords, const String&name, OnExecute f, Check ch, GetCosts c) : Super(coords,name,f,ch),getCosts(c)
		{
			LoadIcon();
		}
		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;

	};

	class VehicleCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;

		Component::VehicleBase		*base;
		Component::Turret	*turret;

		/**/				VehicleCommand()	{}
		/**/				VehicleCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret,   OnExecute f, Check c, bool rasterize=true) : Super(coords,name,f,c),base(base),turret(turret)
		{
			if (rasterize)
				UpdateTexture(base,turret);
		}

		void				UpdateTexture(Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor = float4(0));
		void				UpdateTexture(const PVehicle&v, bool isPrototype, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(EntityCommand&target, Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(EntityCommand&target, const PVehicle&v, bool isPrototype, const float4&backgroundColor = float4(0));
	};

	class VehicleConstructionCommand : public VehicleCommand
	{
	public:
		typedef VehicleCommand	Super;
		typedef VehicleConstructionCommand	This;

		unsigned			roundsLeft = 0xFFFFFFFF;
		unsigned			mineralsLeft = 0xFFFFFFFF;

		/**/				VehicleConstructionCommand()	{}
		/**/				VehicleConstructionCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret,   OnExecute f, Check c) : Super(coords,name,base,turret,f,c)	{}


		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;
	};

	class SatelliteCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;
		WSatellite			sat;

		/**/				SatelliteCommand()	{}
		/**/				SatelliteCommand(int2 coords, const String&name, const PSatellite&sat,   OnExecute f, Check c, bool rasterize=true) : Super(coords,name,f,c),sat(sat)
		{
			if (rasterize)
				UpdateTexture(sat,true);
		}


		void				UpdateTexture(const PSatellite&v, bool isPrototype, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(EntityCommand&target, const PSatellite&v, bool isPrototype, const float4&backgroundColor = float4(0));

	};

	class SatelliteConstructionCommand : public SatelliteCommand
	{
	public:
		typedef SatelliteCommand	Super;


		/**/				SatelliteConstructionCommand()	{}
		/**/				SatelliteConstructionCommand(int2 coords, const String&name, const PSatellite&sat,   OnExecute f, Check c, bool rasterize=true) : Super(coords,name,sat,f,c,rasterize) {}

		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;
	};


	class MobileEntityConstructionAbortCommand : public SatelliteConstructionCommand
	{
	public:
		typedef SatelliteConstructionCommand	Super;
		Component::Turret*		turret;
		Component::VehicleBase*	base;

		unsigned			roundsLeft = 0xFFFFFFFF;
		unsigned			mineralsLeft = 0xFFFFFFFF;


		/**/				MobileEntityConstructionAbortCommand()	{}
		/**/				MobileEntityConstructionAbortCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret, const PSatellite&sat,   OnExecute f, Check c, bool rasterize=true) : Super(coords,name,sat,f,c, rasterize && sat),turret(turret),base(base)
		{
			if (rasterize && !sat)
				VehicleConstructionCommand::UpdateTexture(*this,base,turret);
		}


		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;



	};


	class DeployVehicleCommand : public VehicleCommand
	{
	public:
		typedef VehicleCommand	Super;
		typedef DeployVehicleCommand	This;
		WVehicle			vehicle;


		/**/				This()	{}
		/**/				This(int2 coords, const PVehicle&v,   OnExecute f, Check c);



		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;
	};

	class StructureCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;
		typedef StructureCommand	This;

		WStructure			structure;

		/**/				StructureCommand()	{}
		/**/				StructureCommand(int2 coords, const String&name, const PStructure&structure,   OnExecute f, Check c) : Super(coords,name,f,c),structure(structure)
		{
			//UpdateTexture(structure);
		}

		virtual void		RasterizeImage() override	{PStructure s = structure.lock(); if (s) UpdateTexture(s,true);}
		void				UpdateTexture(const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(EntityCommand&target, const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));

	};

	class StructureConstructionCommand : public StructureCommand
	{
	public:
		typedef StructureCommand	Super;
		typedef StructureConstructionCommand	This;


		/**/				StructureConstructionCommand()	{}
		/**/				StructureConstructionCommand(int2 coords, const String&name, const PStructure&structure,   OnExecute f, Check c) : Super(coords,name,structure,f,c)
		{
			//UpdateTexture(structure);
		}

		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;

	};


	class GlobalCommand : public TemplateCommand<int>
	{
	public:
		typedef TemplateCommand<int>	Super;
		typedef GlobalCommand			This;

		/**/				This()	{}
		/**/				This(int2 coords, const String&name, OnExecute f, Check c) : Super(coords,name,f,c)
		{
			LoadIcon();
		}

		virtual void		Refresh(bool controlEnabled) override;
		virtual void		Execute() override {execute(0);}

	};


	extern Buffer0<GlobalCommand>			globalCommands;



	
	namespace VehicleCmd
	{
		void	UndoOperation(const PWorldEntity&);
		void	AbortOperation(const PWorldEntity&);
		void	Automate(const PWorldEntity&);
		void	Sleep(const PWorldEntity&);

		bool	IsAutomated(const PWorldEntity&);

		BaseCommand::Flags AutomatedCheck(const PWorldEntity&,bool controlEnabled);
		BaseCommand::Flags SleepCheck(const PWorldEntity&,bool controlEnabled);

	}


	namespace CommonCmd
	{
		template<typename T>
			inline static bool AlwaysTrue(T)	{ return true; }
		inline static bool AlwaysTrueV()	{ return true; }
		template<typename T>
			inline static bool AlwaysFalse(T)	{ return false; }
		inline static bool AlwaysFalseV()	{ return false; }
		#ifdef CLIENT
			template<typename T>
				inline static bool AlwaysEnabled(T,bool)	{return CanEnableAtAll();}
		#else
			template<typename T>
				inline static bool AlwaysEnabled(T,bool)	{return true;}
		#endif
		inline BaseCommand::Flags CommonButtonCheck(const PWorldEntity&,bool controlEnabled)	{return controlEnabled ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;}


		void ToggleMode(const PWorldEntity&e, CursorMode mode);
	
		template <CursorMode M>
			inline void ToggleMode(const PWorldEntity&e)
			{
				ToggleMode(e,M);
			}


		bool IsModeActive(const PWorldEntity&e, CursorMode mode);
		bool IsModeActiveOrIdle(const PWorldEntity&e, CursorMode mode);

		template <CursorMode M>
			inline bool IsModeActive(const PWorldEntity&e)
			{
				return IsModeActive(e,M);
			}
		template <CursorMode M>
			inline bool IsModeActiveC(const PWorldEntity&e, bool controlEnabled)
			{
				return controlEnabled && IsModeActive(e,M);
			}
		template <CursorMode M>
			inline bool IsModeActiveOrIdleC(const PWorldEntity&e, bool controlEnabled)
			{
				return controlEnabled && IsModeActiveOrIdle(e,M);
			}

		template <CursorMode M>
			inline BaseCommand::Flags ModeCheck(const PWorldEntity&e, bool controlsEnabled)
			{
				BaseCommand::Flags rs = BaseCommand::Flags::None;
				if (IsModeActiveOrIdleC<M>(e,controlsEnabled))
					rs |= BaseCommand::Flags::IsEnabled;
				if (CommonCmd::IsModeActive<M>(e))
					rs |= BaseCommand::Flags::IsPermaActive;
				return rs;
			}
		inline BaseCommand::Flags IdleCheck(const PWorldEntity&e, bool controlsEnabled)
		{
			return controlsEnabled && activeCursorMode == CursorMode::Idle ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
		}



	}


	namespace GlobalCmd
	{
		void	ToggleSeeThrough(int);
		void	SeeThroughUp(int);
		void	SeeThroughDown(int);
		bool	SeeThroughIsActive(int);
		bool	SeeThroughIsActive2(int,bool);
		void	ToggleObjectView(int);
		bool	ObjectsAreHidden(int);

	}

}


#endif

#endif
