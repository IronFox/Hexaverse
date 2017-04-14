#pragma once

#include <functional>
#include <math/vclasses.h>
#include "types.h"
#include <container/buffer.h>
#include "description.h"

namespace Objects
{
	class BaseCommand;
	class CommandContainer;

	typedef std::function<void(const int2&,const PBaseCommand&cmd)>	FCommandVisitor;




	/**
	Base class for all visual commands.
	Visual commands are currently rendered as hexagonal icons arranged in the four corners of the screen.
	*/
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


		virtual bool	IsGroup() const {return false;}
		virtual void	VisitMembers(const FCommandVisitor&visitor, bool force, const int2&parentGridCoords) {visitor(parentGridCoords,shared_from_this());}
		void			VisitMembers(const FCommandVisitor&visitor, bool force) {VisitMembers(visitor,force,coordOffset);}

		virtual void	SignalBaseChange(CommandContainer*newParent, const int2&newBase) {};


		typedef Objects::Description	Label;




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

}
