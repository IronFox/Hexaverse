#pragma once

#include "stack.h"
#include "../observer.h"

namespace World
{

	struct Condition
	{
		static bool			IsSolid(const Grid::CellStack::Cell&c)	{return c.IsSolid();}
		static bool			IsClearOrWater(const Grid::CellStack::Cell&c)	{return c.IsClearOrWater();}
		static bool			IsNotLava(const Grid::CellStack::Cell&c)	{return c.content != Grid::Content::Lava;}
	};


	/**
	Planetary data container.
	Describes content as well as visibility.
	Tracks changes, and can create deltas as needed.
	Changes to the terrain should be done using Data::BeginChangeSequence().
	Changes to visibility should be done using the Sensor class.
	*/
	class Data
	{
	public:
		struct WorldDeltaType
		{
			enum delta_type_t
			{
				/** *ProgressiveInit*
				Works on the assumption that the receiving faction client starts from an empty terrain.
				Initiates the client state to the current world state where visible, and initial world state where not.
				This type should only be used on completely new factions.
				*/
				ProgressiveInit,	
				ProgressiveUpdate,	//!< Works on the assumption that the recepient has a previous state, which should also be updated if visible
				ProgressiveUpdateNoDelta,	//!< Like ProgressiveUpdate but generates no delta structure. The passed delta parameter is ignored
				LocalCopy,		//!< Creates a copy of the specified world-state. Does not update anything. Does not reference any global state
				GlobalCopy,			//!< Creates a copy of the actual world state, assuming the recepient knows only an empty terrain. Does not update anything
				GlobalInitialCopy,	//!< Creates a copy of the initial world state, assuming the recepient knows only an empty terrain. Does not update anything
			};
		};
	public:

		static const constexpr float MaxOutdated = std::numeric_limits<float>::max();

		Observer<const Data&,const BaseGrid::FamiliesChanged&,float>		onTerrainChange;

		class ChangeSequence;
		class StackAccess
		{
		public:
			/**/				StackAccess(StackAccess&&other):stack(other.stack)	{other.stack = nullptr;}
			/**/				~StackAccess();
			/**/				StackAccess(const StackAccess&)=delete;
			void				operator=(const StackAccess&)=delete;

			Grid::CellStack&	GetStack() {return *stack;}
		private:
			explicit			StackAccess(Grid::CellStack*stack):stack(stack)	{}
			friend class ChangeSequence;


			Grid::CellStack		*stack=nullptr;

		};


		/**
		Begins a new sequence of terrain changes.
		The sequence automatically performs necessary all signalling upon destruction.
		The local Data instance must be global
		*/
		ChangeSequence			BeginChangeSequence(float t) {return ChangeSequence(this,t);}



		/**
		Temporary helper object to manipulate terrain data safely
		*/
		class ChangeSequence
		{
		public:

			typedef std::function<bool(const Grid::CellStack::Cell&)>	FCondition;

			/**
			Sets the specified cell to the specified content.
			Signals change if altered.
			Thread-safe as long as no two threads attempt to change the same cell.
			@return true if the content actually changed
			*/
			bool				Change(const Grid::HCellIdI&, /*Grid::Content*/ BYTE type);
			bool				Change(const Grid::HCellIdU&, /*Grid::Content*/ BYTE type);
			bool				ChangeConditionally(const Grid::HCellIdI&, /*Grid::Content*/ BYTE type, const FCondition&);
			bool				ChangeConditionally(const Grid::HCellIdU&, /*Grid::Content*/ BYTE type, const FCondition&);

			/**
			Changes visibility of the specified cells. Terrain sensory uses this. Should not be called explicitly from anywhere else
			*/
			void				AlterVisibility(const Grid::HCellIdI&,short changeBy);
			void				AlterVisibility(const Grid::HCellIdU&,short changeBy);

			/**
			Identical to the above Change() and ChangeConditionally(), but operates on the currently selected selected stack.
			Thread-safe ONLY if selected stack does not change, and no cell is changed by two threads at the same time. So, generally, not.
			*/
			bool				Change(int height, /*Grid::Content*/ BYTE type);
			bool				ChangeU(unsigned layer, /*Grid::Content*/ BYTE type);
			bool				ChangeConditionally(int height, /*Grid::Content*/ BYTE type, const FCondition&cond);
			bool				ChangeConditionallyU(unsigned layer, /*Grid::Content*/ BYTE type, const FCondition&cond);

