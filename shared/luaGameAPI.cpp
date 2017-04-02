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

