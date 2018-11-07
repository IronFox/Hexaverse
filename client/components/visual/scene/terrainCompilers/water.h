#pragma once
#ifdef HAS_WINDOW
#include "compilerBase.h"

namespace VisualTerrain
{

	struct WaterVertex
	{
		float4		vertex;
		float3		normal;
		float		light;	//water derives color from shader variable

		/**/		WaterVertex()	{};
		/**/		WaterVertex(const float4&vtx, float h, const float3&n,float light, int emp) : vertex(vtx.xyz*h, vtx.w), normal(n),light((float(emp) + 0.05f + light*0.9f) * 0.25f ){}
	};

	struct TWaterHull : public THull<WaterVertex>
	{
		ShadedInstructionBlock32	instruction[3];

		void						Destroy()
		{
			THull<WaterVertex>::Clear();
			instruction[0].Clear();
			instruction[1].Clear();
			instruction[2].Clear();
		}
	};

	
	void		CompileWater(const Array<Grid::CellStack>&stacks, THull<WaterVertex>&cover, index_t offset, count_t count, Grid::FClipHeight clipHeight);
}

#endif
