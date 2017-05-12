#include "compilerBase.h"

namespace VisualTerrain
{
	
	struct TerrainVertex
	{
		float4		vertex;
		sbyte4		normal_structure;
		float4		color;

		/**/		TerrainVertex()	{};
		/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const TLitColor&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f,structure*127.f) {color.CompileFinalColor(this->color);}
		/**/		TerrainVertex(const float4&vtx, float h, const float3&n, const float3&color, float structure) : vertex(vtx.xyz*h, vtx.w), normal_structure(n*127.f, structure*127.f), color(color,1.f){}
	};

	struct OverlayVertex
	{
		float3		vertex;
		float		opacity;	//overlay is always black

		/**/		OverlayVertex()	{};
		/**/		OverlayVertex(const float4&vtx, float h, float opacity) : vertex(vtx.xyz*h), opacity(opacity){}
	};




	struct TTerrainHull : public THull<TerrainVertex>
	{
		ShadedInstructionBlock32	instruction[3];
		Engine::DX11::InstructionBlock32	shadowInstruction;

		void						Destroy()
		{
			THull<TerrainVertex>::Clear();
			instruction[0].Clear();
			instruction[1].Clear();
			instruction[2].Clear();
			shadowInstruction.Clear();
		}
	};

	struct TOverlayHull : public THull<OverlayVertex>
	{
		Engine::DX11::InstructionBlock32	instruction,shadowInstruction;

		void						Destroy()
		{
			THull<OverlayVertex>::Clear();
			shadowInstruction = instruction = Engine::DX11::InstructionBlock32();
		}
	};

	void		CompileSolidTerrain(const Array<Grid::CellStack>&stacks, THull<TerrainVertex>&solid, THull<OverlayVertex>&cover, index_t offset, count_t count, Grid::FClipHeight clipHeight);

}
