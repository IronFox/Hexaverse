#include <global_root.h>
#include "luaGameAPI.h"
#include "logEvent.h"
#include "sphereGrid.h"
#include "objects.h"

namespace Lua
{
	namespace Detail
	{
		void LogLua(const String&msg)
		{
			LogEvent(EventClass::Preset::important,"Lua: "+msg);
		}

		#ifdef CLIENT

			int	GetCompoundResourceChange(const String&resourceName)
			{
				Objects::Resource resource;
				if (!resource.Decode(resourceName.c_str()))
					throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
				#ifndef ONE_COMPOUND_PER_FACTION
					return 0;
				#else
					return Objects::Faction::mine->compound.total.supply.level[resource] - Objects::Faction::mine->compound.total.demand.level[resource];
				#endif
			}

			int GetCompoundResourceStorage(const String&resourceName)
			{
				Objects::Resource resource;
				if (!resource.Decode(resourceName.c_str()))
					throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
				#ifndef ONE_COMPOUND_PER_FACTION
					return 0;
				#else
					return Objects::Faction::mine->compound.total.storage.level[resource];
				#endif
			}

			int FindHighestAvailableResourceLocation(const String&resourceName)
			{
				foreach (Grid::Resource::all,r)
					if (r->GetName() == resourceName)
					{
						int highestAt = -1;
						Grid::Resource::coverage_t highest = 0;
						for (index_t i = 0; i < Grid::Resource::NumEntries; i++)
							if (r->known[i])
							{
								if (r->coverage[i] > highest)
								{
									if (Objects::FindApplicableGround(Grid::LCellIdI(i,0),true) != Objects::badHeight)
									{
										highest = r->coverage[i];
										highestAt = (int)i;
									}
								}
							}
						return highestAt;
					}
				throw Lua::Exception("Resource '"+resourceName+"' does not exist.");
			}

			int GetResourceCoverage(const String&resourceName, int stackID)
			{
				if (stackID < 0 || stackID >= Grid::LowSetup::GridType::CellCount)
					throw Lua::Exception("GetResourceCoverage(): Invalid location ("+String(stackID)+").");
				foreach (Grid::Resource::all,r)
					if (r->GetName() == resourceName)
					{
						if (!r->known[stackID])
							return -1;
						return r->coverage[stackID];
					}
				throw Lua::Exception("GetResourceCoverage(): Resource '"+resourceName+"' does not exist.");
			}
		#endif
		
		Lua::Reference ToTable(Lua::PProgram&p, const Grid::LCellIdI&cell)
		{
			Lua::Table c = p->CreateTable();

			c.SetField("height" ,cell.height);
			c.SetField("stackID" ,cell.stackID);

			return c;
		}

		Lua::Reference ToTable(Lua::PProgram&p, const Objects::MobileEntity::Task&task)
		{
			Lua::Table taskOut = p->CreateTable();
			taskOut.SetField("name",task.name);
			taskOut.SetField("isJob",task.IsJob());

			Lua::Table foci = p->CreateTable();
			foreach (task.foci,f)
				foci.AppendElement(Detail::ToTable(p,f->target));
			taskOut.SetField("foci",foci);

			return taskOut;
		}

		Lua::Table ToTable(Lua::PProgram&p, const Objects::TurretBase&b, const Lua::Reference&researchState)
		{

			Lua::Table t = p->CreateTable();
			//b.
			t.SetField("typeName",b.turret->typeName);
			t.SetField("weight",b.turret->weight.GetValue(researchState));

			t.SetField("canAttack",b.turret->CanAttack());
			if (b.turret->CanAttack())
			{
				t.SetField("trajectoryType",b.turret->trajectoryType.ToString());
				t.SetField("ammoType",b.turret->ammoType.ToString());
				t.SetField("possibleAttacks",b.CountPossibleAttacks());
				t.SetField("attackActionLeft",b.GetAttackActionLeft());
				t.SetField("attackInitiative",b.GetAttackInitiative());
				t.SetField("attacksPerRound",b.GetAttacksPerRound());
				t.SetField("flatAttackRange",b.GetFlatAttackRange());
			}

			for (index_t op = 0; op < Objects::Component::OperationCount; op++)
			{
				const auto o = (Objects::Component::operation_t)op;
				const String os = String(Objects::Component::OperationToString(o));
				t.SetField("can"+os,b.turret->CanDo(o));
				t.SetField("speedOf"+os,b.turret->operationSpeed[o].GetValue(researchState));
			}

			return t;
		}
	}


