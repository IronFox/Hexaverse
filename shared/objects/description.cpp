#include <global_root.h>
#include "description.h"


namespace Objects
{
	void				Description::BeginBrief()
	{
		if (rawLines.IsNotEmpty() && rawLines.Last().first.Trim().IsNotEmpty() && !rawLines.Last().second)
			rawLines << std::make_pair("",false);
		isBrief = !briefLocked;
	}

	void				Description::MarkBriefEnd(bool permanently /*= false*/)
	{
		if (rawLines.IsNotEmpty() && rawLines.Last().first.Trim().IsNotEmpty() && rawLines.Last().second)
			rawLines << std::make_pair("",false);
		isBrief = false;
		if (permanently)
			briefLocked = true;
	}

	String&				Description::GetLastLine()
	{
		return rawLines.Last().first;
	}

	Description&		Description::operator<<(const String&line)
	{
		if (isBrief && (line.IsEmpty() || line.FirstChar() != ' '))
			rawLines << std::make_pair("  "+line,isBrief);
		else
			rawLines << std::make_pair(line,isBrief);
		//WrapAppendText(line);
		//lines << line;
		return *this;
	}

	count_t				WrappedDescription::CountBriefLines() const
	{
		count_t rs = 0;
		foreach (wrappedLines,l)
			if (l->second)
				rs++;
		return rs;
	}

	WrappedDescription::WrappedDescription(const Description&desc)
	{
		operator=(desc);
	}

	void WrappedDescription::operator=(const Description&desc)
	{
		wrappedLines.Clear();
		foreach (desc.rawLines,l)
		{
			if (l->first.length() <= 60)
			{
				wrappedLines << *l;
				continue;
			}
			Ctr::Array<String>	split;
			wrap(l->first,60,split);
			foreach (split,s)
			{
				wrappedLines.Append().first.adoptData(*s);
				wrappedLines.Last().second = l->second;
			}
		}
	}

}

