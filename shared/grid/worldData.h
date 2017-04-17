#pragma once

#include "stack.h"
#include "../observer.h"

namespace World
{

	class Data
	{
		struct TChange
		{
			float			lastChange=0;
			bool			didChange=false;

			void			Set()
			{
				didChange = true;
			}

			void			Set(float t)
			{
				lastChange = vmax(lastChange,t);
				didChange = true;
			}

			bool			Reset(float&compareTo)
			{
				if (!didChange)
					return false;
				didChange = false;
				compareTo = vmax(compareTo,lastChange);
				lastChange = 0;
				return true;
			}
		};

		struct FamilyState
		{
			TChange			structure;
			#ifndef CLIENT
				TChange		clientData;
			#endif

			void			Reset()
			{
				structure = TChange();
				#ifndef CLIENT
					clientData = TChange();
				#endif
			}
		};


		int						lockLevel=0;
		bool					sensoryValid=true;
		FixedArray<FamilyState,BaseGrid::NumFamilies>		families;
		Array<Grid::CellStack>	cellStack;
		bool					structureChanged = true;
		#ifdef HAS_WINDOW
			bool				lightingOutdated = true;
		#endif

		index_t					observerKey = InvalidIndex;

		void					SignalChange(const BaseGrid::FamiliesChanged&ch, float atTime);
	public:

		static const constexpr float MaxOutdated = std::numeric_limits<float>::max();

		Observer<const Data&,const BaseGrid::FamiliesChanged&,float>		onChange;


		class ChangeSequence
		{
		public:
			/**
			Sets the specified cell to the specified content.
			Signals change if altered
			@return true if the content actually changed
			*/
			bool				Change(const Grid::HCellIdI&, Grid::Content type);
			bool				Change(const Grid::HCellIdU&, Grid::Content type);
			/**
			Identical to the above but operates on the currently selected stack
			*/
			bool				Change(int height, Grid::Content type);
			bool				ChangeL(unsigned layer, Grid::Content type);
			/**
			Sets the specified cell to Content::Clear.
			Signals change if altered
			@return true if the content actually changed
			*/
			bool				Clear(const Grid::HCellIdI&);
			/**
			Identical to the above but operates on the currently selected stack
			*/
			bool				Clear(int height);
			bool				ClearL(unsigned height);

			bool				AnyChanged() const {return changed.AnyChanged() || selectedStackChanged;}

			/**
			Selects the specified stack for faster manipulation
			*/
			void				SelectStack(UINT32);

			void				ApplyFamilyDelta(BaseGrid::family_t family, const Array<Grid::CellStack::TTypeDelta>&data0, bool initialization);
			void				ApplyFamilyDelta(BaseGrid::family_t family, const Array<Grid::CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld);


			/**/				ChangeSequence(ChangeSequence&&);
			/**/				~ChangeSequence();

			void				operator=(const ChangeSequence&) = delete;
			/**/				ChangeSequence(const ChangeSequence&) = delete;
		private:
			friend class Data;

			/**/				ChangeSequence(Data*parent, float t):atTime(t),parent(parent)
			{}
			void				SignalChange(UINT32 stackID);
			static bool			DetailChange(Grid::CellStack&stack, unsigned uh, Grid::Content type);

			const float			atTime;
			Data				*parent;
			UINT32				selectedStackIndex=0;
			Grid::CellStack		*selectedStack=nullptr;
			bool				selectedStackChanged=false;
			BaseGrid::FamiliesChanged	changed;
		};
		friend class ChangeSequence;
		friend class Sensor;

		/**/					Data();
		virtual					~Data();

		void					swap(Data&other)
		{
			cellStack.swap(other.cellStack);
			families.swap(other.families);
			swp(structureChanged,other.structureChanged);
			swp(sensoryValid,other.sensoryValid);
			swp(lockLevel,other.lockLevel);
			swp(observerKey,other.observerKey);

			#ifdef HAS_WINDOW
				swp(lightingOutdated,other.lightingOutdated);
			#endif
		}

		bool					FamilyIsOutdated(index_t i) const {return families[i].structure.didChange;}
		void					UnsetFamilyOutdated(index_t i) {families[i].Reset();}

		/**
		Checks if the specified low-resolution cell is not at all covered (not even partially)
		*/
		bool					LowCellIsClear(const Grid::LCellIdI&cell)const;
		String					LowCellReport(const Grid::LCellIdI&cell)const;
		bool					LowCellIsClearOrWater(const Grid::LCellIdI&cell)const;
		bool					LowCellHasStructure(const Grid::LCellIdI&cell)const;
		bool					LowCellIsClearIgnoreStructure(const Grid::LCellIdI&cell)const;
		/**
		Determines the weakest and strongest content types found in the specified low-resolution cell
		@param includeUnknown Consider unknown blocks for outWeakest and outStrongest. If set false, outStrongest and outWeakest may still end up being unknown, if all are unknown
		@return true, if any Content::Unknown blocks were encountered, false otherwise
		*/
		bool					GetStrengthRange(const Grid::LCellIdI&cell, Grid::Content&outWeakest, Grid::Content&outStrongest, bool includeUnknown) const;

		bool					IsRoad(const Grid::LCellIdI&, bool ignoreClearCellsOnEdge) const;

		/**
		Resets all cells to undefined. Leaves sensory as is.
		Signals change for all families
		*/
		void					FlushTerrain();


		/**
		Begins a new sequence of terrain changes.
		The sequence automatically performs necessary all signalling upon destruction.
		The local Data instance must be global
		*/
		ChangeSequence			BeginChangeSequence(float t) {ASSERT__(IsGlobalInstance()); return ChangeSequence(this,t);}