	void DefineGeneralAPI(Lua::Table&api)
	{
		api.SetFunction("Log",Detail::LogLua);
		#ifdef CLIENT
			api.SetFunction("FindHighestAvailableResourceLocation",Detail::FindHighestAvailableResourceLocation);
			api.SetFunction("GetResourceCoverage",Detail::GetResourceCoverage);
			api.SetFunction("GetCompoundResourceChange",Detail::GetCompoundResourceChange);
			api.SetFunction("GetCompoundResourceStorage",Detail::GetCompoundResourceStorage);
		#endif

		#ifdef LOWER_RESOLUTION_PLANET
			api.SetField("LowerResolutionPlanet",true);
		#else
			api.SetField("LowerResolutionPlanet",false);
		#endif
		api.SetField("UnitScale",Grid::UnitScale);
	}

	Lua::Table CreateGeneralAPI(const Lua::PProgram&p)
	{
		Lua::Table api = p->CreateTable();
		DefineGeneralAPI(api);
		api.SetGlobal("Game");
		return api;
	}



	void		EntityToTable(const Objects::PWorldEntity&e, Lua::Table&t)
	{
		Lua::PProgram p = t.GetProgram();
		if (!p)
			return;

		t.SetField("name",e->GetName());
		t.SetField("niceName",e->GetNiceName());
		t.SetField("id",e->id.ToFullString());
		t.SetField("typeName",e->GetTypeName());
		t.SetField("isMobile",e->IsMobile());
		t.SetField("isStructure",e->IsStructure());
		#ifdef CLIENT
			t.SetField("isOwnedByMe",e->isOwnedByMe);
		#endif

		t.SetField("location",Detail::ToTable(p,e->location));
		t.SetField("isSleeping",e->IsSleeping());
		t.SetField("health",e->health);
		t.SetField("maxHealth",e->GetMaxHealth());
		t.SetField("empDamageLevel",e->empDamageLevel);
		t.SetField("scanningRange",e->GetScanningRange());
		if (e->IsWorkingStructure())
		{
			t.SetField("isWorkingStructure",true);
			t.SetField("isEnabled",To<Objects::WorkingStructure>::Cast(e)->IsEnabled());
			t.SetFunction("Enable", (std::function<void(bool)>) [e](bool doEnable)
			{
				#ifdef CLIENT
					ApplicationInterface::SignalEnableStructure(*To<Objects::WorkingStructure>::Cast(e),doEnable);
				#else
					float t= 0;
					To<Objects::WorkingStructure>::Cast(e)->SetEnabled(doEnable, t);
				#endif
			});
		}
		else
			t.SetField("isWorkingStructure",false);

		auto prod = e->CastToProducer();
		t.SetField("isMobileEntityProducer",prod != nullptr);
		if (prod)
		{
			Lua::Table queue = p->CreateTable();
			foreach (prod->queue,w)
			{
				queue.AppendElement(w->GetName());
			}
			t.SetField("productionQueue",queue);
		}


		float zOffset;
		const auto base = e->GetTurretBase(zOffset);
		if (base)
		{
			Lua::Table sub = Detail::ToTable(p, *base, e->GetResearchState());
			sub.SetField("zOffset",zOffset);
			t.SetField("turretBase",sub);
		}



		if (e->IsMobile())
		{
			Lua::Table tasks = p->CreateTable();
			const auto&m = To<Objects::MobileEntity>::Cast(e);
			t.SetField("isAutomated",m->isAutomated);
			foreach (m->myTasks,task)
			{
				tasks.AppendElement(Detail::ToTable(p,*task));
			}
			t.SetField("tasks",tasks);
		}
	}

}

