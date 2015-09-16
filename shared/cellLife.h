#ifndef cellLifeH
#define cellLifeH

#include "config.h"

#if CELL_LIFE

#include "sphereGrid.h"
#include "typeDecl.h"
#include "uniqueID.h"

namespace CellLife
{
	DECLARE_POINTER_CLASS(CellFaction);




	class LiveCell
	{
	public:
		Grid::HCellIdI	location;

		struct Attributes
		{
			BYTE		size = 1,
						energy = 0,
						food = 0;
		}				attributes;


		enum class Task : BYTE
		{
			None,
			EatMoveTo,
			GrowTo,
			TransferEnergyTo,
			TransferFoodTo,
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

		bool			WillMoveToTarget() const {return nextTask == Task::EatMoveTo;}
		bool			TargetMustBeClear() const {return nextTask == Task::GrowTo;}
		bool			TargetMustBeMyLiveCell() const {return nextTask == Task::TransferEnergyTo || nextTask == Task::TransferFoodTo;}
		bool			TargetMustNotBeMyLiveCell() const {return nextTask == Task::EatMoveTo;}
	};



	class CellFaction
	{
	public:

		Grid::WorldData	knownWorld;

		Buffer<LiveCell>	cells;
		GenericHashTable<Grid::HCellIdI,index_t>	cellMap;
		BYTE			gridID;				//!< Identifier in the grid. 255/0xFF is invalid. Up to 254 different cell factions on one planet.
		TUniqueID		globalID;


		enum class ExecResult
		{
			NothingToDo,
			BadCommand,
			TargetConditionNotSuitable,
			CellConditionNotSuitable,
			Success
		};

		ExecResult		ExecuteTask(LiveCell&cell);

		void			SplitCell(const Grid::HCellIdI&cellID, const Grid::HCellIdI&newCellLocation, const LiveCell::Attributes&transfer);
			
	};



	void		Create();
	void		Destroy();





}





#endif










#endif
