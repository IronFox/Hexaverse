#ifndef visualCommandsH
#define visualCommandsH

#ifdef HAS_WINDOW


#include <global_string.h>
#include <math/vclasses.h>
#include "../../client/ExtRenderer/DirectX11.h"
#include "cursorMode.h"
#include "types.h"
#include "resource.h"
#include "../cast.h"
#include "componentAttribute.h"
#include "../stringTable.h"

namespace Objects
{
	namespace DX11 = Engine::DX11;




	void				RefreshCommands(bool controlEnabled);

	class BaseCommand;

	typedef std::function<void(const int2&,const PBaseCommand&cmd)>	FCommandVisitor;





	class BaseCommand : public std::enable_shared_from_this<BaseCommand>
	{
	public:
		enum class Flags
		{
			None = 0,
			IsEnabled=0x1,
			IsPermaActive=0x2,
			IsFlashing=0x4,
			SoftFlashing=0x8,
			IsInvisible=0x10
		};


		String			name;
		Flags			state = Flags::IsEnabled;
		DX11::Texture	texture;
		TFloatRect		screenRect;


		static const int2 coordOffset;


		/**/			BaseCommand(const String&name):name(name) {}
		///**/			BaseCommand(const int2&coords, const String&name):name(name) {SetCoords(coords);}
		/**/			BaseCommand()	{}

		/**
		Triggered when the local command is executed
		*/
		virtual void	Execute() = 0;
		/**
		Triggered when @a state is supposed to be updated
		@param controlEnabled Indicates that the current phase and client state allows execution of server-commands. Commands that do not involve server-interaction may ignore this parameter
		*/
		virtual	void	Refresh(bool controlEnabled) = 0;
		/**
		Queries any subsequent costs of an execution of the local command
		*/
		virtual void	GetCosts(int&costOut, Resource&costTypeOut, int&timeCostOut)	{costOut = 0; costTypeOut = Resource::Minerals; timeCostOut = 0;}

		#undef LoadIcon
		static GenericHashTable<PathString, DX11::Texture>	textureMap;

		virtual void	RasterizeImage()	{}
		virtual bool	IsGroup() const {return false;}
		virtual void	VisitMembers(const FCommandVisitor&visitor, bool force, const int2&parentGridCoords) {visitor(parentGridCoords,shared_from_this());}
		void			VisitMembers(const FCommandVisitor&visitor, bool force) {VisitMembers(visitor,force,coordOffset);}

		virtual void	LoadIcon();
		void			LoadIcon(const String&name);


		//void			SetCoords(const int2&rawCoords)
		//{
		//	gridCoords = rawCoords+coordOffset;
		//}


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



		virtual void	CompileLabel(Label&outResult);

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

	inline void operator-=(BaseCommand::Flags&a, BaseCommand::Flags b)
	{
		using T = std::underlying_type_t <BaseCommand::Flags>;
		a = (BaseCommand::Flags)(((T)a)&(~((T)b)));
	}


	typedef std::shared_ptr<BaseCommand>	PBaseCommand;


	class CallbackCommand : public BaseCommand
	{
		typedef	BaseCommand	Super;
	protected:
		/**/			CallbackCommand(const String&name):Super(name)	{}
	public:
		typedef std::function<void()>		FOnExecute;
		typedef std::function<Flags(bool)>	FCheck;
		typedef std::function<void(int&costOut, Resource&costTypeOut, int&timeCostOut)>	FGetCosts;

		FGetCosts		getCosts;
		FOnExecute		execute;
		FCheck			check;

		/**/			CallbackCommand()	{}
		/**/			CallbackCommand(const String&name, const FOnExecute&exec, const FCheck&check, const FGetCosts&getCosts):Super(name), execute(exec),check(check),getCosts(getCosts)	{}
		/**/			CallbackCommand(const String&name, const FOnExecute&exec, const FCheck&check):Super(name), execute(exec),check(check)	{}

		virtual void	Execute() override												{execute();}
		virtual	void	Refresh(bool controlEnabled) override							{state = check(controlEnabled);}
		virtual void	GetCosts(int&costOut, Resource&costTypeOut, int&timeCostOut) override	{if (getCosts) getCosts(costOut,costTypeOut,timeCostOut); else Super::GetCosts(costOut,costTypeOut,timeCostOut);}
	};



