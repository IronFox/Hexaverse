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
		String			name;
		bool			permaActive, isEnabled;
		int2			coords;
		DX11::Texture	texture;

		static const int2 coordOffset;

		/**/			BaseCommand(int2 coords, const String&name):name(name),permaActive(false), isEnabled(true),coords(coords+coordOffset)
		{}

		virtual void	Execute() = 0;

		#undef LoadIcon
		static GenericHashTable<PathString, DX11::Texture>	textureMap;

		virtual void	RasterizeImage()	{}

		void			LoadIcon();


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

	template <class Target>
		class TemplateCommand : public BaseCommand
		{
		public:
			typedef std::function<void(Target)>	OnExecute;
			typedef std::function<bool(Target, bool)>	CheckEnabled;
			typedef std::function<bool(Target)>	IsPermaActive;

			OnExecute	execute;
			CheckEnabled	checkEnabled;
			IsPermaActive	checkPermaActive;

			TFloatRect	screenRect;

			/**/		TemplateCommand():BaseCommand(int2(0),""){}
			/**/		TemplateCommand(int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p) :BaseCommand(coords,name), execute(f), checkEnabled(a), checkPermaActive(p)
			{}

			virtual	void	Refresh(bool controlEnabled) = 0;
		};





	class EntityCommand : public TemplateCommand<PWorldEntity const&>
	{
	public:
		typedef TemplateCommand<const PWorldEntity&>	Super;
		typedef EntityCommand	This;

		/**/				This()	{}
		/**/				This(int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p) : Super(coords,name,f,a,p)
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
		/**/				This(int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p, GetCosts c) : Super(coords,name,f,a,p),getCosts(c)
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
		/**/				VehicleCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret,   OnExecute f, CheckEnabled a, IsPermaActive p, bool rasterize=true) : Super(coords,name,f,a,p),base(base),turret(turret)
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
		/**/				VehicleConstructionCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret,   OnExecute f, CheckEnabled a, IsPermaActive p) : Super(coords,name,base,turret,f,a,p)	{}


		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;
	};

	class SatelliteCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;
		PSatellite			sat;

		/**/				SatelliteCommand()	{}
		/**/				SatelliteCommand(int2 coords, const String&name, const PSatellite&sat,   OnExecute f, CheckEnabled a, IsPermaActive p, bool rasterize=true) : Super(coords,name,f,a,p),sat(sat)
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
		/**/				SatelliteConstructionCommand(int2 coords, const String&name, const PSatellite&sat,   OnExecute f, CheckEnabled a, IsPermaActive p, bool rasterize=true) : Super(coords,name,sat,f,a,p,rasterize) {}

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
		/**/				MobileEntityConstructionAbortCommand(int2 coords, const String&name, Component::VehicleBase*base, Component::Turret*turret, const PSatellite&sat,   OnExecute f, CheckEnabled a, IsPermaActive p, bool rasterize=true) : Super(coords,name,sat,f,a,p, rasterize && sat),turret(turret),base(base)
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
		PVehicle			vehicle;


		/**/				This()	{}
		/**/				This(int2 coords, const PVehicle&v,   OnExecute f, CheckEnabled a, IsPermaActive p);



		virtual void		CompileLabel(const PWorldEntity&, Label&outResult) override;
	};

	class StructureCommand : public EntityCommand
	{
	public:
		typedef EntityCommand	Super;
		typedef StructureCommand	This;

		PStructure			structure;

		/**/				StructureCommand()	{}
		/**/				StructureCommand(int2 coords, const String&name, const PStructure&structure,   OnExecute f, CheckEnabled a, IsPermaActive p) : Super(coords,name,f,a,p),structure(structure)
		{
			//UpdateTexture(structure);
		}

		virtual void		RasterizeImage() override	{UpdateTexture(structure,true);}
		void				UpdateTexture(const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(EntityCommand&target, const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));

	};

	class StructureConstructionCommand : public StructureCommand
	{
	public:
		typedef StructureCommand	Super;
		typedef StructureConstructionCommand	This;


		/**/				StructureConstructionCommand()	{}
		/**/				StructureConstructionCommand(int2 coords, const String&name, const PStructure&structure,   OnExecute f, CheckEnabled a, IsPermaActive p) : Super(coords,name,structure,f,a,p)
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
		/**/				This(int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p) : Super(coords,name,f,a,p)
		{
			LoadIcon();
		}

		virtual void		Refresh(bool controlEnabled) override
		{
			permaActive = checkPermaActive(0); 
			isEnabled = checkEnabled(0,controlEnabled);
		}
		virtual void		Execute() override {execute(0);}

	};


	extern Buffer0<GlobalCommand>			globalCommands;



	
	namespace VehicleCmd
	{
		void	UndoOperation(const PWorldEntity&);
		void	AbortOperation(const PWorldEntity&);
		void	Automate(const PWorldEntity&);

		bool	IsAutomated(const PWorldEntity&);

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
		template<typename T>
			inline static bool MostlyEnabled(T,bool controlEnabled)	{return controlEnabled;}


		void ToggleMode(const PWorldEntity&e, CursorMode::mode_t mode);
	
		template <CursorMode::mode_t M>
			inline void ToggleMode(const PWorldEntity&e)
			{
				ToggleMode(e,M);
			}


		bool IsModeActive(const PWorldEntity&e, CursorMode::mode_t mode);
		bool IsModeActiveOrIdle(const PWorldEntity&e, CursorMode::mode_t mode);
		template <CursorMode::mode_t M>
			inline bool IsModeActive(const PWorldEntity&e)
			{
				return IsModeActive(e,M);
			}
		template <CursorMode::mode_t M>
			inline bool IsModeActiveC(const PWorldEntity&e, bool controlEnabled)
			{
				return controlEnabled && IsModeActive(e,M);
			}
		template <CursorMode::mode_t M>
			inline bool IsModeActiveOrIdleC(const PWorldEntity&e, bool controlEnabled)
			{
				return controlEnabled && IsModeActiveOrIdle(e,M);
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
		void	ToggleCompoundHightlights(int);
		bool	CompoundAreHightlighted(int);
	}

}


#endif

#endif
