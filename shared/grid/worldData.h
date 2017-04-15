#pragma once

#include "stack.h"

namespace World
{
	using namespace Grid;

	class Data
	{
		int						lockLevel=0;
		bool					sensoryValid=true;
	public:
		Array<CellStack>		cellStack;
		bool					structureChanged = true;
		#ifdef HAS_WINDOW
			bool				lightingOutdated = true;


		#endif

		static const constexpr float MaxOutdated = std::numeric_limits<float>::max();

		struct FamilyState
		{
			float			lastChange;
			bool			coverageChanged, changed;

			/**/			FamilyState() :lastChange(0), coverageChanged(false), changed(false)
			{}
			void			Reset()
			{
				lastChange = 0;
				coverageChanged = false;
				changed = false;
			}

			void			SignalStructureChange(float t)
			{
				lastChange = vmax(lastChange,t);
				changed = true;
			}
			void			SignalStructureChange()
			{
				changed = true;
			}

			void			SignalCoverageChange(float t)
			{
				lastChange = vmax(lastChange,t);
				coverageChanged = true;
			}
			void			SignalCoverageChange()
			{
				lastChange = 0;
				coverageChanged = true;
			}
		};
		FixedArray<FamilyState,BaseGrid::NumFamilies>		families;

		/**/					Data();

		void					swap(Data&other)
		{
			cellStack.swap(other.cellStack);
			families.swap(other.families);
			#ifdef HAS_WINDOW
				swp(lightingOutdated,other.lightingOutdated);
				swp(structureChanged,other.structureChanged);
			#endif
		}

		bool					FamilyIsOutdated(index_t i) const {return families[i].changed;}
		void					UnsetFamilyOutdated(index_t i) {families[i].Reset();}
		void					SignalFamilyStructureChange(index_t i, float t) {families[i].SignalStructureChange(t);structureChanged=true;}
		void					SignalFamilyStructureChange(index_t i) {families[i].SignalStructureChange();structureChanged=true;}
		void					SignalFamilyCoverageChange(index_t i, float t) {families[i].SignalCoverageChange(t);}
		void					SignalFamilyCoverageChange(index_t i) {families[i].SignalCoverageChange();}
		void					SetAllFamiliesOutdated(float t)
		{
			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				SignalFamilyStructureChange(k,t);
			}
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

		void					FlushAllData(bool resetVisibility);
		/**
		Resets the structure counters of all cells
		@param resetVisibility Also clear out any visibility of any cell
		@param changedFamilies Entries are set to true where stacks were changed. May be null
		@return true if anything changed
		*/
		bool					ResetStructureCounters(bool resetVisibility, BaseGrid::FamiliesChanged*changedFamilies=nullptr);
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

		//these manipulations should only be called on World::actual, since they alter the outdated state of global families
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
		Copies the volumetric composition of the local world data to the specified target according to the specified
		world discovery value.
		@param[out] target Target container to hold the filtered world data
		@param discoveryLevel Level of terrain obfuscation to apply to the copy. If set to FullTerrain, then @a target will contain an exact copy of the local world data.
		*/
		void					ExportDiscovered(Data&target, WorldDiscovery discoveryLevel) const;

		UINT32					TraceSkySensorLayer(UINT32 highCellCenter, UINT32 startingLayer, int penetration) const;
		int						TraceSkySensorHeight(UINT32 highCellCenter,  int penetration) const;


		#ifdef HAS_WINDOW
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
			void				OverrideVisibility(short v);
		#endif

		bool					HasSensor(const LCellIdI&, float radius) const;

		bool					GridCanSee(const LCellIdI&, int height, bool broad) const;
		bool					GridCanSeeAny(const HCellIdU&, UINT height) const;

		/**
		Prevents all calls to CheckChanges() until UnlockChangeChecking() is called.
		Supports nested calls up to depth 16
		*/
		void					LockChangeChecking()	{ASSERT_LESS__(lockLevel,16);lockLevel++; }
		/**
		Unlocks calls to CheckChanges(). Calls CheckChanges() automatically, if the lock level falls back to zero
		*/
		void					UnlockChangeChecking(float t)	{ASSERT_GREATER__(lockLevel,0); if (--lockLevel == 0) CheckChanges(t);}

		/**
		Processes all families for which FamilyState::changed is true, and detects if the visible terrain
		coverage actually changed. If so, the respective value FamilyState::coverageChange is set true,
		and FamilyState::lastChange is set to the specified parameter t.
		Finally, FamilyState::changed is set false for all families.
		*/
		void					CheckChanges(float t);

		/**
		Affects the cell and its immediate neighbors only
		*/
		void					ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy);
		void					RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);


		void					InvalidateSensory() {sensoryValid = false;}
		void					FlushAndValidateSensory();
		bool					SensoryIsInvalid()const	{return !sensoryValid;}
	};

	extern Data			actual;
	#ifndef CLIENT
		extern Volume		initial;
	#endif
	
	


	/**
	Sensor shape descriptor
	*/
	class Sensor
	{
	private:
		static Grid::HighSetup::TVolumeDescriptor::Operator op;
		static Grid::HighSetup::TVolumeDescriptor	volume;

		static void		ApplyVolume(World::Data&v, short changeBy, bool*familyChanged);

		void			ApplySolidSphereTo(World::Data&v, short change, bool*familyChanged)	const;
		void			ApplyShadedSphereTo(World::Data&v, short change, bool*familyChanged)	const;
		void			ApplyShaftTo(World::Data&v, short change, bool*familyChanged, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
	public:
		/**
		3d location of the local sensor
		*/
		float3			worldCenter;
		/**

		*/
		Grid::HCellIdU	gridCenter;
		/**

		*/
		float			radius=1;

		/**
		Steps the sensor can walk through terrain matter.
		Only effective if type is ShadedCylinder
		*/
		int				terrainPenetration=0;

		enum type_t
		{
			SolidSphere,	//!< Sphere that covers the entire area. Unaffected by terrain
			ShadedSphere,	//!< Sphere that covers the area where non-occluded by terrain
			SolidCylinder,	//!< Cylinder stretching down from the specified world center to the bottom of the grid. Unaffected by terrain
			ShadedCylinder,	//!< Cylinder stretching down from the specified world center to the bottom of the grid. Halted by terrain
		};
		type_t			type=SolidSphere;

		/**/			Sensor()	{}
		/**/			Sensor(const float3&center, float radius, type_t t, int terrainPenetration=0)
							:worldCenter(center),
							gridCenter(Grid::high.Locate(center),Grid::CellStack::ToLayer(Grid::HeightFloorFromRadius(center.length()))),
							radius(radius),type(t)	{}

		/**/			Sensor(const Grid::LCellIdI&center, float fLayerOffset, float radius, type_t t)
							:worldCenter(Grid::ToWorld(center,fLayerOffset)),
							gridCenter(Grid::ToLayered(  Grid::ToHigh(center) )),
							radius(radius),type(t)	{}

		/**/			Sensor(const Grid::HCellIdI&center, float fLayerOffset, float radius, type_t t)
							:worldCenter(Grid::ToWorld(center,fLayerOffset)),
							gridCenter(Grid::ToLayered(center)),
							radius(radius),type(t)	{}

		void			ApplyTo(World::Data&, short change)	const;
		void			InsertInto(World::Data&w) const {ApplyTo(w,+1);}
		void			RemoveFrom(World::Data&w) const {ApplyTo(w,-1);}
	};

	
}
