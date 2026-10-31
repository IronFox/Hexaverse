#include <global_root.h>
#include "niceName.h"
#include <io/string_file.h>
#include <container/vectorList.h>
#include <io/file_system.h>

namespace NiceName
{

	static Vector<String>	nouns;

	#ifdef CLIENT
		#define PRE_PATH
	#else
		#define PRE_PATH	L"../client/"
	#endif


	static String& MakeNice(String&in)
	{
		in.ConvertToLowerCase();
		in.Set(0,CharFunctions::toupper(in.GetChar(0)));
		return in;
	}
	static String& MakeNiceAdjective(String&in)
	{
		//for now, same
		in.ConvertToLowerCase();
		in.Set(0,CharFunctions::toupper(in.GetChar(0)));
		return in;
	}

	void	Load()
	{
		StringFile file;
		String line;

		file.Open(PRE_PATH L"resources/nice/nouns.txt"), FileSystem::GetWorkingDirectory();

		while (file >> line)
			nouns << line;

	}

	String	Get(UINT64 code)
	{
		String result = nouns[((UINT32)code) % nouns.Count()];
		code >>= 32;
		BYTE ch = (code&0xff)%26;
		result += ' ';
		result += 'A' + ch;
		result += String((code>>8)%100);
		return result;
	}


}
