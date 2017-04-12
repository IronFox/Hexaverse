#pragma once

#include "stack.h"


namespace SensorCoverage
{




	
	class Volume
	{
	private:
		count_t		lockLevel = 0;
	public:
		struct CellStack
		{
			struct Cell
			{
				short	visibility=0;
				bool	oldVisible=false, temporarilyVisible=false;
				BYTE	user=0;

				#ifdef HAS_WINDOW
					Grid::TVisibility	GetVisibility() const {return Grid::TVisibility(visibility>0);}
				#endif

				bool	Changed(bool reset)
				{
					bool visible = temporarilyVisible || visibility > 0;
					bool before = oldVisible;
					if (reset)
						oldVisible = visibility > 0;	//dont include temp here
					return visible != before;
				}

				void	AlterVisibility(short change)
				{
					visibility += change;
					if (change >= 0)
					{
						temporarilyVisible = true;
					}
				}

				/**
				Checks whether or not the terrain contents of the local cell are currently visible,
				either because it is currently in range, or has been in range at any given time during
				the last processing round.
				Use this method for terrain contents only. Not to be used for entity visibility.
				Use IsCurrentlyVisible() instead!
				*/
				bool	TerrainIsVisible() const
				{
					return visibility > 0 || temporarilyVisible;
				}

				/**
				Checks whether or not this cell is currently visible.
				Should be used for entity visibility.
				*/
				bool	IsCurrentlyVisible() const
				{
					return visibility > 0;
				}
			};
			Cell	volumeCell[Grid::CellStack::Range];

			/**/	CellStack()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}

			void	Reset()
			{
				memset(volumeCell, 0, sizeof(volumeCell));
			}
		};

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

		};
		FamilyState			families[BaseGrid::NumFamilies];

		Array<CellStack>	cellStack;


		/**/				Volume():cellStack(Grid::HighSetup::GridType::CellCount)
		{
		}



		/**
		Affects the cell and its immediate neighbors only
		*/
		void				ChangeSensory(const Grid::HCellIdU&center, float t, short changeBy);
		void				RegisterExplosionCrater(const Grid::HCellIdI&center, float radius, float t);

		/**
		Completely resets the internal state
		*/
		void				FlushAllData();
		/**
		Prevents all calls to CheckChanges() until UnlockChangeChecking() is called.
		Supports nested calls up to depth 16
		*/
		void				LockChangeChecking()	{ASSERT_LESS__(lockLevel,16);lockLevel++; }
		/**
		Unlocks calls to CheckChanges(). Calls CheckChanges() automatically, if the lock level falls back to zero
		*/
		void				UnlockChangeChecking(float t)	{ASSERT_GREATER__(lockLevel,0); if (--lockLevel == 0) CheckChanges(t);}

		/**
		Processes all families for which FamilyState::changed is true, and detects if the visible terrain
		coverage actually changed. If so, the respective value FamilyState::coverageChange is set true,
		and FamilyState::lastChange is set to the specified parameter t.
		Finally, FamilyState::changed is set false for all families.
		*/
		void				CheckChanges(float t);



		bool				IsValid();
		/**
		Signals that this state cannot be referenced until sensory has been reconstructed from all world-entities
		*/
		void				Invalidate();
		void				SetValid();

		static Volume		client;
	};


	class Resources
	{
	public:

		FixedArray<bool,Grid::LowSetup::GridType::CellCount>			resourcesDiscovered;//discovers all resources (for now)

		/**/				Resources()
		{
			resourcesDiscovered.Fill(false);
		}


		/**
		Detects resource coverage changes within the specified sphere, and updates @a resourcesDiscovered where necessary.
		Uses some static fields. Do not call in parallel.
		@param center Terrain center to detect resources from
		@param range Sphere radius to detect resources within
		@param[inout] outNewDiscoveries Buffer to add newly discovered cell stacks to. Cleared automatically
		*/
		void				Update(const Grid::LCellIdI&center,float range, BasicBuffer<UINT32>&outNewDiscoveries);

		
		/**
		Completely resets the internal state
		*/
		void				FlushAllData();


	};


	/**
	Sensor shape descriptor
	*/
	class Sensor
	{
	private:
		static Grid::HighSetup::TVolumeDescriptor::Operator op;
		static Grid::HighSetup::TVolumeDescriptor	volume;

		static void		ApplyVolume(Volume&v, short changeBy, bool*familyChanged);

		void			ApplySolidSphereTo(Volume&v, short change, bool*familyChanged)	const;
		void			ApplyShadedSphereTo(Volume&v, const World::Data&,short change, bool*familyChanged)	const;
		void			ApplyShaftTo(Volume&v, short change, bool*familyChanged, const std::function<UINT32(UINT32 stackID)>&traceFunction)	const;
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

		void			ApplyTo(Volume&, const World::Data&, short change)	const;
		void			InsertInto(Volume&s,const World::Data&w) const {ApplyTo(s,w,+1);}
		void			RemoveFrom(Volume&s,const World::Data&w) const {ApplyTo(s,w,-1);}
	};


}

