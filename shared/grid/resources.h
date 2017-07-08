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
		ConstName				cname;
		float3					color;

	public:
		typedef BYTE			coverage_t;

		static const count_t	NumEntries = LowSetup::GridType::CellCount;

		coverage_t				coverage[NumEntries];

		/**
		Computes the coverage for a given location
		*/
		static coverage_t		GetRandomCoverage(const SimplexNoise&n, const float3&direction, float p, coverage_t max, float scale);

		static float			probability[],density[];
		static coverage_t		maximum[];

		static Ctr::Buffer0<Resource>all;
		static void				Create(const String&name, ConstName cname, const float3&color, index_t);
		static Resource&		Require(const String&name);
		static Resource&		Require(ConstName name);
		static coverage_t		Sample(const String&resourceName, index_t cellID);
		static coverage_t		Sample(ConstName resourceName, index_t cellID);
		static void				FlushAll();

		index_t					resourceIndex;

		/**/					Resource()
		{}
		void					Name(const String&name, ConstName cname, const float3&color)	{this->name = name; this->cname = cname; this->color = color;}
		
		void					FillInternal(TwisterRandom&random, float p, coverage_t, float density);
		void					FillInternal(TwisterRandom&random);

		static void				FillAll(TwisterRandom&random, const String&distribution);

		const String&			GetName() const {return name;}
		ConstName				GetCName() const {return cname;}
		const float3&			GetColor() const {return color;}
	};


	extern String			currentCustomResourceDistribution;

	void					CreateResources();
	#ifdef CLIENT
		void				UpdateResourceMap(ConstName resourceName,const Ctr::ArrayData<TCellResourceCoverage>&, ResourceDiscovery&setKnown);
	#endif
}


