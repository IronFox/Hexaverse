#include <global_root.h>
#include "resources.h"

#include "../objects/resource.h"
#include "../globalEvents.h"
#include "../luaGameAPI.h"

namespace Grid
{
	#ifndef NO_MUNITION
		/*static*/ float			Resource::probability[Objects::Resource::NumberOfPossibilities] = {0.08f,0.08f,0,0,0.008f } ;
		/*static*/ float			Resource::density[Objects::Resource::NumberOfPossibilities] = {0.04f,0.06f,0,0,0.02f } ;
		/*static*/ Resource::coverage_t		Resource::maximum[Objects::Resource::NumberOfPossibilities] = {12,12,0,0,6};
	#else
		/*static*/ float			Resource::probability[Objects::Resource::NumberOfPossibilities] = {0.08f,0.08f,0,0.008f } ;
		/*static*/ float			Resource::density[Objects::Resource::NumberOfPossibilities] = {0.04f,0.06f,0,0.02f } ;
		/*static*/ Resource::coverage_t		Resource::maximum[Objects::Resource::NumberOfPossibilities] = {12,12,0,6};
	#endif


	/*static*/ Buffer0<Resource>	Resource::all;

	String							currentCustomResourceDistribution="clustered";


	
	/*static*/ void Resource::FillAll(Random&random, const String&distribution)
	{
		#ifndef CLIENT
		if (distribution.IsNotEmpty())
		{
			try
			{
				Lua::PProgram prog = Lua::Program::Create("Resources");
				Lua::Table api = Lua::CreateGeneralAPI(prog);
				Lua::DefineLowGridCells(api);
				
				SimplexNoise n(random.NextSeed());
				api.SetFunction("Simplex",[&n](double x, double y, double z)->double
				{
					return n.Get(x,y,z);
				});

				api.SetFunction("RandomInt",[&random](int min, int max)->int
				{
					return random.Next(min,max);
				});


				LogEvent(EventClass::Preset::configurationPrint,"Executing resource distribution program '"+distribution+"'");
				Timer::Time t0 = timer.Now();
				prog->ExecuteFile(PathString("../lua/distributions/"+distribution+".lua"));

				Lua::Function<Lua::Table()> compute = prog->RequireGlobal("Compute");
				Lua::Table result = compute();
				LogEvent(EventClass::Preset::configurationPrint,"'"+distribution+"' took "+timer.GetSecondsSince(t0)+" seconds");

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
				LogEvent(EventClass::Preset::configurationPrint,"Falling back to internal distribution");
				foreach (Resource::all,res)
					res->FillInternal(random);
			}
		}
		else
		#endif
		{
			foreach (Resource::all,res)
				res->FillInternal(random);
		}
	}


	void	CreateResources()
	{
		Resource::Create("Fuel", ConstName::Resource_Fuel, float3(0.1,1,0.1), Objects::Resource::Fuel);
		Resource::Create("Minerals", ConstName::Resource_Minerals, float3(0.9,0.9,0.9), Objects::Resource::Minerals);
		Resource::Create("Uranium", ConstName::Resource_Uranium, float3(1,1,0.1), Objects::Resource::Uranium);


		GlobalEvents::terrainFlush << Resource::FlushAll;
		#ifdef CLIENT
			GlobalEvents::terrainFlush << [](){Grid::ResourceDiscovery::client.FlushAllData();};
		#endif
	}






	/*static*/ Resource::coverage_t			Resource::Sample(const String&resourceName, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		foreach (all,res)
			if (res->name.equalsIgnoreCase(resourceName))
				return res->coverage[stackID];
		FATAL__("Unable to locate resource '"+resourceName+'"');	//may be disabled later when resources are fully dynamic
		return 0;
	}
	/*static*/ Resource::coverage_t			Resource::Sample(ConstName resourceName, index_t stackID)
	{
		ASSERT_LESS__(stackID, ARRAYSIZE(((Resource*)nullptr)->coverage));
		foreach (all,res)
			if (res->cname == resourceName)
				return res->coverage[stackID];
		FATAL__("Unable to locate resource #"+String(int(resourceName)));	//may be disabled later when resources are fully dynamic
		return 0;
	}

	/*static*/ Resource&		Resource::Require(const String&name)
	{
		foreach (all,res)
			if (res->name.equalsIgnoreCase(name))
				return *res;

		FATAL__("Unable to locate resource '"+name+'"');
		return all.First();
	}

	/*static*/ Resource&		Resource::Require(ConstName name)
	{
		foreach (all,res)
			if (res->cname == name)
				return *res;

		FATAL__("Unable to locate resource #"+String(int(name)));
		return all.First();
	}