	template <typename Base>
		class CustomLabel : public Base
		{
			typedef	Base	Super;
		public:
			BaseCommand::Label		customLabel;

			/**/		CustomLabel(const String&name):Super(name) {customLabel << name; customLabel.MarkBriefEnd();}
			/**/		CustomLabel() {}
			virtual void	CompileLabel(BaseCommand::Label&outResult) override
			{
				outResult = customLabel;
			}
		};


	class IconCommand : public CustomLabel<CallbackCommand>
	{
		typedef	CustomLabel<CallbackCommand>	Super;
	protected:
		/**/			IconCommand(const String&name):Super(name)	{}

	public:

		/**/			IconCommand()	{}
		/**/			IconCommand(const String&name, const FOnExecute&exec, const FCheck&check, const FGetCosts&getCosts):Super(name)
		{
			LoadIcon();
			this->execute = exec;
			this->check = check;
			this->getCosts = getCosts;
		}
		/**/			IconCommand(const String&name, const FOnExecute&exec, const FCheck&check):Super(name)
		{
			LoadIcon();
			this->execute = exec;
			this->check = check;
		}
	};
	typedef std::shared_ptr<IconCommand> PIconCommand;

	template <typename Base>
		class CustomIcon : public Base
		{
			typedef	Base	Super;
			String		imageName;
		public:
			/**/		CustomIcon(const String&name, const String&imageName):Super(name),imageName(imageName)	{LoadIcon();}
			/**/		CustomIcon() {}
			virtual void	LoadIcon()	override {Super::LoadIcon(imageName);}
		};


	class CustomIconCommand : public CustomIcon<IconCommand>
	{
		typedef	CustomIcon<IconCommand>	Super;
	protected:
		/**/			CustomIconCommand(const String&name, const String&imageName):Super(name,imageName)	{}

	public:
		/**/			CustomIconCommand()	{}
		/**/			CustomIconCommand(const String&name, const String&imageName, const FOnExecute&exec, const FCheck&check, const FGetCosts&getCosts):Super(name,imageName)
						{
							this->execute = exec;
							this->check = check;
							this->getCosts = getCosts;
						}
		/**/			CustomIconCommand(const String&name, const String&imageName, const FOnExecute&exec, const FCheck&check):Super(name,imageName)
						{
							this->execute = exec;
							this->check = check;
						}
	};

	class CommandContainer
	{
	public:
		typedef std::pair<int2,PBaseCommand>	Member;
	protected:
		Buffer0<Member>				members;
		const CommandContainer		*parent = nullptr;	//!< Must not access members

	public:
		int2						baseCoords;

		PCommandGroup	AddChild(const String&name);
		PCommandGroup	AddChild(const int2&coords, bool relativeCoords, const String&name);
		PCommandGroup	AddChild(const String&name, const String&iconName);
		PCommandGroup	AddChild(const int2&coords, bool relativeCoords, const String&name, const String&iconName);

		void			VisitMembers(const FCommandVisitor&visitor, bool force)	const
		{
			foreach (members,m)
				m->second->VisitMembers(visitor,force,BaseCommand::coordOffset + baseCoords + m->first);
		}

		void			Refresh(bool controlEnabled)
		{
			foreach (members,m)
				m->second->Refresh(controlEnabled);
		}

		PBaseCommand	Add(BaseCommand*newCommand)	{ return Add(PBaseCommand(newCommand));}
		PBaseCommand	Add(const PBaseCommand&newCommand);
		PBaseCommand	Add(const int2&coords, bool relativeCoords, BaseCommand*newCommand)	{ return Add(coords, relativeCoords, PBaseCommand(newCommand));}
		PBaseCommand	Add(const int2&coords, bool relativeCoords, const PBaseCommand&newCommand);
		const Member&	GetLast() const {return members.last();}
		void			Truncate(count_t numMembers)	{members.Truncate(numMembers);}
		count_t			Count() const {return members.Count();}
		
		index_t			Find(const PBaseCommand&) const;
	};


	/**
	Button that represents a whole group of other buttons (including tabs)
	Only one branch of groups can be visible at any time.
	This version of Group must hold copies of one type of command. It cannot be used to store shared_ptr of commands!
	*/
	class CommandGroup : public CustomLabel<BaseCommand>, public CommandContainer	//for size/compatibility reasons
	{
		typedef CustomLabel<BaseCommand> Super;

		static Buffer0<CommandGroup*>		activeGroupPath;

