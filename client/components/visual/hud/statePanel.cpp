#include "statePanel.h"

#include "resourceGauges.h"
#include <roundNumber.h>
#include <runtimeConfig.h>
#include "../../configurationMode.h"
#include <luaInterface.h>
#include <objects/vehicle.h>
#include <objects/structure/tower.h>
#include <objects/structure/vehicleStore.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <objects/faction.h>
#include "../display.h"

namespace HUD
{


namespace StatePanel
{
	//0.0135f;

	float		OnTag(const float3&at, char tagChar, float scaleX, float scaleY)
	{
//		float2 at = float2(textout.state.left+0.015,textout.state.top - textout.GetScaledHeight() * (float(textout.state.line)-0.5f));

		index_t i = tagChar - tagOffset;
//		float2 p = at.xy;
		if (i < Objects::Resource::NumberOfPossibilities/* UI::gauges.Count()*/)
		{
			ResourceGauges::gauges[i].RenderIcon(tagIconSize /** scaleX*/);
		}
		else
		{
			if (i - Objects::Resource::NumberOfPossibilities == 0)
				ResourceGauges::timeResource.RenderIcon(tagIconSize);
			#ifndef NO_HIBERNATION
				else
					UI::trafficGauge.RenderIcon(tagIconSize);
			#endif
		}
		return tagIconSize * scaleX;
		//;*2.f/display.GetPixelAspect();
	}


	static RoundNumber cachedNumber = RoundNumber(-1);
	static const Objects::WorldEntity *cachedEntity = nullptr;

	static float4	currentColor;

	static Buffer0<float4>	pushedColors;
	static void PushCurrentColor()
	{
		pushedColors << currentColor;
		ASSERT_LESS__(pushedColors.Count(),3);
	}
	static void PopCurrentColor()
	{
		ASSERT__(pushedColors.Count() != 0);
		currentColor = pushedColors.Pop();
	}



	struct TCachedLine
	{
		float4		color;
		String		text;

		float		indent = 0.f;
		Array<String>	wrapped;

		/**/		TCachedLine()	{}
		/**/		TCachedLine(const String&str):text(str),color(currentColor){}
		/**/		TCachedLine(const StringRef&str):text(str),color(currentColor){}
		/**/		TCachedLine(String&&str):text(std::move(str)),color(currentColor){}
		/**/		TCachedLine(const char*str):text(str),color(currentColor){}

		void		swap(TCachedLine&other)
		{
			swp(color,other.color);
			text.swap(other.text);
			swp(indent,other.indent);
			wrapped.swap(other.wrapped);
		}

		static float	CharLen(char c)
		{
			return textout.GetScaledWidth(StringRef(&c,1));
		}

		void		Rewrap(float width)
		{
			count_t numSpaces = 0;
			while (numSpaces < text.Length() && text.GetChar(numSpaces) == ' ')
				numSpaces++;
			indent = textout.GetScaledWidth(text.SubStringRef(0,numSpaces));

			wrapf(text.c_str() + numSpaces,width,CharLen,wrapped);
		}
	};
	static Buffer0<TCachedLine,Swap>	cachedLines;
	static bool doRewrap = false;

	void		SignalChange()
	{
		cachedEntity = nullptr;
	}