			void				AlterVisibility(int height,short changeBy);
			void				AlterVisibilityU(unsigned layer,short changeBy);

			/**
			Sets the specified cell to Content::Clear.
			Signals change if altered.
			Thread-safe as long as no two threads attempt to change the same cell.
			@return true if the content actually changed
			*/
			bool				Clear(const Grid::HCellIdI&);
			bool				Clear(const Grid::HCellIdU&);
			/**
			Identical to the above but operates on the currently selected stack
			*/
			bool				Clear(int height);
			bool				ClearU(unsigned height);

			bool				AnyChanged() const {return changed.AnyChanged() || selectedStackChanged;}

			const Grid::CellStack::Cell&	GetCellI(int height) const {DBG_ASSERT_NOT_NULL__(parent); return selectedStack->GetCellI(height);}	//causes null-ptr exception if no stack is selected

			/**
			Selects the specified stack for faster manipulation
			*/
			void				SelectStack(index_t);
			StackAccess			AccessStack(index_t);

			void				ApplyFamilyDelta(BaseGrid::family_t family, const Array<Grid::CellStack::TTypeDelta>&data0, bool initialization);
			void				ApplyFamilyDelta(BaseGrid::family_t family, const Array<Grid::CellStack::TTypeDeltaBlock>&data0, bool initialization, bool actualWorld);


			/**
			Similar to Clear() but does not remove lava
			@return true if no lava was hit, false otherwise. Non-lava terrain is always removed
			*/
			bool				ClearNonLava(const Grid::LCellIdI&c);
			bool				ClearNonLava(const Grid::HCellIdI&c);
			bool				ClearNonLava(const Grid::HCellIdU&c);


			/**
			@brief Clears all high resolution cells intersecting the specified low resolution cell
			@return true if any matter was removed, false otherwise
			*/
			bool				Clear(const Grid::LCellIdI&ch);
			bool				Clear(const Grid::LCellIdU&ch);


			/**/				ChangeSequence(ChangeSequence&&);
			/**/				~ChangeSequence();

			void				operator=(const ChangeSequence&) = delete;
			/**/				ChangeSequence(const ChangeSequence&) = delete;
		private:
			friend ChangeSequence Data::BeginChangeSequence(float);

			/**/				ChangeSequence(Data*parent, float t):atTime(t),parent(parent)
			{}
			void				SignalChange(UINT32 stackID);	//thread-safe
			static void			DetailChangeVisibility(Grid::CellStack&stack, unsigned uh, short changeBy);
			bool				DetailChange(Grid::CellStack&stack, unsigned uh, BYTE type);
			bool				DetailChange(Grid::CellStack&stack, unsigned uh, BYTE type, const FCondition&);
			void				DetailOverwrite(Grid::CellStack&stack, Grid::CellStack::Cell&cell, unsigned uh, BYTE newType);

			const float			atTime;
			bool				terrainChanged = false;
			Data				*parent=nullptr;
			index_t				selectedStackIndex=0;
			Grid::CellStack		*selectedStack=nullptr;
			bool				selectedStackChanged=false;
			BaseGrid::FamiliesChanged	changed;
		};
		friend class ChangeSequence;

		/**/					Data();
		virtual					~Data();

		void					swap(Data&other)
		{
			cellStack.swap(other.cellStack);
			families.swap(other.families);
			swp(sensoryValid,other.sensoryValid);
			swp(lockLevel,other.lockLevel);
			swp(observerKey,other.observerKey);

			#ifdef HAS_WINDOW
				swp(lightingOutdated,other.lightingOutdated);
			#endif
		}

		bool					FamilyIsOutdated(index_t i) const {return families[i].structureOrSensory.didChange;}
		//void					UnsetFamilyOutdated(index_t i) {families[i].Reset();}

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
		@return true if any Content::Unknown blocks were encountered, false otherwise
		*/
		bool					GetStrengthRange(const Grid::LCellIdI&cell, Grid::Content&outWeakest, Grid::Content&outStrongest, bool includeUnknown) const;

