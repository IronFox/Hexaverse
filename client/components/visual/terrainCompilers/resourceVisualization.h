#pragma once
#ifdef HAS_WINDOW

#include "compilerBase.h"
namespace VisualTerrain
{
	


	struct ResourceVertex //! Temporarily the same layout as TerrainVertex
	{
		float4		vertex;
		float4		normal;
		float4		color;

		/**/		ResourceVertex()	{};
		/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n,structure), color(color){}
		/**/		ResourceVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal(n, structure), color(color,1.f){}
		/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float4&color, float structure) : vertex(vtx*h, 1.f), normal(n,structure), color(color){}
		/**/		ResourceVertex(const float3&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx*h, 1.f), normal(n, structure), color(color,1.f){}
	};


	


	struct TResourceHull : public THull<ResourceVertex>
	{
		Engine::DX11::InstructionBlock32	instruction,shadowInstruction;	// might cast shadow some day

		void						Destroy()
		{
			THull<ResourceVertex>::Clear();
			shadowInstruction = instruction = Engine::DX11::InstructionBlock32();
		}
	};





	void CompileResources(THull<ResourceVertex>&cover, Grid::ResourceDiscovery&visibility, index_t offset, count_t count, Grid::FClipHeight clipHeight, const Grid::Resource*resources, count_t numResources);

}


#endif
