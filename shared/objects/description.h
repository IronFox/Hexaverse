#pragma once

#include <global_string.h>
#include <container/buffer.h>

using namespace DeltaWorks;

namespace Objects
{
	class WrappedDescription;

	/**
	General purpose description container.
	It distinguishes between brief and extended lines, which can be filtered as needed.
	*/
	class Description
	{
	public:
		typedef std::pair<String, bool>		Line;
		typedef Ctr::Buffer0<Line>	Storage;
	private:
		Storage			rawLines;
		friend class WrappedDescription;
	public:
		typedef Storage::const_iterator	const_iterator;

		bool				isBrief=true,briefLocked=false;

		void				Clear()
		{
			rawLines.Clear();
			briefLocked = false;
		}

		void				swap(Description&other) {rawLines.swap(other.rawLines); swp(isBrief,other.isBrief); swp(briefLocked,other.briefLocked); }
		friend void			swap(Description&a, Description&b)	{a.swap(b);}

		bool				IsEmpty() const {return rawLines.IsEmpty();}
		bool				IsNotEmpty() const {return rawLines.IsNotEmpty();}

		Line&				Insert(index_t before) {return rawLines.Insert(before);}
		Line&				GetLine(index_t at) {return rawLines[at];}
		const Line&			GetLine(index_t at) const {return rawLines[at];}
		void				SetBrief(index_t line, bool isBrief=true)	{rawLines[line].second = isBrief;}

		const_iterator		begin() const {return rawLines.begin();}
		const_iterator		end() const {return rawLines.end();}

		void				BeginBrief();
		void				MarkBriefEnd(bool permanently = false);
		String&				GetLastLine();
		Description&		operator<<(const String&line);
	};


	/**
	Final general purpose description, created by applying line-wrapping to all raw lines of a Description instance
	*/
	class WrappedDescription
	{
	public:
		/**
		Finished (wrapped) lines, ready for visualization.
		The boolean is true for brief lines, false for extended lines
		*/
		Ctr::Buffer0<std::pair<String, bool> >		wrappedLines;

		/**/				WrappedDescription()	{}
		/**/				WrappedDescription(const Description&desc);
		void				operator=(const Description&desc);
		void				swap(WrappedDescription&other) {wrappedLines.swap(other.wrappedLines);}
		friend void			swap(WrappedDescription&a, WrappedDescription&b)	{a.swap(b);}

		/**
		Counts all brief lines
		*/
		count_t				CountBriefLines() const;

		bool				IsEmpty() const {return wrappedLines.IsEmpty();}
		bool				IsNotEmpty() const {return wrappedLines.IsNotEmpty();}
	};

}
