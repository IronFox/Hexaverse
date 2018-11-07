// BuildCounter.cpp : Defines the entry point for the console application.
//

//#include "stdafx.h"


#include <global_root.h>
#include <global_string.h>
#include <string/string_converter.h>
#include <io/string_file.h>

using namespace DeltaWorks;



int main(int argc, const char* argv[])
{
	if (argc <= 1)
	{
		std::cerr << "This program requires a filepath to process"<<std::endl;
		return -1;
	}

	StringFile file;
	try
	{
		file.Open(argv[1]);
	}
	catch (const std::exception&ex)
	{
		std::cerr << "Unable to open '"<<argv[1]<<"': "<<ex.what() << std::endl;
		return -1;
	}
	String line;
	if (!(file >> line))
	{
		std::cerr << "Unable to read '"<<argv[1]<<"'" << std::endl;
		return -1;
	}

	file.Close();

	line.FindAndReplace('\t',' ');
	while (line.Contains("  "))
		line.FindAndReplace("  "," ");

	Array<String> parts;
	explode(' ',line,parts);
	
	if (parts.Count() != 3)
	{
		std::cerr << "Expected three words in first line of '"<<argv[1]<<"'. Found "<< parts.Count() << std::endl;
		return -1;
	}
	if (parts.First() != "#define" || parts[1] != "BUILD_COUNTER")
	{
		std::cerr << "Expected first line of '"<<argv[1]<<"' to be like '#define BUILD_COUNTER [number]'." << std::endl;
		return -1;
	}

	count_t current;
	if (!convert(parts.Last().c_str(),current))
	{
		std::cerr << "Cannot parse argument. Expected first line of '"<<argv[1]<<"' to be like '#define BUILD_COUNTER [number]'." << std::endl;
		return -1;
	}

	current++;
	try
	{
		file.Create(argv[1]);
		file << "#define BUILD_COUNTER "<<current<<nl;
		file << "#define BUILD_COUNTER_STR \""<<current<<'"' << nl;
		file.Close();
	}
	catch (const std::exception&ex)
	{
		std::cerr << "Unable to write '"<<argv[1]<<"': "<<ex.what() << std::endl;
	}


	return 0;
}

