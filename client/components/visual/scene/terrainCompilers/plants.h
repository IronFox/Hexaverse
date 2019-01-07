#pragma once
#ifdef HAS_WINDOW
#include "compilerBase.h"

namespace VisualTerrain
{
	/*
	Plants are instantiated, three per high resolution terrain grid cell,
	if the cell is clear and the type underneath specified such.

	All planet types share the same instantiation and animation mechanic, diverging only in the
	geometry presented.

	Plant geometries are expected to be normalized in height [0,1], and width set proportionally,
	semi-evenly distributed around the point of origin.

	Plant geometry is expected to provide vertex configuration:
	position:3, normal:3, color:3, wind sensitivity:1 = 10 floats per vertex
	*/


	struct TPlantInstance
	{
		M::TVec3<>	position,	//absolute
					up,		//normalized
					dir;	//horizontal x direction, normalized
		float		scale;	// >0

		void		Set(const M::TVec3<>&position, const M::TVec3<>&up,const M::TVec3<>&dir, float scale)
		{
			this->position = position;
			this->up = up;
			this->dir = dir;
			this->scale = scale;
		}
	};



	struct TLeafBlob
	{
		M::TVec3<>	center;
		float radius;
		float windSensitivity;

		void	Set(const M::TMatrix3<>&sys,const M::TVec3<>&dir, float z, float r, float wind)
		{
			Vec::mad(dir,sys.z,z,center);
			radius = r;
			windSensitivity = wind;
		}
	};

	struct TTreeTrunk
	{
		M::TVec3<>	center;
		float	height,
				radius;

		void	Set(const M::TMatrix3<>&sys,const M::TVec3<>&dir, float z, float r, float h)
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
			ShadedInstructionBlock32I			instruction;
			Engine::DX11::InstructionBlock32I	shadowInstruction;

			void						Destroy()
			{
				Super::Clear();
				instruction.Clear();
				shadowInstruction.Clear();
			}
		};

	//void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TLeafBlob>&leafBlobs,TPlantHull<TTreeTrunk>&treeTrunks, index_t offset, count_t count, Grid::FClipHeight clipHeight);
	void		CompilePlants(const Array<Grid::CellStack>&stacks, TPlantHull<TPlantInstance>&treeInstances, index_t offset, count_t count, Grid::FClipHeight clipHeight);


	namespace Instruction
	{
		extern ShadedInstructionBlock32I			plantBase;
		extern Engine::DX11::InstructionBlock32I	plantShadowBase;
		extern Engine::DX11::InstructionBlock32I::Prototype	tree,dendrite;
	}
	void	CreatePlants();

}



#endif
