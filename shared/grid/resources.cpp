#include <global_root.h>
#include "resources.h"

#include "../objects/resource.h"

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


}

