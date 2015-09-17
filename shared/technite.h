#ifndef techniteH
#define techniteH

#include "config.h"

#if TECHNITES

#include "sphereGrid.h"
#include "typeDecl.h"
#include "uniqueID.h"

namespace Technites
{
	DECLARE_POINTER_CLASS(Faction);




	class Technite
	{
	public:
		Grid::HCellIdI	location;

		static const BYTE	MoveEnergyCost = 1,
							EatEnergyCost = 1,
							SplitEnergyCost = 5,
							SplitMatterCost = 5,
							EnergyPackageSize = 5,
							MatterPackageSize = 5;

		struct Resources
		{
			BYTE		energy = 0,
						matter = 0;


			/**/		Resources() {}
			/**/		Resources(BYTE energy, BYTE matter) :energy(energy), matter(matter) {}
		}				resources;


		static const Resources SplitCost, MoveCost, EatCost, EnergyPackage, MatterPackage;


		enum class Task : BYTE
		{
			None,
			EatMoveTo,
			GrowTo,
			TransferEnergyTo,
			TransferMatterTo,
		};

		struct TTarget
		{
			unsigned	neighborIndex;
			int			heightDelta;


			BYTE		Compress()	const
			{
				BYTE base = (BYTE)neighborIndex;
				base |= (BYTE)((heightDelta+1) << 3);
				return base;
			}

			void		Decompress(BYTE compressed)
			{
				neighborIndex = (compressed&0x7);
				heightDelta = ((int)(compressed >> 3)) -1;
			}
		};


		bool			DecodeAbsoluteTarget(Grid::HCellIdI&target) const;


		Task			nextTask = Task::None;
		BYTE			compressedTarget;

	};



	class Faction
	{
	public:

		Grid::WorldData	knownWorld;

		Buffer<Technite>	cells;
		GenericHashTable<Grid::HCellIdI,index_t>	cellMap;
		BYTE			gridID;				//!< Identifier in the grid. 255/0xFF is invalid. Up to 254 different cell factions on one planet.
		TUniqueID		globalID;


		enum class ExecResult
		{
			NothingToDo,
			BadCommand,
			CannotEatThis,
			CannotStandThere,
			TechniteLacksResources,
			NoTechniteAtDestination,
			MoreWorkNeeded,
			Success
		};


		void					Process();


	private:
		ExecResult				GrowTo(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell);
		ExecResult				MoveTo(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell);
		ExecResult				Eat(Technite&technite, const Grid::HCellIdI&target, const Grid::CellStack::Cell&targetCell);


		Technite::Resources		EatEntitiesIn(const Grid::HCellIdI&target);
		Technite::Resources		EatTechnite(const Grid::HCellIdI&target);
		void					SpawnNewTechnite(const Grid::HCellIdI&target);
		void					MoveTechnite(Technite&technite, const Grid::HCellIdI&target);
		bool					ReduceResources(Technite&technite, const Technite::Resources&by);
		void					IncreaseResources(Technite&technite, const Technite::Resources&by);
		Grid::Content			DegradeMatter(const Grid::HCellIdI&target, Technite::Resources&outYield);
		Technite*				GetOwnTechnite(const Grid::HCellIdI&location);

		PFaction				GetFactionOf(BYTE gridFactionID);
		ExecResult				ExecuteTask(Technite&cell);
	};



	void		Create();
	void		Destroy();





}





#endif










#endif
