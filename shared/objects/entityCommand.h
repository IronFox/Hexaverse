#pragma once

#include <functional>
#include <math/vclasses.h>
#include "types.h"
#include <container/buffer.h>
#include "description.h"

namespace Objects
{

	namespace Command
	{

		class Base;
		class Container;

		typedef std::function<void(const int2&,const PBase&cmd)>	FVisitor;




		/**
		Base class for all visual commands.
		Visual commands are currently rendered as hexagonal icons arranged in the four corners of the screen.
		*/
		class Base : public std::enable_shared_from_this<Base>
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


			/**/			Base(const String&name):name(name) {}
			///**/			Command::Base(const int2&coords, const String&name):name(name) {SetCoords(coords);}
			/**/			Base()	{}

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
			virtual void	VisitMembers(const FVisitor&visitor, bool force, const int2&parentGridCoords) {visitor(parentGridCoords,shared_from_this());}
			void			VisitMembers(const FVisitor&visitor, bool force) {VisitMembers(visitor,force,coordOffset);}

			virtual void	SignalBaseChange(Container*newParent, const int2&newBase) {};


			typedef Objects::Description	Label;




		};

		inline Base::Flags operator|(Base::Flags a, Base::Flags b)
		{
			using T = std::underlying_type_t <Base::Flags>;
			return (Base::Flags)(((T)a)|((T)b));
		}
	
		inline void operator|=(Base::Flags&a, Base::Flags b)
		{
			using T = std::underlying_type_t <Base::Flags>;
			a = (Base::Flags)(((T)a)|((T)b));
		}

		inline bool operator&(Base::Flags a, Base::Flags b)
		{
			using T = std::underlying_type_t <Base::Flags>;
			return (((T)a)&((T)b)) != 0;
		}

		inline void operator-=(Base::Flags&a, Base::Flags b)
		{
			using T = std::underlying_type_t <Base::Flags>;
			a = (Base::Flags)(((T)a)&(~((T)b)));
		}


		typedef std::shared_ptr<Base>	PBase;



		class Container
		{
		public:
			typedef std::pair<int2,PBase>	Member;
		protected:
			Ctr::Buffer0<Member>		members;
			const Container				*parent = nullptr;	//!< Must not access members

		public:
			int2						baseCoords;

			//PCommandGroup	AddChild(const String&name);
			//PCommandGroup	AddChild(const int2&coords, bool relativeCoords, const String&name);
			//PCommandGroup	AddChild(const String&name, const String&iconName);
			//PCommandGroup	AddChild(const int2&coords, bool relativeCoords, const String&name, const String&iconName);

			void			VisitMembers(const FVisitor&visitor, bool force)	const
			{
				foreach (members,m)
					if (m->second)
						m->second->VisitMembers(visitor,force,Base::coordOffset + baseCoords + m->first);
			}

			void			Refresh(bool controlEnabled)
			{
				foreach (members,m)
					if (m->second)
						m->second->Refresh(controlEnabled);
			}

			PBase			Add(Base*newCommand)	{ return Add(PBase(newCommand));}
			PBase			Add(const PBase&newCommand);
			void			AddSpace();
			PBase			Add(const int2&coords, bool relativeCoords, Base*newCommand)	{ return Add(coords, relativeCoords, PBase(newCommand));}
			PBase			Add(const int2&coords, bool relativeCoords, const PBase&newCommand);
			const Member&	GetLast() const {return members.last();}
			void			Truncate(count_t numMembers)	{members.Truncate(numMembers);}
			count_t			Count() const {return members.Count();}
		
			index_t			Find(const PBase&) const;
		};
	}

	typedef Command::FVisitor	FCommandVisitor;
	typedef Command::Container	CommandContainer;
	typedef Command::Base::Flags CommandFlags;

}
