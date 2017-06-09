#include <global_root.h>
#include "niceName.h"
#include <io/string_file.h>
#include <container/buffer.h>
#include <io/file_system.h>

namespace NiceName
{

	static Buffer<String>	adjectives,nouns;
	static UINT64	totalCombinations = 0;

	#ifdef CLIENT
		#define PRE_PATH
	#else
		#define PRE_PATH	L"../client/"
	#endif


	static String& MakeNice(String&in)
	{
		in.convertToLowerCase();
		in.set(0,CharFunctions::toupper(in.get(0)));
		return in;
	}
	static String& MakeNiceAdjective(String&in)
	{
		//for now, same
		in.convertToLowerCase();
		in.set(0,CharFunctions::toupper(in.get(0)));
		return in;
	}

	void	Load()
	{
		StringFile file;
		String line;

		ASSERT1__(file.Open(PRE_PATH L"resources/nice/nouns.txt"), FileSystem::GetWorkingDirectory());

		while (file >> line)
			nouns << line;

		ASSERT__(file.Open(PRE_PATH L"resources/nice/adjectives.txt"));

		while (file >> line)
			adjectives << MakeNiceAdjective(line);

		totalCombinations = adjectives.Count() * nouns.Count();
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

		code %= totalCombinations;
		return adjectives[code / nouns.Count()]+" "+nouns[code % nouns.Count()];
	}


}
