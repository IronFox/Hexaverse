#pragma once

#include <global_string.h>
#include <math/vclasses.h>
#include "../stringTable.h"
#include "stack.h"

namespace Grid
{

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

		static Buffer0<Resource>all;
		static void				Create(const String&name, ConstName cname, const float3&color, index_t);
		static Resource&		Require(const String&name);
		static Resource&		Require(ConstName name);
		static coverage_t		Sample(const String&resourceName, index_t cellID);
		static coverage_t		Sample(ConstName resourceName, index_t cellID);

		index_t					resourceIndex;

		/**/					Resource()
		{}
		void					Name(const String&name, ConstName cname, const float3&color)	{this->name = name; this->cname = cname; this->color = color;}
		
		void					FillInternal(Random&random, float p, coverage_t, float density);
		void					FillInternal(Random&random);

		static void				FillAll(Random&random, const String&distribution);

		const String&			GetName() const {return name;}
		ConstName				GetCName() const {return cname;}
		const float3&			GetColor() const {return color;}
	};


	extern String			currentCustomResourceDistribution;

	void					CreateResources();
}


