#pragma once

#include "volume.h"

namespace World
{
	using namespace Grid;

	class WorldData
	{
	public:
		Array<CellStack>		cellStack;
		#ifdef HAS_WINDOW
			bool				lightingOutdated = true;
			bool				structureChanged = true;
		#endif

		/**/					WorldData();

		void					swap(WorldData&other)
		{
			cellStack.swap(other.cellStack);
			#ifdef HAS_WINDOW
				swp(lightingOutdated,other.lightingOutdated);
				swp(structureChanged,other.structureChanged);
			#endif
		}
		/**
		Checks if the specified low-resolution cell is not at all covered (not even partially)
		*/
		bool					LowCellIsClear(const LCellIdI&cell)const;
		String					LowCellReport(const LCellIdI&cell)const;
		bool					LowCellIsClearOrWater(const LCellIdI&cell)const;
		bool					LowCellHasStructure(const LCellIdI&cell)const;
		bool					LowCellIsClearIgnoreStructure(const LCellIdI&cell)const;
		/**
		Determines the weakest and strongest content types found in the specified low-resolution cell
		@param includeUnknown Consider unknown blocks for outWeakest and outStrongest. If set false, outStrongest and outWeakest may still end up being unknown, if all are unknown
		@return true, if any Content::Unknown blocks were encountered, false otherwise
		*/
		bool					GetStrengthRange(const LCellIdI&cell, Content&outWeakest, Content&outStrongest, bool includeUnknown) const;

		bool					IsRoad(const LCellIdI&, bool ignoreClearCellsOnEdge) const;

		void					FlushAllData()
		{
			for (index_t i = 0; i < cellStack.Count(); i++)
			{
				cellStack[i].ZeroInit();
			}
		}
		void					ResetStructureCounters(bool resetVisibility)
		{
			for (index_t i = 0; i < cellStack.Count(); i++)
			{
				cellStack[i].ResetStructureCounters(resetVisibility);
			}
		}
		void					ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDelta>&data0, bool initialization);
		void					ApplyFamilyDelta(BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld);
		//void					ApplyFamilyDelta(Array<CellStack>&stacks, BYTE family, const Array<CellStack::TTypeDeltaBlock>&data0, const Array<CellStack::TStructCountDeltaBlock>&data1, bool initialization);

		/**
			Signals that the fine geometry in and on the edge of the specified cell has changed.
			This and horizontally neighboring cells will be updated (TODO)
		*/
		void					SignalChange(const LCellIdI&)	{}
		#ifdef HYBRID_GRID
			void				SignalChange(const HCellIdI&)	{}
		#endif
		/**
			Executes SignalChange() on all low cells up to and including the specified height
		*/
		void					SignalChangeUpTo(const LCellIdI&c)
		{
			//for (int h = -CellStack::HalfRange; h <= c.height; h++)	//no point now
				//SignalChange(LCellIdI(c.cellID,h));
		}

		//these manipulations should only be called on Grid::actualWorld, since they alter the outdated state of global families
		/**
		@brief Clears all high resolution cells intersecting the specified low resolution cell
		@return true, if any matter was removed, false otherwise
		*/
		bool					Clear(const LCellIdI&ch, float atTime);
		/**
		Similar to Clear() but fails if lava is attempted to remove
		@return true, if no lava was hit, false otherwise
		*/
		bool					ClearNonLava(const LCellIdI&ch, float atTime);
		bool					Overwrite(const LCellIdI&ch, Content with,  float atTime);
		void					Explode(const HCellIdI&,float radius, float atTime);
		void					Explode(const HCellIdI&id,float radius, bool*familyChanged);

		/**
		Redefines the world volume, based on the specified world configuration and discovery setting
		MUST NOT be used on the actual world while a session with active factions/players
		*/
		void					BuildWorld(const ExtendedPlanetConfig&config);
		/**
		Copies the volumetric composition of the local world data to the specified target according to the specified
		world discovery value.
		@param[out] target Target container to hold the filtered world data
		@param discoveryLevel Level of terrain obfuscation to apply to the copy. If set to FullTerrain, then @a target will contain an exact copy of the local world data.
		*/
		void					ExportDiscovered(WorldData&target, WorldDiscovery discoveryLevel) const;

		UINT32					TraceSkySensorLayer(UINT32 highCellCenter, UINT32 startingLayer, int penetration) const;
		int						TraceSkySensorHeight(UINT32 highCellCenter,  int penetration) const;


		#ifdef HAS_WINDOW
			/**
			Updates the visibility of all cells to the specified value.
			The previous value is discarded.
			*/
			void				OverrideVisibility(short v);
			/**
			Retrieves the average light-level of all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetAverageLightLevel(const LCellIdI&) const;

			/**
			Retrieves the highest light-level among all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetHighestLightLevel(const LCellIdI&id) const;


			void				UpdateLighting();	//updates local and neighbor stack lighting
			//void				CompileResources(THull<ResourceVertex>&target, index_t cellOffset, count_t cellCount, FClipHeight clipHeight,const Resource*resources, count_t numResources) const;
			#ifdef HYBRID_GRID
				void			SignalLightingChange(const HCellIdI&cellID);
			#endif
			void				SignalLightingChange(const LCellIdI&cellID);
		#endif

		bool					HasSensor(const LCellIdI&, float radius) const;

		bool					GridCanSee(const LCellIdI&, int height, bool broad) const;
		bool					GridCanSeeAny(const HCellIdU&, UINT height) const;

	};

	extern WorldData			actualWorld;
	#ifndef CLIENT
		extern WorldData		initialWorld;
	#endif
	
	
	void						ResetStructureCounters(bool resetVisibility=true);
	
}
