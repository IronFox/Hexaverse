#pragma once
#ifdef HAS_WINDOW
#include "compilerBase.h"

namespace VisualTerrain
{
	struct TLeafBlob
	{
		TVec3<>	center;
		float radius;
		float windSensitivity;

		void	Set(const TMatrix3<>&sys,const TVec3<>&dir, float z, float r, float wind)
		{
			Vec::mad(dir,sys.z,z,center);
			radius = r;
			windSensitivity = wind;
		}
	};

	struct TTreeTrunk
	{
		TVec3<>	center;
		float	height,
				radius;

		void	Set(const TMatrix3<>&sys,const TVec3<>&dir, float z, float r, float h)
		{
			Vec::mad(dir,sys.z,z,center);
			radius = r;
			height = h;
		}
	};

	
	template <typename TPlant>
		struct TPlantHull : public TPointSet<TPlant>
		{
			typedef TPointSet<TPlant>	Super;
			Engine::DX11::BaseInstructionBlock	shadowInstruction;
			ShadedVertexInstructionBlock	instruction;

			void						Destroy()
			{
				Super::Clear();
				instruction.Clear();
				shadowInstruction.Clear();
			}
		};

	void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks, index_t offset, count_t count, Grid::FClipHeight clipHeight);


	namespace Instruction
	{
		extern Engine::DX11::BaseInstructionBlock	leafBlobShadowBase,treeTrunkShadowBase;
		extern ShadedInstruction					leafBlobBase, treeTrunkBase;
	}

}



#endif
