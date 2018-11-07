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

		typedef std::function<void(const PBase&cmd)>				FVisitor;
		typedef std::function<bool(const PBase&cmd)>				FFilter;
		
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


			virtual const Container*	ToContainer() const {return nullptr;}
			virtual Container*	ToContainer() {return nullptr;}
//			virtual void	VisitMembers(const FVisitor&visitor, bool force) {visitor(shared_from_this());}



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
			typedef Ctr::Buffer0<PBase>	Row;
		protected:
			Ctr::Buffer0<Row>		rows;
			const Container			*parent = nullptr;	//!< Must not access members
		public:
			/**
			Visits all locally known member commands
			*/
			void			VisitLocalMembers(const FVisitor&visitor)	const;
			/**
			Replaces the first filter match with the new command
			*/
			bool			ReplaceLocalMember(const FFilter&filter, const PBase&newCommand);
			void			ReplaceLocalMember(const FFilter&filter, Base*newCommand)	{ASSERT__(ReplaceLocalMember(filter,PBase(newCommand)));}

			void			Refresh(bool controlEnabled)
			{
				VisitLocalMembers([controlEnabled](const PBase&cmd)
				{
					cmd->Refresh(controlEnabled);
				});
			}

			bool			Contains(const PBase&) const;

			PBase			Add(Base*newCommand)	{ return Add(PBase(newCommand));}
			PBase			Add(const PBase&newCommand);
			PBase			Add(index_t row, Base*newCommand)	{ return Add(row,PBase(newCommand));}
			PBase			Add(index_t row, const PBase&newCommand);
			void			AddSpace();
			const PBase&	GetLast() const {return rows.Last().Last();}
			void			TruncateRow(index_t row, count_t numMembers)	{if (row < rows.Count()) rows[row].Truncate(numMembers);}
			count_t			CountCommands() const;
			count_t			CountRows() const {return rows.Count();}
			const Row&		GetRow(index_t row) const {return rows[row];}
			void			Clear()
			{
				rows.clear();
			}
			/**
			Appends a single new row.
			Note that the local container starts with zero rows, but a row is appended automatically
			when the first command is added.
			If AppendRow() is called on an empty container, nothing will immediately change as that row
			would have been added anyway.
			*/
			Container&		AppendRow()
			{
				rows.Append();
				return *this;
			}

			Container&		Append(const Container&other)	{return Append(&other);}
			/**
			Includes the contents of another container into the local container.
			Does nothing if this is null
			@param other Container to include. May be empty or point to this, causing the method to do nothing
			*/
			Container&		Append(const Container*other);
		};
	}

	typedef Command::Container	CommandContainer,CC;
	typedef Command::Base::Flags CommandFlags;

}