		void	Collapse()
		{
			if (state & Flags::IsPermaActive)
			{
				state -= Flags::IsPermaActive;
				if (onCollapse)
					onCollapse();
			}
		}


		void	Expand()
		{
			bool trigger = !(state & Flags::IsPermaActive);
			state |= Flags::IsPermaActive;
			if (trigger && onExpand)
				onExpand();
		}


	public:
		std::function<void()>	onCollapse;
		std::function<void()>	onExpand;


		/**/			CommandGroup(const String&name) : Super(name)	{LoadIcon();}
		

		virtual void	Execute() override;
		virtual void	VisitMembers(const FCommandVisitor&visitor, bool force, const int2&parentCoords)	override
		{
			Super::VisitMembers(visitor,force,parentCoords);
			if (force || state & Flags::IsPermaActive)
			{
				CommandContainer::VisitMembers(visitor,force);
			}
		}

		virtual	void	Refresh(bool controlEnabled) override {/*CommandContainer::Refresh(controlEnabled);*/}
		virtual bool	IsGroup() const override {return true;}

		void			CollapseChildren()
		{
			if (!activeGroupPath.Contains(this))
				return;
			while (activeGroupPath.last() != this)
			{
				activeGroupPath.last()->Collapse();
				activeGroupPath.EraseLast();
			}
			Collapse();
		}

		static void		CollapseAll()
		{
			foreach (activeGroupPath,p)
				(*p)->Collapse();
			activeGroupPath.Clear();
		}
	};


	class ResearchCommand : public CustomIcon<CommandGroup>
	{
		typedef CustomIcon<CommandGroup>	Super;
	public:
		PResearchTopic	topic;

		/**/			ResearchCommand(const PResearchTopic&topic);

		virtual	void	Refresh(bool controlEnabled) override;
		virtual void	RasterizeImage()override;

		virtual void	CompileLabel(Label&outResult) override;
		virtual void	Execute() override;

	};
	typedef std::shared_ptr<ResearchCommand>	PResearchCommand;


	class EntityCommand : public CallbackCommand
	{
		typedef CallbackCommand	Super;
	protected:
		/**/				EntityCommand(const String&name):Super(name)	{}
	public:
		WWorldEntity		reference;

		/**/				EntityCommand()	{}
		/**/				EntityCommand(const String&name, const PWorldEntity&e,   FOnExecute f, FCheck c) : Super(name,f,c),reference(e) {}
		///**/				EntityCommand(const int2&coords, const String&name, const PWorldEntity&e,   FOnExecute f, FCheck c) : Super(name,f,c),reference(e)
		//{
		//	SetCoords(coords);
		//}

		virtual void		RasterizeImage()override
		{
			PWorldEntity e = reference.lock();
			if (e)
				UpdateTexture(e,false);
		}
		void				UpdateTexture(const PWorldEntity&v, bool isPrototype, const float4&backgroundColor = float4(0));
		static void			UpdateTexture(BaseCommand&target, const PWorldEntity&v, bool isPrototype, const float4&backgroundColor = float4(0));

	};

	class EntityConstructionCommand : public EntityCommand
	{
		typedef EntityCommand	Super;
	protected:
		/**/				EntityConstructionCommand(const String&name):Super(name)	{}
	public:
		Component::Turret*		turret;
		Component::VehicleBase*	base;

		unsigned			roundsLeft = 0xFFFFFFFF;
		unsigned			mineralsLeft = 0xFFFFFFFF;

		/**/				EntityConstructionCommand()	{}
		/**/				EntityConstructionCommand(const String&name, Component::VehicleBase*base, Component::Turret*turret, const PWorldEntity&e,   FOnExecute f, FCheck c) 
						: Super(name,e,f,c), base(base),turret(turret) {}
		///**/				EntityConstructionCommand(const int2&coords, const String&name, Component::VehicleBase*base, Component::Turret*turret, const PWorldEntity&e,   FOnExecute f, FCheck c)
		//				: Super(name,e,f,c), base(base),turret(turret)
		//{
		//	SetCoords(coords);
		//}

		virtual void		CompileLabel(Label&outResult) override;

		virtual void		RasterizeImage()override
		{
			PWorldEntity e = reference.lock();
			if (e)
				Super::UpdateTexture(e,true);
			else
				UpdateTexture(base,turret);
		}