		bool					IsRoad(const Grid::LCellIdI&, bool ignoreClearCellsOnEdge) const;

		/**
		Resets all cells to undefined. Leaves sensory as is.
		Signals change for all families at t=0
		*/
		void					FlushTerrain();

		/**
		Unsets all family changes.
		@return true if any changes were unset, false otherwise
		*/
		bool					FlushAllChanges();

		const Grid::CellStack::Cell&	GetCell(const Grid::HCellIdU&id) const { return cellStack[id.stackID].volumeCell[id.layer];}
		const Grid::CellStack::Cell&	GetCell(const Grid::HCellIdI&id) const { return cellStack[id.stackID].GetCellI(id.height);}
		const Grid::CellStack&			GetStack(UINT32 stackID) const {return cellStack[stackID];}
		const Array<Grid::CellStack>&	GetStacks() const {return cellStack;}


		/**
		Resets the structure counters of all cells
		@param resetVisibility Also clear out any visibility of any cell
		@param changedFamilies Entries are set to true where stacks were changed. May be null
		@return true if anything changed
		*/
		bool					ResetStructureCounters(bool resetVisibility, BaseGrid::FamiliesChanged*changedFamilies=nullptr);


		/**
		Similar to Clear() but fails if lava is attempted to remove
		@return true if no lava was hit, false otherwise. Non-lava terrain is always removed
		*/
		bool					ClearNonLava(const Grid::LCellIdI&ch, float atTime);

		//these manipulations should only be called on World::actual, since they alter the outdated state of global families
		/**
		@brief Clears all high resolution cells intersecting the specified low resolution cell
		@return true if any matter was removed, false otherwise
		*/
		bool					Clear(const Grid::LCellIdI&ch, float atTime);
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


			bool				UpdateLighting(bool force=false);	//updates local and neighbor stack lighting
		#endif

		bool					HasSensor(const Grid::LCellIdI&, float radius) const;

		bool					GridCanSee(const Grid::LCellIdI&, int height, bool broad) const;
		bool					GridCanSeeAny(const Grid::HCellIdU&, UINT height) const;

		float					GetMaxChangeTime() const {float t = 0; foreach (families,fam) t = vmax(t,fam->structureOrSensory.lastChange); return t;}

		/**
		@brief	Checks for changes in the specified family and possibly generates a delta by comparison with World::actual.

		Very fast if the respective family is not registered as changed. Parallel brute-force search otherwise.
		Resets the respective family change.

		Must only be called on non-global instances (asserted).
		Sensory must be valid (asserted).
		Family must be valid (asserted).
		
		@param f Family to check
		@param[out] delta Target storage for RLE compressed terrain delta. Content is undefined if the method returns false
		@param[inout] Time value to update in case changes were detected
		@param noDelta If set true, @a delta is not altered and the method always returns false, but internal grid is updated and flagged unchanged as needed
		@return true if noDelta is false and changes were detected, false otherwise. @a t remains unchanged if no changes were detected or noDelta is true
		*/
		bool					ProgressiveUpdate(BaseGrid::family_t f,  ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta, float&t, bool noDelta=false);

		/**
		@brief Compresses the specified family of the local world state as is.

		Change flags or timestamps are ignored.
		*/
		void					CompressFamilyState(BaseGrid::family_t f,  ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta);

		/**
		Affects the cell and its immediate neighbors only
		*/
		void					ChangeSensory(const Grid::HCellIdU&center, short changeBy, float t);
		void					RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);

		/**
		Checks whether or not the local instance is either @a World::initial or @a World::actual
		*/
		bool					IsGlobalInstance()	const;

		void					InvalidateSensory() {sensoryValid = false;}
		
		/**
		Validates the local sensory, and sets all sensory data to @a initialValue.
		All families are set to changed at time=0
		@param initialValue Initial cell visibility. 0 indicates invisibility
		*/
		void					FlushAndValidateSensory(short initialValue=0);


		bool					SensoryIsInvalid()const	{return !sensoryValid;}

	private:


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
			/**
			Must account for both structure and sensory. If either changed, must be set
			*/
			TChange			structureOrSensory;

