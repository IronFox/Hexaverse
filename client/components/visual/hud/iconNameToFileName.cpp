#include "iconNameToFileName.h"

namespace Icons
{

	static bool NotValidFSChar(char c)
	{
		if (c >= 'a' && c <= 'z')
			return false;
		if (c >= 'A' && c <= 'Z')
			return false;
		if (c >= '0' && c <= '9')
			return false;
		return c != '_';
	}


	DeltaWorks::String	NameToFileName(const DeltaWorks::String&name)
	{
		DeltaWorks::String name2 = name;
		name2.FindAndReplace(NotValidFSChar,'-');
		while (name2.EndsWith('-'))
			name2.EraseRight(1);
		while (name2.BeginsWith('-'))
			name2.EraseLeft(1);
		return name2;
	}
}
