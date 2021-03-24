#include <global_root.h>
#include "resources.h"

#include "../objects/resource.h"
#include "../globalEvents.h"
#ifndef CLIENT
	#include <lua/luaGameAPI.h>
#endif

namespace Grid
{


	/*static*/ FixedArray<Resource,3>	Resource::all;


	String							currentCustomResourceDistribution="clustered";


	
	/*static*/ void Resource::FillAll(TwisterRandom&random, const String&distribution)
	{
		#ifndef CLIENT
		if (distribution.IsNotEmpty())
		{
			try
			{
				Lua::API api;
				api.Create("Resources:"+distribution);
				api.AddGrid();
				
				SimplexNoise n(random.MaxNext());
				api.gameTable.SetFunction("Simplex",[&n](double x, double y, double z)->double
				{
					return n.Get(x,y,z);
				});

				api.gameTable.SetFunction("RandomInt",[&random](int min, int max)->int
				{
					return random.Next(min,max);
				});


				LogEvent(EventClass::Preset::configurationPrint,"Executing resource distribution program '"+distribution+"'");
				Time::Stamp t0;
				api.program->ExecuteFile(PathString("../lua/distributions/"+distribution+".lua"),nullptr);

				Lua::Function<Lua::Table()> compute = api.program->RequireGlobal("Compute");
				Lua::Table result = compute();
				LogEvent(EventClass::Preset::configurationPrint,"'"+distribution+"' took "+String(t0.ElapsedSeconds())+" seconds");

				foreach (Resource::all,res)
				{
					Lua::Table sub = result.GetField(res->name);
					if (sub.IsEmpty())
						throw Lua::Exception("Unable to fetch table for resource '"+res->name+"' in response from "+distribution+".lua : Compute()");
					if (sub.GetLength() != LowSetup::GridType::CellCount)
						throw Lua::Exception("Response size of "+distribution+".lua : Compute() for resource '"+res->name+"' ("+String(sub.GetLength())+") does not match expected size "+String(LowSetup::GridType::CellCount));
					for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
						res->coverage[i] = sub.GetElement(i+1).GetAs<coverage_t>();
				}
			}
			catch (const Lua::Exception&ex)
			{
				LogEvent(EventClass::Preset::configurationError,ex.what());
				LogEvent(EventClass::Preset::configurationPrint,"Natural resources will be unavailable exept for base starting locations");
				foreach (Resource::all,res)
					res->Clear();
			}
		}
		else
		#endif
		{
				foreach (Resource::all,res)
					res->Clear();
		}
	}


	void	CreateResources()
	{
		Resource::Create(Objects::Resource::Fuel, float3(0.1,1,0.1));
		Resource::Create(Objects::Resource::Minerals, float3(0.9,0.9,0.9));
		Resource::Create(Objects::Resource::Uranium, float3(1,1,0.1));


		GlobalEvents::terrainFlush << Resource::FlushAll;
		#ifdef CLIENT
			GlobalEvents::terrainFlush << [](){Grid::ResourceDiscovery::client.FlushAllData();};
		#endif
	}






	/*static*/ Resource::coverage_t			Resource::Sample(const String&resourceName, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		return Require(resourceName).coverage[stackID];
	}
	/*static*/ Resource::coverage_t			Resource::Sample(Objects::Resource::ID id, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		return Require(id).coverage[stackID];
	}

	/*static*/ Resource&		Resource::Require(const String&name)
	{
		foreach (all,res)
			if (res->name.EqualsIgnoreCase(name))
				return *res;

		FATAL__("Unable to locate resource '"+name+'"');
		return all.First();
	}

	/*static*/ Resource&		Resource::Require(Objects::Resource::ID id)
	{
		ASSERT_LESS__(id.serialIndex,3);
		return all[id.serialIndex];
	}


	///*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, const M::TVec3<>&seed,float rarity, float maxYield)
	//{
	//	// low.cells[i].centerVertex.xyz,
	//	R&rc = Resource::all.Append();
	//	rc.Name(name,cname,color);
	//	for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
	//	{
	//		#ifndef CLIENT
	//			rc.coverage[i] = GetResourceCoverage(seed, low.cells[i].centerVertex.xyz,rarity,maxYield);
	//		#else
	//			rc.coverage[i] = 0;
	//			rc.known[i] = false;
	//		#endif

	//	}
	//}