			bool			Reset()
			{
				bool rs = structureOrSensory.didChange;
				structureOrSensory = TChange();
				return rs;
			}
		};


		int						lockLevel=0;
		bool					sensoryValid=true;
		FixedArray<FamilyState,BaseGrid::NumFamilies>		families;
		Array<Grid::CellStack>	cellStack;
		#ifdef HAS_WINDOW
			bool				lightingOutdated = true;
		#endif

		index_t					observerKey = InvalidIndex;

		void					SignalChange(const BaseGrid::FamiliesChanged&ch, float atTime, bool terrainChanged);

		/**
		Generates a world delta for storage/client notification
		//@param faction Reference to the respective client faction (must be set if deltaType is ProgressiveInit, ProgressiveUpdate, or FactionCopy)
		@param ws Faction/local world state. Required not-null for ProgressiveUpdate and LocalCopy, ignored otherwise. Updated during ProgressiveUpdate
		@param[out] delta Resulting delta
		@param familyID Index of the grid family that the delta should be computed of
		@param rightNow Indicates that the enclosed timestamp should be flagged as 'rightNow', as opposed to using the grid timestamp
		@param last Indicates that the result should include the 'last'-flag, which means no deltas will follow this one for the current sequence
		@param deltaType Type of delta to compute (see delta_type_t for details)
		*/
		void					GenerateDelta(BaseGrid::family_t familyID, WorldDeltaType::delta_type_t deltaType, ArrayData<Grid::CellStack::TTypeDeltaBlock>&delta);

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

		static void		ApplyVolume(World::Data&v, short changeBy, Data::ChangeSequence&);

		void			ApplySolidSphereTo(World::Data&v, short change, Data::ChangeSequence&)	const;
		void			ApplyHalfSolidSphereTo(World::Data&v, short changeBy, Data::ChangeSequence&seq)	const;
		void			ApplyShadedSphereTo(World::Data&v, short change, Data::ChangeSequence&)	const;
		void			MakeShaftVolume(Grid::HighSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
		void			MakeShaftVolume(Grid::LowSetup::TVolumeDescriptor&v, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
		void			ApplyShaftTo(World::Data&v, short change, const std::function<UINT32(UINT32 stackID)>&traceFunction, Data::ChangeSequence&)	const;
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
			UpperHalfSolidSphere,	//!< Upper half of SolidSphere	(including entity base level)
			ShadedSphere,	//!< Sphere that covers the area where non-occluded by terrain
			SolidCylinder,	//!< Cylinder stretching down from the specified world center to the bottom of the grid. Unaffected by terrain
			ShadedCylinder,	//!< Cylinder stretching down from the specified world center to the bottom of the grid. Halted by terrain
		};
		type_t			type=SolidSphere;

		/**/			Sensor()	{}
		/**/			Sensor(const float3&center, float radius, type_t t, int terrainPenetration=0)
							:worldCenter(center),
							gridCenter(Grid::high.Locate(center),Grid::CellStack::ToLayer(Grid::HeightFloorFromRadius(center.length()))),
							radius(radius),type(t), terrainPenetration(terrainPenetration){}

		/**/			Sensor(const Grid::LCellIdI&center, float fLayerOffset, float radius, type_t t, int terrainPenetration = 0)
							:worldCenter(Grid::ToWorld(center,fLayerOffset)),
							gridCenter(Grid::ToLayered(  Grid::ToHigh(center) )),
							radius(radius),type(t), terrainPenetration(terrainPenetration) {}

		/**/			Sensor(const Grid::HCellIdI&center, float fLayerOffset, float radius, type_t t, int terrainPenetration = 0)
							:worldCenter(Grid::ToWorld(center,fLayerOffset)),
							gridCenter(Grid::ToLayered(center)),
							radius(radius),type(t), terrainPenetration(terrainPenetration) {}

		void			ApplyTo(World::Data&clientWorld, short change, float t)	const;
		void			InsertInto(World::Data&clientWorld, float t) const {ApplyTo(clientWorld,+1,t);}
		void			RemoveFrom(World::Data&clientWorld, float t) const {ApplyTo(clientWorld,-1,t);}


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