	///*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, const TVec3<>&seed,float rarity, float maxYield)
	//{
	//	// low.cells[i].centerVertex.xyz,
	//	Resource&rc = Resource::all.Append();
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
			coverage_t rs (Round(sqr(sqr(sqr(std::max(0.f,(random.NextFloat()-q)/p)))) * (max-1)));
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
//			t *= smoothstep(0.2f,0.3f,t2);
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
//			coverage_t rs (Round(/*sqr*/(/*sqr*/(/*sqr*/(clamp((t-q)/p *h,0,1)))) * (max-1)));
//			if (rs)
//				rs++;
//			return rs;
		#endif
	}


	///*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color)
	//{
	//	// low.cells[i].centerVertex.xyz,
	//	Resource&rc = Resource::all.Append();
	//	rc.Name(name,cname,color);
	//}

	void Resource::FillInternal(Random&random)
	{
		FillInternal(random,probability[resourceIndex],maximum[resourceIndex], density[resourceIndex]);
	}

	void Resource::FillInternal(Random&random,float p0, coverage_t max, float avgPerCell)
	{
		SimplexNoise n(random.NextSeed());
		//avgPerCell *= UnitScale;

		for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
		{
			#ifndef CLIENT
				coverage[i] = Resource::GetRandomCoverage(n, Grid::low.cells[i].centerVertex.xyz,  p0,max,1.f);
			#else
				coverage[i] = 0;
			#endif
		}

		#ifndef CLIENT
			if (p0 <= 0.f || !max)
				return;

			LogEvent(EventClass::Preset::uncommon,nullptr,"Equalizing resource distribution of "+name);
			count_t familyTotal[BaseGrid::NumFamilies] = {0}, total = 0;

			for (index_t i = 0; i < LowSetup::GridType::CellCount; i++)
			{
				familyTotal[Grid::low.grid.GetCell(i).family] += coverage[i];
				total += coverage[i];
			}

			//float avgPerCell = float(total) / float(LowSetup::GridType::CellCount);
			//LogEvent(EventClass::Preset::uncommon,nullptr,"Average per cell estimated at "+String(avgPerCell));


//			float avgPerCell = float(total) / float(LowSetup::GridType::CellCount);
			for (int k = 0; k < BaseGrid::NumFamilies; k++)
			{
				float p = 1.f;
				LogEvent(EventSignificance::Debug,nullptr,"Processing family "+String(k));
				const count_t count = Grid::low.grid.CountFamilyMembers(k);
				const index_t offset = Grid::low.grid.GetFamilyOffset(k);
				float want = avgPerCell * count;
				count_t retries = 0;
				while (want * 1.5f < familyTotal[k] || want * 0.75f > familyTotal[k])
				{
					//if (!(retries %3))
					{
						float factor = want / float(std::max((count_t)1,familyTotal[k]));
						float magnitude = log(factor);
						factor = pow(4.f,float(sign(factor-1.f))/(retries*2 + 1));
						p *= factor;
						p = clamp(p,0.0001f,10000.f);
						LogEvent(EventSignificance::Debug,nullptr," Family found out of range ("+String(want)+"<-"+String(float(std::max((count_t)1,familyTotal[k])))
									+"), p adjusted by "+String(factor)+" to "+String(p)+". Randomizing...");
					}
					//else
					//	LogEvent(EventSignificance::Debug,nullptr," Family found out of range ("+String(want)+"<-"+String(float(std::max((count_t)1,familyTotal[k])))
					//				+"). Randomizing...");
	
					//if (factor > 0)
					//	p *= 1.1f;
					//else
					//	p *= 1.f / 1.1f;
//					factor = 1.f + clamp(magnitude,-0.5f,2.f);
					//clamp((factor-1.f)*0.4f + 1.f,0.25f,4.f);
	//				p *= factor;
					familyTotal[k] = 0;
					for (index_t i = offset; i < offset + count; i++)
					{
						coverage[i] = Resource::GetRandomCoverage(n, Grid::low.cells[i].centerVertex.xyz,  p0,max, p);
						familyTotal[k]+=coverage[i];
					}
					if (++retries > 10)
					{
						LogEvent(EventSignificance::Debug,nullptr," Max retries reached for this family. Giving up");
						break;
					}
				}

			}


		#endif



	}

	/*static*/ void Resource::Create(const String&name, ConstName cname, const float3&color, index_t resIndex)
	{
		ASSERT_LESS__(resIndex, Objects::Resource::NumberOfPossibilities);
		Resource&rc = Resource::all.Append();
		rc.Name(name,cname,color);
		rc.resourceIndex = resIndex;
	}


	#ifdef CLIENT
		void		UpdateResourceMap(ConstName resourceName,const ArrayData<TCellResourceCoverage>&data, ResourceDiscovery&setSensor)
		{
			foreach (Resource::all, res)
				if (res->GetCName() == resourceName)
				{
					foreach (data,el)
					{
						ASSERT_LESS__(el->cellIndex,ARRAYSIZE(res->coverage));
						res->coverage[el->cellIndex] = el->coverage;
						setSensor.resourcesDiscovered[el->cellIndex] = true;
						setSensor.familyChanged[low.grid.GetCell(el->cellIndex).family] = true;
					}
					return;
				}
			FATAL__("Unable to locate resource type #"+String((int)resourceName));
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


	void		ResourceDiscovery::Update(const Grid::LCellIdI&center,float range, BasicBuffer<UINT32>&outNewDiscoveries)
	{
		static IndexSet covered;
		static Buffer0<UINT32>	coverage;
		static Queue<UINT32>	stacks;
		covered.Clear();
		coverage.Clear();
		stacks.Clear();
		outNewDiscoveries.Clear();

		stacks << center.stackID;

		const TVec3<> worldCenter = Grid::ToWorld(center);

		UINT32 stackID;
		while (stacks >> stackID)
		{
			const TVec3<> world = Grid::ToWorld(Grid::LCellIdI(stackID,center.height));
			if (Vec::quadraticDistance(world,worldCenter) > sqr(range))
				continue;
			if (!covered.IsSet(stackID))
			{
				covered.Set(stackID);
				coverage << stackID;
			}
			else
				continue;

			const auto&n = Grid::low.grid.GetCell(stackID).neighborhood;
			for (BYTE k = 0; k < n.numNeighbors; k++)
				stacks << n.neighborID[k];
		}
			
		foreach (coverage,cov)
		{
			if (resourcesDiscovered[*cov])
				continue;
			resourcesDiscovered[*cov] = true;
			outNewDiscoveries << *cov;
		}
	}

}

