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
	semi-evenly distributed atop the point of origin.

	Plant geometry is expected to provide vertex configuration:
	position:3, normal:3, color:3, wind sensitivity:1 = 10 floats per vertex
	*/


	struct TPlantInstance
	{
		M::TVec3<>	position,	//absolute
					up,		//normalized
					dir;	//horizontal x direction, normalized
		float		scale, 
					tone, 
					light;	// >0


		void		SetParameters( const M::TVec3<>&position, const M::TVec3<>&up,const M::TVec3<>&dir, float scale, float tone, float light )
		{
			this->position = position;
			this->up = up;
			this->dir = dir;
			this->scale = scale;
			this->tone = tone;
			this->light = light;
		}
	};

	static_assert(sizeof(TPlantInstance)==48,"Bad struct alignment");	//this data is uploaded to the gpu so must be compact

	CONSTRUCT_ENUMERATION2(PlantType,
			Tree,
			Crystal
			);

	struct TExtPlantInstance : public TPlantInstance
	{
		typedef TPlantInstance	Super;
		PlantType		type;	//!< Plant type

		void		Set( PlantType t, const M::TVec3<>&position, const M::TVec3<>&up,const M::TVec3<>&dir, float scale, float tone, float light )
		{
			type = t;
			Super::SetParameters(position,up,dir,scale,tone,light);
		}
	};





	
	
	template <typename TPlant>
		struct TPlantHull : public TPointSet<TPlant>
		{
			typedef TPointSet<TPlant>			Super;
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
	void		CompilePlants(const Array<Grid::CellStack>&stacks, FixedArray<TPlantHull<TPlantInstance>, PlantType::N>&outPlants, index_t offset, count_t count, Grid::FClipHeight clipHeight);


	namespace Instruction
	{
		extern ShadedInstruction					plantBase;
		extern Engine::DX11::InstructionBlock32I	plantShadowBase;
		extern FixedArray<Engine::DX11::InstructionBlock32I::Prototype,PlantType::N>	plantPrototypes;
	}
	void	CreatePlants();

}



#endif