		static void			UpdateTexture(BaseCommand&target, Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor = float4(0));
		void				UpdateTexture(Component::VehicleBase*base, Component::Turret*turret, const float4&backgroundColor = float4(0));
	};




	class DeployMobileCommand : public EntityCommand
	{
		typedef EntityCommand	Super;
		/**/				DeployMobileCommand(const String&name):Super(name)	{}
	public:
		typedef DeployMobileCommand	This;

		/**/				DeployMobileCommand() {}
		/**/				DeployMobileCommand(const PMobileEntity&v, FOnExecute f, FCheck c);

		virtual void		CompileLabel(Label&outResult) override;
	};

	//class StructureCommand : public EntityCommand
	//{
	//	typedef EntityCommand	Super;
	//protected:
	//	/**/				StructureCommand(const String&name):Super(name)	{}
	//public:
	//	typedef StructureCommand	This;

	//	/**/				StructureCommand(const String&name, const PStructure&structure,   FOnExecute f, FCheck c);

	//	virtual void		RasterizeImage() override	{PStructure s = GetReference(); if (s) UpdateTexture(s,true);}
	//	void				UpdateTexture(const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));
	//	static void			UpdateTexture(BaseCommand&target, const PStructure&structure, bool isPrototype, const float4&backgroundColor = float4(0));

	//	PStructure			GetReference() const;
	//};

	//class StructureConstructionCommand : public StructureCommand
	//{
	//	typedef StructureCommand	Super;
	//protected:
	//	/**/				StructureConstructionCommand(const String&name):Super(name)	{}
	//public:
	//	typedef StructureConstructionCommand	This;


	//	/**/				StructureConstructionCommand(const int2&coords,const String&name, const PStructure&structure,   FOnExecute f, FCheck c) : Super(name,structure,f,c) {SetCoords(coords);}
	//	/**/				StructureConstructionCommand(const String&name, const PStructure&structure,   FOnExecute f, FCheck c) : Super(name,structure,f,c) {}

	//	virtual void		CompileLabel(Label&outResult) override;

	//};

	typedef IconCommand		GlobalCommand;



	extern CommandContainer	globalCommands;
	extern PCommandGroup	viewCommands;



	
	namespace VehicleCmd
	{
		void	UndoOperation();
		void	AbortOperation();
		void	Automate();
		void	Sleep();

		bool	IsAutomated();

		BaseCommand::Flags AutomatedCheck(bool controlEnabled);
		BaseCommand::Flags SleepCheck(bool controlEnabled);

	}


	namespace CommonCmd
	{

		inline BaseCommand::Flags	CommonButtonCheck(bool controlEnabled)	{return controlEnabled ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;}
		void						DemolishSelected();

		void						ToggleMode(CursorMode mode);
	
		template <CursorMode M>
			inline void ToggleMode()
			{
				ToggleMode(M);
			}


		bool IsModeActive(CursorMode mode);
		bool IsModeActiveOrIdle(CursorMode mode);

		template <CursorMode M>
			inline bool IsModeActive()
			{
				return IsModeActive(M);
			}
		template <CursorMode M>
			inline bool IsModeActiveC(bool controlEnabled)
			{
				return controlEnabled && IsModeActive(M);
			}
		template <CursorMode M>
			inline bool IsModeActiveOrIdleC(bool controlEnabled)
			{
				return controlEnabled && IsModeActiveOrIdle(M);
			}

		template <CursorMode M>
			inline BaseCommand::Flags ModeCheck(bool controlsEnabled)
			{
				BaseCommand::Flags rs = BaseCommand::Flags::None;
				if (IsModeActiveOrIdleC<M>(controlsEnabled))
					rs |= BaseCommand::Flags::IsEnabled;
				if (CommonCmd::IsModeActive<M>())
					rs |= BaseCommand::Flags::IsPermaActive;
				return rs;
			}
		inline BaseCommand::Flags IdleCheck(bool controlsEnabled)
		{
			return controlsEnabled && activeCursorMode == CursorMode::Idle ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
		}



	}


	namespace GlobalCmd
	{
		void	ToggleSeeThrough();
		void	SeeThroughUp();
		void	SeeThroughDown();
		bool	SeeThroughIsActive();
		bool	SeeThroughIsActive2(bool);
		void	ToggleObjectView();
		bool	ObjectsAreHidden();

	}

}


#endif

#endif