	/*static*/ Resource::coverage_t		Resource::GetRandomCoverage(const SimplexNoise&n, const float3&direction, float p, coverage_t max, float scale)
	{
		#ifdef GRID_INDEPENDENT_RANDOM
			static Random random;
			float q = 1.f - p;
			coverage_t rs (M::Round(M::Sqr(M::Sqr(M::Sqr(std::max(0.f,(random.NextFloat()-q)/p)))) * (max-1)));
			if (rs)
				rs++;
			return rs;
		#else
		//	{0.08,0.08,0,0,0.008 } ;

			static const int reduce = 5;

			//float t0 = fabs(noise.Get(direction + seed,8.f));	//wide areas
			float t1 = (Grid::Get(n,direction *M_PIF/2.5f * 8));	//small areas

			int rs = std::min(1.f,scale * t1) * (reduce + max);
			rs -= reduce;
			if (rs < 3)
				rs = 0;
			return rs;
//
//			float t2 = fabs(noise.Get(direction + seed+3.f,9.f));	//cutout
//
//			float common = std::min(1.f,(p/0.06f));
//			t0 = std::min(1.f,t0*(t0*1.9f)*common);
//			t1 = std::min(1.f,t1*(t1*1.9f)*(1.1f-0.2f * common));
//
//
//			float t = std::max(t0,t1);
//
//			t *= M::SmoothStep(0.2f,0.3f,t2);
//
//
//			//float t = t0 * (0.3f + 0.7f * t1);
////			float q = 0.26f-sqrt(p) * 0.5f;
//			float q = 0.9f-sqrt(p) * 1.5f;
//			p = 0.30f;// + 0.1f * t0;
//
//			float h = 0.15f + (1.f - t0)*0.85f;
//			//p = 0.75f;
//			//float q = 1.f - p;
//			coverage_t rs (M::Round(/*M::Sqr*/(/*M::Sqr*/(/*M::Sqr*/(M::Clamp((t-q)/p *h,0,1)))) * (max-1)));
//			if (rs)
//				rs++;
//			return rs;
		#endif
	}


	void Resource::Clear()
	{
		for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
		{
			coverage[i] = 0;
		}
	}

	/*static*/ void Resource::Create(Objects::Resource::ID id, const float3&color)
	{
		ASSERT_LESS__(id.serialIndex, Objects::Resource::table.Count());
		ASSERT_LESS__(id.serialIndex, Resource::all.Count());
		Resource&rc = Resource::all[id.serialIndex];
		rc.Name(id,color);
	}

	void			Resource::Name(Objects::Resource::ID id, const float3&color)	
	{
		this->name = id.ToString(); 
		this->resourceIndex = id.serialIndex; 
		this->color = color;
	}

	/*static*/ Resource&		Resource::GetFuel()		{return all[Objects::Resource::FuelID];}
	/*static*/ Resource&		Resource::GetMinerals()	{return all[Objects::Resource::MineralID];}
	/*static*/ Resource&		Resource::GetUranium()	{return all[Objects::Resource::UraniumID];}


	#ifdef CLIENT
		void		UpdateResourceMap(Objects::Resource::ID resourceID,const Ctr::ArrayData<TCellResourceCoverage>&data, ResourceDiscovery&setSensor)
		{
			auto&res = Grid::Resource::all[resourceID.serialIndex];
			foreach (data,el)
			{
				ASSERT_LESS__(el->cellIndex,ARRAYSIZE(res.coverage));
				res.coverage[el->cellIndex] = el->coverage;
				setSensor.resourcesDiscovered[el->cellIndex] = true;
				setSensor.familyChanged[low.grid.GetCell(el->cellIndex).family] = true;
			}
		}
	#endif


	#ifdef CLIENT
		/*static*/ ResourceDiscovery	ResourceDiscovery::client;
	#endif


	void ResourceDiscovery::FlushAllData()
	{
		resourcesDiscovered.Fill(false);
	}


	/*static*/ void		Resource::FlushAll()
	{
		foreach (all,r)
			for (index_t i = 0; i < NumEntries; i++)
				r->coverage[i] = 0;
	}

	/*static*/ void			ResourceDiscovery::DetermineCoverage(const Grid::LCellIdI&center,float range,BasicBuffer<UINT32>&outCoveredStacks)
	{
		static Ctr::IndexSet covered;
		static Ctr::Queue<UINT32>	stacks;
		covered.Clear();
		outCoveredStacks.Clear();
		
		const M::TVec3<> worldCenter = Grid::ToWorld(center);
		
		stacks.Clear();
		stacks << center.stackID;

		UINT32 stackID;
		while (stacks >> stackID)
		{
			const M::TVec3<> world = Grid::ToWorld(Grid::LCellIdI(stackID,center.height));
			if (Vec::quadraticDistance(world,worldCenter) > M::Sqr(range))
				continue;
			if (!covered.IsSet(stackID))
			{
				covered.Set(stackID);
				outCoveredStacks << stackID;
			}
			else
				continue;

			const auto&n = Grid::low.grid.GetCell(stackID).neighborhood;
			for (BYTE k = 0; k < n.numNeighbors; k++)
				stacks << n.neighborID[k];
		}

	}



	void		ResourceDiscovery::Update(const Grid::LCellIdI&center,float range, Ctr::BasicBuffer<UINT32>&outNewDiscoveries)
	{
		static Ctr::Buffer0<UINT32>	coverage;
		outNewDiscoveries.Clear();

		DetermineCoverage(center,range,coverage);
			
		foreach (coverage,cov)
		{
			if (resourcesDiscovered[*cov])
				continue;
			resourcesDiscovered[*cov] = true;
			outNewDiscoveries << *cov;
		}
	}

}