	void		UpdateCacheIfNeeded(const Objects::WorldEntity&e)
	{
		if (cachedNumber == RuntimeConfig::roundNumber && cachedEntity == &e)
			return;

		const Objects::MobileEntity *const m = e.IsMobile() ? (const Objects::MobileEntity*)(&e) : nullptr;


		cachedNumber = RuntimeConfig::roundNumber;
		cachedEntity = &e;
		cachedLines.Clear();
		const auto&researchState = e.GetResearchState();

		currentColor = float4(1);
		#ifndef NO_HIBERNATION
			const Objects::Hibernating*h = e.ToHibernating();
		#endif
			

		auto base = e.GetTurretBase();
		const String&name= e.GetNiceName();
		if (name.GetLength() < 20)
			cachedLines << name;
		else
		{
			index_t split = name.Find('\'');
			if (split == InvalidIndex)
				split = name.Find(' ');
			cachedLines << name.subStringRef(0,split);
			cachedLines << String(' ')+name.SubStringRef(split);
				
			#ifdef _DEBUG
				cachedLines << ' '+e.GetName();

			#endif

		}

		#ifdef _DEBUG
			cachedLines << " Location: "+ToString(e.location);
			if (e.IsMobile())
				cachedLines << " Last point of origin: "+ToString(((const Objects::MobileEntity&)e).GetLastPointOfOrigin());
			cachedLines << " x: "+Vec::toString(e.system.x.xyz);
			cachedLines << " y: "+Vec::toString(e.system.y.xyz);
			cachedLines << " z: "+Vec::toString(e.system.z.xyz);
			cachedLines << " w: "+Vec::toString(e.system.w.xyz);
			M::TVec3<> wz = e.system.w.xyz;
			Vec::normalize(wz);
			cachedLines << " w1: "+Vec::toString(wz);
		#else
			cachedLines << " Height: "+String(e.location.height);
		#endif

		const bool ownedByMe = e.owner == Objects::Faction::mine;	//ghosts set isOwnedByMe true if they are actually owned by the alliance, so it cannot be used here
		cachedLines << " Owner: "+e.owner->GetName()+(ownedByMe ? " (me)":"");
		if (e.isOwnedByMe)
		{
			if (e.IsAlive())
			{
				static const Objects::WorldEntity *last=nullptr;
				static String lastDeployed;
				if (last != &e)
				{
					last = &e;
					lastDeployed = e.CompileDeploymentCount(e.owner);
				}
				if (lastDeployed.IsNotEmpty())
				{
					PushCurrentColor();
					currentColor.rgb = float3(0.6,0.7,1.f);
					cachedLines << lastDeployed;

					PopCurrentColor();
				}
			}

		}
		cachedLines << "";
		cachedLines << " Scanning-Range: "+e.DescribeScanningRange();
		if (base && base->turret->subOrbitalScanningRange.IsFunctionOrConstNonZero())
			cachedLines << " Sub-Orbital Scanning-Range: "+base->DescribeSubOrbitalScanningRange();
		#ifndef NO_HIBERNATION
			cachedLines << " "<<Objects::TrafficResourceTag()  <<e.GetUplinkTraffic() << nl;
			if (h)
				cachedLines << " "<<Objects::TrafficResourceTag()  << "Priority: "<<h->GetUplinkPriority()<<nl;
		#endif
		PushCurrentColor();
		if (e.IsAlive())
		{
			if (e.IsIndestructible())
				cachedLines << " Indestructible";
			else
			{
				int maxHealth = e.GetMaxHealth();
				if (e.health < maxHealth/4)
					currentColor.rgb &= float3(1,0.2,0.2);
				elif (e.health < maxHealth/2)
					currentColor.rgb &= float3(1,1,0.2);
				elif (e.health < maxHealth)
					currentColor.rgb &= float3(0.2,1,0.2);
	
				cachedLines << " Health: " + String(e.health) + "/" + e.DescribeMaxHealth();
			}
		}
		else
		{
			currentColor.rgb &= float3(1, 0.8, 0.75);
			cachedLines << " Ghost created " + String( -e.health )+ " round(s) ago";
		}
		PopCurrentColor();
		#ifndef NO_HIBERNATION
			if (h->IsHibernating())
			{
				color = float3(0.5f);
				cachedLines.SetColor(0.6,1,1);
				cachedLines << " Hibernating" << nl;
				cachedLines.SetColor(color);
			}
		#endif
		if (e.empDamageLevel)
		{
			PushCurrentColor();
			currentColor.rgb &= float3(1,0.8,0.75);
			cachedLines << " EMP-Damage detected: "+String(e.empDamageLevel);
			PopCurrentColor();
		}
	
	
		if (m)
		{
	
			//if (!v->storedIn.expired())
				//cachedLines << " Stored in: "<<v->storedIn.lock()->GetName()<<nl;	//probably never happens
			if (e.IsGroundBoundVehicle())
			{
				const Objects::Vehicle*veh = (const Objects::Vehicle*)(m);
				cachedLines << " Turret-Overweight: "+String((int)M::Max(0,veh->turret->weight(e.owner) - floor(veh->base->GetCarryingCapacity(e.owner))));
			}
			cachedLines << " Movement Initiative: "+String((int)m->GetMovementInitiative())+"/"+String((int)m->GetBaseMovementInitiative());
			#ifndef NO_SATELLITES
				elif (e.IsSatellite())
				{
					Objects::Satellite*sat = (Objects::Satellite*)(&e);
					int demand = sat->GetFuelConsumptionPerRound();
					if (!sat->RequiresFuel())
						demand = 0;
					cachedLines << " Fuel-Consumption: "<<(char)(tagOffset+Objects::Resource::Fuel)<<demand;
					if (demand > 0)
						cachedLines <<'+'<<sat->GetExtraFuelDemand();
							
					cachedLines<<nl;
				}
			#endif
			if (m->turret->CanAttack())
				cachedLines << " Attack Initiative: "+String((int)m->GetAttackInitiative())+"/"+String((int)m->turret->initiative);
			cachedLines << " Speed: " + m->DescribeMovementStepsPerRound();
			if (m->turret->CanAttack())
			{
				if (m->turret->independentAttackAndMotion)
					cachedLines << "  (ignores attacks)";
				//cachedLines << " Range: "<<m->GetAttackRange() << nl;
				//cachedLines << " Damage: "<<m->turret->damageLevel << nl;
				//cachedLines << " Ammo-Usage: "<<m->turret->ammoNeedPerShot<<nl;
				if (m->turret->horizontalAttackRange.IsFunctionOrConstNonZero())
				{
					cachedLines << " Hrz. Range: "+m->turret->horizontalAttackRange.Describe(researchState)
									+'+'+m->turret->explosionRadius.Describe(researchState);
				}
				elif (m->turret->explosionRadius.IsFunctionOrConstNonZero())
					cachedLines << " Explosion Radius: "+m->turret->explosionRadius.Describe(researchState);
				cachedLines << " Damage: "+String(m->turret->GetDamageLevel(e.owner));
				int ammoNeed = m->turret->GetAmmoNeedPerShot(e.owner);
				if (ammoNeed > 0)
					cachedLines << " "+String((char)(tagOffset+m->turret->ammoType))+"Ammo-Usage: "+String(ammoNeed);
	
			}
			if (m->turret->CanAttack())
			{
				cachedLines << " Attacks left: "+String(int(m->CountPossibleAttacks()))+"/"+String(m->GetAttacksPerRound());
				if (m->turret->independentAttackAndMotion)
					cachedLines << "  (ignores movement)";
			}
			if (m->turret->CanDo(Objects::Component::Construct))
			{
				String opSpeed = m->turret->operationSpeed[Objects::Component::Construct].Describe(m->GetResearchState());
				cachedLines << " Converts "+String(Objects::ResourceTag(Objects::Resource::Minerals))+opSpeed+" per round";
			}
			if (m->turret->CanDo(Objects::Component::Repair))
			{
				String opSpeed = m->turret->operationSpeed[Objects::Component::Repair].Describe(m->GetResearchState());
				cachedLines << " Repairs "+String(Objects::ResourceTag(Objects::Resource::Minerals))+opSpeed+" per round";
			}
	
			cachedLines << " Tasks:";
			if (m->myTasks.IsEmpty())
			{
				if (m->unstable)
					cachedLines << "  Operation done. Relocate to complete";
				else
					cachedLines << "  idle";
			}
			else
			{
				PushCurrentColor();
				currentColor.rgb &= float3(0.75,0.8,1);
				foreach (m->myTasks,t)
				{
					cachedLines << "  * "+t->name;
					if (t->IsMode(Objects::CursorMode::AttackMode) && t->foci.Count() > 1)
						cachedLines.Last().text+= " (x"+String(t->foci.Count())+')';
					if (t->job && t->job->continuationsToJobCompletion)
					{
						int rounds = ceil(m->ContinuationsToRounds(t->job->continuationsToJobCompletion,t->job->GetOperationAffinity()));
						cachedLines.Last().text+= " ("+String(rounds)+" round"+(rounds != 1 ? "s)" : ")");
					}
				}
				PopCurrentColor();
			}
			#ifndef ONE_COMPOUND_PER_FACTION
				const Objects::ResourceVector max = m->GetMaxStorageCapacity();
				bool any = false;
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					if (max.level[i] > 0)
						any = true;
				if (any)
				{
	
					cachedLines << "Resources in storage:";
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
						if (max.level[i] > 0)
						{
	
	//						cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<m->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(m->currentStorage.level[i])+"/"+String(max.level[i]);
							//cachedLines << "        : "<<m->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
						}
				}
			#endif
		}
	
		bool storeListed = false;
	
		if (e.IsStructure())
		{
			#ifdef _DEBUG
			{
				cachedLines << " Connected to: ";
				const Objects::Structure&s = (const Objects::Structure&)e;
				foreach (s.connectedTo,c)
					cachedLines << "  "+c->lock()->GetName();
			}
			#endif
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
			if (t)
			{
				cachedLines << " Attack Initiative: "+String((int)t->turret->initiative);
				if (t->turret->CanAttack())
				{
					const Objects::Tower*v = t;	//rename
					cachedLines << " Hrz. Range: "+t->DescribeFlatAttackRange();
					cachedLines << " Damage: "+t->turret->damageLevel.Describe(e.GetResearchState());
					cachedLines << " "+String((char)(tagOffset+v->turret->ammoType))+"Ammo-Usage: "+String(v->turret->ammoNeedPerShot.Describe(e.GetResearchState()));
				}
				if (t->turret->CanAttack())
					cachedLines << " Attacks left: "+String(int(t->CountPossibleAttacks()))+"/"+String(t->turret->attacksPerRound.Describe(e.GetResearchState()));
				//cachedLines << " Tasks:" << nl;
	
				//cachedLines << "Resources in storage:" << nl;
				//const Objects::ResourceVector max = t->GetMaxStorageCapacity();
				//for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				//	if (max.level[i] > 0)
				//		cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<t->storage.level[i]<<"/"<<max.level[i]<<nl;
			
			}
	
	
			const Objects::VehicleStore*st = dynamic_cast<const Objects::VehicleStore*>(&e);
			if (st)
			{
				storeListed = true;
				cachedLines << " Max Vehicles per Store: "+String(Objects::VehicleStore::MaxVehiclesPerStore);
	
	
				if (st->park)
				{
					cachedLines << " Vehicles stored in park ("+String(st->park->vehiclesStored)+"/"+String(st->GetParkVehicleCapacity())+"):";
					PushCurrentColor();
					currentColor.rgb &= float3(0.8f,0.9f,1.1f);
					count_t cnt = 0;
	
					{
						const Objects::VehicleStore *other = st;
						for (const auto&v : other->stored)
						{
							Objects::PMobileEntity v1 = v.entity.lock();
							ASSERT1__(v1,st->ToEntity().id);
							if (++cnt > 10)
							{
								cachedLines << "...";
								break;
							}
							cachedLines << "  "+v1->GetNiceName() +" (local)";
						}
					}
	
					currentColor.rgb &= float3(0.9f,0.9f,0.9f);
	
					for (const auto&_s : st->park->members)
					{
						Objects::PVehicleStore other = _s.lock();
						if (other.get() == st)
							continue;
						for (const auto&v : other->stored)
						{
							Objects::PMobileEntity v1 = v.entity.lock();
							ASSERT1__(v1,st->ToEntity().id);
							if (++cnt > 10)
							{
								cachedLines << "...";
								break;
							}
							cachedLines << "  "+v1->GetNiceName();
						}
						if (cnt > 10)
							break;
					}
					PopCurrentColor();
				}
				else
				{
					cachedLines << " Vehicles stored ("+String(st->stored.Count())+"/"+String(Objects::VehicleStore::MaxVehiclesPerStore)+"):";
					for (const auto&v : st->stored)
					{
						Objects::PMobileEntity v1 = v.entity.lock();
						ASSERT1__(v1,st->ToEntity().id);
						cachedLines << "  "+v1->GetNiceName();
					}
				}
			}
	
	
			const Objects::Structure*s = (const Objects::Structure*)(&e);
	
			#if defined _DEBUG && !defined ONE_COMPOUND_PER_FACTION
				cachedLines << " Compound ID: "+String(s->compoundID);
			#endif
			const Objects::Compound*net = nullptr;
			if (s->IsAlive())
			{
				const Objects::Compound&network = s->GetCompound();
				net = &network;
				cachedLines << " Compound status:";
				if (!network.stable)
				{
					PushCurrentColor();
					if (ownedByMe)
						currentColor.rgb &= float3(1, 0.2, 0.2);
	
					cachedLines << "  UNSTABLE";
					if (!ownedByMe)
						cachedLines.Last().text += " (?)";
					PopCurrentColor();
				}
				cachedLines << "  Resource consumption:";
				cachedLines << "    (demand/supply)";
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				{
					PushCurrentColor();
					if (ownedByMe && network.total.demand.level[i] > network.total.supply.level[i])
						currentColor.rgb &= float3(1, 0.2, 0.2);
					cachedLines << "   " + String((char)(tagOffset+i))+ " " +String(network.total.demand.level[i])+  "/" +String( network.total.supply.level[i] );
					PopCurrentColor();
				}
				cachedLines << "  Resource storage:";
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				{
					if (network.total.storageMax.level[i] > 0)
						cachedLines << "   " +String( (char)(tagOffset+i) ) + " " + String(network.total.storage.level[i]) + "/" + String( network.total.storageMax.level[i]);
				}
			}
			else
			{
				//cachedLines << " Ghost created in round "<<
			}
	
			{
				const Objects::MobileEntityProducer*ps = e.CastToProducer();
				if (ps)
				{
					//const Objects::VehiclePlant*ps = (const Objects::VehiclePlant*)(&e);
					bool active = true;
					if (e.IsWorkingStructure())
					{
						active = ((const Objects::WorkingStructure*)(&e))->IsEnabled();
					}
							
					if (ps->queue.IsNotEmpty())
					{
						cachedLines << "  producing "+ps->queue.First().name;
						if (!active)
							cachedLines << "   completion (when enabled) in "+String(Objects::TimeResourceTag())+String(ps->productionRoundsLeft);
						else
							cachedLines << "   completion in "+String(Objects::TimeResourceTag())+String(ps->productionRoundsLeft);
					}
	
				}
				else
				{
	
	
				}
			}
	
			if (e.IsWorkingStructure())
			{
				const Objects::WorkingStructure*ws = (const Objects::WorkingStructure*)(&e);
				cachedLines << "";
				cachedLines << " Structure status:";
				if (ws->IsHibernating())
					cachedLines << "  Hibernating";
	
				if (ws->AwaitsUserInteraction())
					cachedLines << "  Awaiting user input";
				else
					cachedLines << (ws->IsEnabled() ? "  Enabled" : "  Disabled");
					

				if (ws->IsTower())
				{
					const auto*t = dynamic_cast<const Objects::Tower*>(ws);
					cachedLines << " Tasks:";
					if (t->attackQueue.IsEmpty())
					{
						cachedLines << "  Idle";
					}
					else
					{
						PushCurrentColor();
						currentColor.rgb &= float3(0.75,0.8,1);
						cachedLines << "  * Attack";
						if (t->attackQueue.Count() > 1)
							cachedLines.Last().text+= " (x"+String(t->attackQueue.Count())+')';
						PopCurrentColor();
					}
				}
	
	
				{
							
					if (ws->IsMine())
					{
						const Objects::Mine*ms = (const Objects::Mine*)(ws);
						cachedLines << "  Efficiency: "+String(  int(ms->GetEfficiency()*100) )+ "%";
						if (ms->IsLinkedToCore())
							cachedLines << "  Linked to Core" ;
						else
						{
							PushCurrentColor();
							if (ownedByMe)
								currentColor.rgb &= float3(1,0.5,0.4);
							cachedLines << "  Disconnected from Core";
							if (!ownedByMe)
								cachedLines.Last().text += " (?)";
							PopCurrentColor();
						}
					}
					else
					{
						const Objects::NexusPort*np = dynamic_cast<const Objects::NexusPort*>(&e);
						if (np)
						{
							if (np->owner->IsAlliedWithMe())
							{
								PushCurrentColor();
									currentColor.rgb &= float3(1,0.5,0.4);
									if (np->resistance)
										cachedLines << "  Remaining resistance: "+String(np->resistance);
									else
										cachedLines << "  Captured";
								PopCurrentColor();
							}
						}
					}
				}
				Objects::ResourceVector	delta,storage,storageMax;
				ws->GetResourceVector(delta,storage,storageMax);
	
				bool any = false;
				for (index_t i = 0; i < delta.NumEntries; i++)
					if (delta.level[i] < 0)
						any = true;
				const bool isEnabled = ws->IsEnabled();
				if (any)
				{
					cachedLines << " Consumes";
					if (!isEnabled)
						cachedLines.Last().text += " (if enabled)";
					cachedLines.Last().text +=':';
					if (!isEnabled)
						cachedLines << "    (demand/compound supply)";
						
	
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] < 0)
						{
							//cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<-delta.level[i];
							if (isEnabled)
								cachedLines << "  "+String((char)(tagOffset+i))+" "+String(-delta.level[i]);
							else
							{
								PushCurrentColor();
								if (net && delta.level[i] + net->total.balance.level[i] < 0)
									currentColor.rgb &= float3(1,0.6,0.5);
								cachedLines << "  "+String((char)(tagOffset+i))+" "+String(-delta.level[i]);
									
								if (net)
								{
									cachedLines.Last().text += " / " + String( net->total.balance.level[i] );
									if (net->total.storage.level[i])
										cachedLines.Last().text += " (" + String(net->total.storage.level[i])+ " stored)";
								}
								//cachedLines << nl;
								PopCurrentColor();
	
							}
						}
				}
				any = false;
				for (index_t i = 0; i < delta.NumEntries; i++)
					if (delta.level[i] > 0)
						any = true;
				const Objects::Laboratory*lab = dynamic_cast<const Objects::Laboratory*>(ws);

				if (any || lab)
				{
					cachedLines << " Produces";
					bool isEnabled = ws->IsEnabled();
					if (!isEnabled)
						cachedLines.Last().text += " (if enabled)";
					cachedLines.Last().text +=':';
					if (lab)
						cachedLines << "  Research: "+String(lab->GetTotalResearchOutput());
				}
				if (any)
				{
					//cachedLines << " Produces:"<<nl;
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] > 0)
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(delta.level[i]);
				}
				any = false;
				for (index_t i = 0; i < delta.NumEntries; i++)
					if (storageMax.level[i] > 0)
						any = true;
				if (any)
				{
					cachedLines << " Storage:";
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (storageMax.level[i] > 0)
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(storage.level[i])+"/"+String(storageMax.level[i]);
				}
			}
	
	
		
		}
	
	
		if (!storeListed && e.IsHangar())
		{
			const Objects::Hangar&h = *dynamic_cast<const Objects::Hangar*>(&e);
			PushCurrentColor();
			currentColor.rgb &= float3(0.75,0.8,1);
			cachedLines << " Units in storage ("+String(h.stored.Count())+"/"+String(h.GetMaxStorage())+"):";
			for (const auto&v : h.stored)
			{
				Objects::PMobileEntity v1 = v.entity.lock();
				ASSERT1__(v1,h.ToEntity().id);
				cachedLines << "  "+v1->GetNiceName();
			}
			PopCurrentColor();
	
		}

		doRewrap = true;

	}

	float		GetPanelWidth()
	{
		return 300.f / display.GetClientWidth();
	}

	void		Render(const Objects::WorldEntity&e)
	{
		if (!showControlsAndContacts)
			return;
		UpdateCacheIfNeeded(e);
			
		//textout.GetActiveFont()->EndTagMode();
		textout.PushState();

		float len = GetPanelWidth();
		//if (len == 0)
			//len = textout.GetScaledWidth("012345678901234567890123456789012345");

		const float h = textout.GetScaledHeight();
		float at = 0.95f;

		if (doRewrap)
		{
			doRewrap = false;
			const float w = GetPanelWidth();
			foreach (cachedLines,line)
				line->Rewrap(w);
		}


		foreach (cachedLines,l)
		{
			const index_t i = l - cachedLines.pointer();
			textout.SetColor(l->color);
			textout.MoveTo(1.f - len + l->indent,at);
			foreach (l->wrapped,str)
				textout << *str << nl;
			at -= h * std::max<count_t>(1,l->wrapped.Count());
		}

		textout.PopState();
	
	}


}
}
