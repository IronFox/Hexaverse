#include <global_root.h>
#include "shadedRenderInstruction.h"


/*static*/ ShadedInstruction::Quality			ShadedInstruction::chosenQuality;

void					ShadedInstruction::LoadQualitiesFromFile(const PathString&filename, const String&vsMain, const String&psMain, const Engine::DX11::Shader::Binding*b/*=NULL*/, count_t numBindings/*=0*/)
{
	namespace DX11 = Engine::DX11;
	typedef DX11::Shader	Shader;

	Shader::macroDefinitions << Shader::Macro("SHADOW_FILTER_QUALITY",6);
	quality[Quality::Filtered6x6].shader.LoadFromFile(filename,vsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 3;
	quality[Quality::Filtered3x3].shader.LoadFromFile(filename,vsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 1;
	quality[Quality::Unfiltered].shader.LoadFromFile(filename,vsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 0;
	quality[Quality::Off].shader.LoadFromFile(filename,vsMain,psMain,b,numBindings);
	Shader::macroDefinitions.EraseLast();
}

void					ShadedInstruction::SetTopology(D3D_PRIMITIVE_TOPOLOGY t)
{
	for (index_t i = 0; i < Quality::N; i++)
	{
		quality[i].topology = t;
	}
}

void					ShadedInstruction::UpdateLayout()
{
	for (index_t i = 0; i < Quality::N; i++)
	{
		quality[i].binding = binding;
		quality[i].UpdateLayout();
	}
}


