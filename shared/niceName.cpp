#include <global_root.h>
#include "niceName.h"
#include <io/string_file.h>
#include <container/buffer.h>

namespace NiceName
{

	static Buffer<String>	adjectives,nouns;
	static UINT64	totalCombinations = 0;

	#ifdef CLIENT
		#define PRE_PATH
	#else
		#define PRE_PATH	"../client/"
	#endif


	static String& MakeNice(String&in)
	{
		in.convertToLowerCase();
		in.set(0,Template::toupper(in.get(0)));
		return in;
	}
	static String& MakeNiceAdjective(String&in)
	{
		//for now, same
		in.convertToLowerCase();
		in.set(0,Template::toupper(in.get(0)));
		return in;
	}

	void	Load()
	{
		StringFile file;
		String line;

		ASSERT__(file.open(PRE_PATH"resources/nice/nouns.txt"));

		while (file >> line)
			nouns << MakeNice(line);

		ASSERT__(file.open(PRE_PATH"resources/nice/adjectives.txt"));

		while (file >> line)
			adjectives << MakeNiceAdjective(line);

		totalCombinations = adjectives.Count() * nouns.Count();
	}

	String	Get(UINT64 code)
	{
		code %= totalCombinations;
		return adjectives[code / nouns.Count()]+" "+nouns[code % nouns.Count()];
	}


}
