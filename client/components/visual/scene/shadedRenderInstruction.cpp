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

void					ShadedInstruction::LoadQualitiesFromFile(const PathString&filename, const String&vsMain, const String&gsMain, const String&psMain, const Engine::DX11::Shader::Binding*b/*=NULL*/, count_t numBindings/*=0*/)
{
	namespace DX11 = Engine::DX11;
	typedef DX11::Shader	Shader;

	Shader::macroDefinitions << Shader::Macro("SHADOW_FILTER_QUALITY",6);
	quality[Quality::Filtered6x6].shader.LoadFromFile(filename,vsMain,gsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 3;
	quality[Quality::Filtered3x3].shader.LoadFromFile(filename,vsMain,gsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 1;
	quality[Quality::Unfiltered].shader.LoadFromFile(filename,vsMain,gsMain,psMain,b,numBindings);
	Shader::macroDefinitions.Last().definition = 0;
	quality[Quality::Off].shader.LoadFromFile(filename,vsMain,gsMain,psMain,b,numBindings);
	Shader::macroDefinitions.EraseLast();
}

void					ShadedInstruction::UpdateLayout()
{
	for (index_t i = 0; i < Quality::N; i++)
	{
		if (instanceBinding.IsNotEmpty())
		{
			DX11::VertexDescription combined;
			combined.AddAsNewStream(binding,D3D11_INPUT_PER_VERTEX_DATA);
			combined.AddAsNewStream(instanceBinding,D3D11_INPUT_PER_INSTANCE_DATA);
			quality[i].vertexLayout = quality[i].shader.BuildLayout(combined);
		}
		else
			quality[i].vertexLayout = quality[i].shader.BuildLayout(binding);
	}

	stride = binding.GetStride();
	instanceStride = instanceBinding.GetStride();
}


