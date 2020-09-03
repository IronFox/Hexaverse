#pragma once

#include <global_string.h>
#include <math/vclasses.h>
#include "../stringTable.h"
#include "stack.h"

namespace Grid
{

	
	class ResourceDiscovery
	{
	public:
		Ctr::FixedArray<bool,Grid::LowSetup::GridType::CellCount>			resourcesDiscovered;//discovers all resources (for now)

		bool				familyChanged[BaseGrid::NumFamilies] = {false};

		/**/				ResourceDiscovery()
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
		void				Update(const Grid::LCellIdI&center,float range, Ctr::BasicBuffer<UINT32>&outNewDiscoveries);
		/**
		Determines the set of visible stacks in the given radius around the given location.
		Clears the result set before appending new stack IDs.
		Do not call in parallel.
		*/
		static void			DetermineCoverage(const Grid::LCellIdI&center,float range,BasicBuffer<UINT32>&outCoveredStacks);

		
		/**
		Completely resets the internal state
		*/
		void				FlushAllData();

		#ifdef CLIENT
			static ResourceDiscovery	client;
		#endif
	};




	class Resource
	{
		String					name;
		float3					color;

	public:
		typedef BYTE			coverage_t;

		static const count_t	NumEntries = LowSetup::GridType::CellCount;

		coverage_t				coverage[NumEntries];

		/**
		Computes the coverage for a given location
		*/
		static coverage_t		GetRandomCoverage(const SimplexNoise&n, const float3&direction, float p, coverage_t max, float scale);

		static FixedArray<Resource,3>	all;

		static Resource&		GetFuel();
		static Resource&		GetMinerals();
		static Resource&		GetUranium();


		static void				Create(Objects::Resource::ID, const float3&color);
		static Resource&		Require(const String&name);
		static Resource&		Require(Objects::Resource::ID id);
		static coverage_t		Sample(const String&resourceName, index_t cellID);
		static coverage_t		Sample(Objects::Resource::ID id, index_t cellID);
		static void				FlushAll();

		index_t					resourceIndex;

		/**/					Resource()
		{}
		void					Name(Objects::Resource::ID id, const float3&color);
		
		void					Clear();

		static void				FillAll(TwisterRandom&random, const String&distribution);

		const String&			GetName() const {return name;}
		const float3&			GetColor() const {return color;}
	};


	extern String			currentCustomResourceDistribution;

	void					CreateResources();
	#ifdef CLIENT
		void				UpdateResourceMap(Objects::Resource::ID,const Ctr::ArrayData<TCellResourceCoverage>&, ResourceDiscovery&setKnown);
	#endif
}