		/**
		Resets the structure counters of all cells
		@param resetVisibility Also clear out any visibility of any cell
		@param changedFamilies Entries are set to true where stacks were changed. May be null
		@return true if anything changed
		*/
		bool					ResetStructureCounters(bool resetVisibility, BaseGrid::FamiliesChanged*changedFamilies=nullptr);


		//these manipulations should only be called on World::actual, since they alter the outdated state of global families
		/**
		@brief Clears all high resolution cells intersecting the specified low resolution cell
		@return true, if any matter was removed, false otherwise
		*/
		bool					Clear(const Grid::LCellIdI&ch, float atTime);
		/**
		Similar to Clear() but fails if lava is attempted to remove
		@return true, if no lava was hit, false otherwise
		*/
		bool					ClearNonLava(const Grid::LCellIdI&ch, float atTime);
		bool					Overwrite(const Grid::LCellIdI&ch, Grid::Content with,  float atTime);
		void					Explode(const Grid::HCellIdI&,float radius, float atTime);
		
		/**
		Copies the volumetric composition of the local world data to the specified target according to the specified
		world discovery value.
		@param[out] target Target container to hold the filtered world data
		@param discoveryLevel Level of terrain obfuscation to apply to the copy. If set to FullTerrain, then @a target will contain an exact copy of the local world data.
		*/
		void					ExportDiscovered(Data&target, Grid::WorldDiscovery discoveryLevel) const;

		UINT32					TraceSkySensorLayer(UINT32 highCellCenter, UINT32 startingLayer, int penetration) const;
		int						TraceSkySensorHeight(UINT32 highCellCenter,  int penetration) const;


		#ifdef HAS_WINDOW
			/**
			Retrieves the average light-level of all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetAverageLightLevel(const Grid::LCellIdI&) const;

			/**
			Retrieves the highest light-level among all high-resolution cells touched by the specified low-resolution cell
			*/
			float				GetHighestLightLevel(const Grid::LCellIdI&id) const;


			void				UpdateLighting();	//updates local and neighbor stack lighting
			//void				CompileResources(THull<ResourceVertex>&target, index_t cellOffset, count_t cellCount, FClipHeight clipHeight,const Resource*resources, count_t numResources) const;
			//#ifdef HYBRID_GRID
			//	void			SignalLightingChange(const Grid::HCellIdI&cellID);
			//#endif
			//void				SignalLightingChange(const Grid::LCellIdI&cellID);
			void				OverrideVisibility(short v);
		#endif

		bool					HasSensor(const Grid::LCellIdI&, float radius) const;

		bool					GridCanSee(const Grid::LCellIdI&, int height, bool broad) const;
		bool					GridCanSeeAny(const Grid::HCellIdU&, UINT height) const;

		/**
		Prevents all calls to CommitClientChanges() until UnlockClientChanges() is called.
		Supports nested calls up to depth 16
		*/
		void					LockClientChanges()	{ASSERT_LESS__(lockLevel,16);lockLevel++; }
		/**
		Unlocks calls to CommitClientChanges(). Calls CommitClientChanges() automatically, if the lock level falls back to zero
		*/
		void					UnlockClientChanges(float&t)	{ASSERT_GREATER__(lockLevel,0); if (--lockLevel == 0) CommitClientChanges(t);}

		/**
		Must only be called on non-global instances.
		Sensory must be valid.
		Compares visible changed terrain to actual terrain, copies where necessary, and sets clientData where changed.
		*/
		void					CommitClientChanges(float&t);

		/**
		Affects the cell and its immediate neighbors only
		*/
		void					ChangeSensory(const Grid::HCellIdU&center, short changeBy);
		void					RegisterExplosionCrater(const Grid::HCellIdI&center, float radius);

		/**
		Checks whether or not the local instance is either @a World::initial or @a World::actual
		*/
		bool					IsGlobalInstance()	const;

		void					InvalidateSensory() {sensoryValid = false;}
		void					FlushAndValidateSensory();
		bool					SensoryIsInvalid()const	{return !sensoryValid;}
	};

	extern Data			actual;
	#ifndef CLIENT
		extern Data		initial;
	#endif
	
	


	/**
	Sensor shape descriptor
	*/
	class Sensor
	{
	private:
		static Grid::HighSetup::TVolumeDescriptor::Operator op;
		static Grid::LowSetup::TVolumeDescriptor::Operator lowOp;
		static Grid::HighSetup::TVolumeDescriptor	volume;

		static void		ApplyVolume(World::Data&v, short changeBy);

		void			ApplySolidSphereTo(World::Data&v, short change)	const;
		void			ApplyShadedSphereTo(const World::Data&w, World::Data&v, short change)	const;
		void			MakeShaftVolume(Grid::HighSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
		void			MakeShaftVolume(Grid::LowSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
		void			ApplyShaftTo(World::Data&v, short change, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
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

		void			ApplyTo(World::Data&clientWorld, short change)	const;
		void			InsertInto(World::Data&clientWorld) const {ApplyTo(clientWorld,+1);}
		void			RemoveFrom(World::Data&clientWorld) const {ApplyTo(clientWorld,-1);}


		/**
		Determines whether the local sensor contains the specified location.
		Note that this does not equal actual visibility unless the specified location is off-grid.
		If within the terrain grid and using a shaded type, the method only checks theoretical visibility.
		@param loc Location to check
		@return true if the location can be visibile, false if it is definitely out of scope
		*/
		bool			Contains(const Grid::LCellIdI&loc) const;

		/**
		Computes the maximum sensor volume in low-resolution.
		The actual volume may diverge considerably, but never exceed the result
		@param[out] desc Resulting volume
		*/
		void			GetMaxVolume(Grid::LowSetup::TVolumeDescriptor&desc) const;

		
	};


	void	Create();

	
}
