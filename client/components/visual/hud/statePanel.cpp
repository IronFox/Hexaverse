#include "statePanel.h"

#include "resourceGauges.h"
#include <roundNumber.h>
#include <runtimeConfig.h>
#include "../../configurationMode.h"
#include <objects/vehicle.h>
#include <objects/structure/tower.h>
#include <objects/structure/vehicleStore.h>
#include <objects/structure/mine.h>
#include <objects/structure/laboratory.h>
#include <objects/faction.h>
#include "../display.h"
#include "hexGrid.h"

namespace HUD
{


namespace StatePanel
{
	//0.0135f;

	float		OnTag(const float3&at, char tagChar, float scaleX, float scaleY)
	{
		auto type = Objects::Resource::table.GetTypeByTag(tagChar);
		if (type)
		{
			ResourceGauges::gauges[type->serialIndex].RenderIcon(tagIconSize /** scaleX*/);
		}
		else
		{
			if (tagChar == Objects::TimeResourceTag())
				ResourceGauges::timeResource.RenderIcon(tagIconSize);
			#ifndef NO_HIBERNATION
				else
					UI::trafficGauge.RenderIcon(tagIconSize);
			#endif
		}
		return tagIconSize * scaleX;
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

		float		indent = 0.f;

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

		static float	CharLen(const char c)
		{
			return textout.GetScaledWidth(StringRef(&c,1));
		}

		void		Rewrap(float width)
		{
			count_t numSpaces = 0;
			while (numSpaces < text.Length() && text.GetChar(numSpaces) == ' ')
				numSpaces++;
			indent = textout.GetScaledWidth(text.SubStringRef(0,numSpaces));

			Wrap(text.SubStringRef(numSpaces),width,CharLen,wrapped);
		}


		void		Render(float panelWidth, float lineHeight, float&at) const
		{
			//const index_t i = l - cachedLines.pointer();
			textout.SetColor(color);
			textout.MoveTo(1.f - panelWidth + indent,at);
			foreach (wrapped,str)
				textout << *str << nl;
			at -= lineHeight * std::max<count_t>(1,wrapped.Count());
		}


		template <typename T>
			void		operator+=(const T&stringLikeAppendix)
			{
				text += stringLikeAppendix;
				wrapped.Clear();
			}
	private:
		String				text;
		Array<StringRef>	wrapped;

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
		if (ConfigurationMode::IsActive())
			return;

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
			cachedLines << " x: "+Vec::toString(e.system.x.xyz)+" ("+String(Vec::length(e.system.x.xyz))+")";
			cachedLines << " y: "+Vec::toString(e.system.y.xyz)+" ("+String(Vec::length(e.system.y.xyz))+")";
			cachedLines << " z: "+Vec::toString(e.system.z.xyz)+" ("+String(Vec::length(e.system.z.xyz))+")";
			cachedLines << " w: "+Vec::toString(e.system.w.xyz)+" ("+String(Vec::length(e.system.w.xyz))+")";
			M::TVec3<> wz = e.system.w.xyz;
			Vec::normalize(wz);
			cachedLines << " w1: "+Vec::toString(wz);
		#else
			cachedLines << " Height Level: "+String(e.location.height);
		#endif

		const bool ownedByMe = e.owner == Objects::Faction::mine;	//ghosts set isOwnedByMe true if they are actually owned by the alliance, so it cannot be used here
		if (e.t2)
			cachedLines << " T2 Experience: "+String(e.t2->experience - e.t2->currentLevelAtXp)+"/"+String(e.t2->nextLevelAtXP);
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
					cachedLines << " "+lastDeployed;

					PopCurrentColor();
				}
			}

		}
		cachedLines << "";
		cachedLines << " Scanning-Range: "+e.DescribeScanningRange();
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
			if (e.IsVehicle())
			{
				const Objects::Vehicle*veh = (const Objects::Vehicle*)(m);
				cachedLines << " Turret-Overweight: "+String((int)M::Max(0,veh->turret->weight.GetValue(researchState) - floor(veh->base->GetCarryingCapacity(researchState))));
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
				cachedLines << " Attack Initiative: "+String((int)m->GetAttackInitiative())+"/"+String((int)m->turret->initiative.GetValue(researchState));
			cachedLines << " Speed: " + m->DescribeMovementStepsPerRoundExt(e.owner,m->t2 ? m->t2->speed : Objects::T2::TModifier());
			if (m->turret->CanAttack())
			{
				if (m->turret->independentAttackAndMotion)
					cachedLines << "  (ignores attacks)";
				//cachedLines << " Range: "<<m->GetAttackRange() << nl;
				//cachedLines << " Damage: "<<m->turret->damageLevel << nl;
				//cachedLines << " Ammo-Usage: "<<m->turret->ammoNeedPerShot<<nl;
				if (m->turret->horizontalAttackRange.IsFunctionOrConstNonZero())
				{
					String srange = m->turret->horizontalAttackRange.Describe(researchState);
					String sexp = m->turret->explosionRadius.Describe(researchState);
					if (m->t2)
					{
						srange += m->t2->horizontalAttackRange.Describe();
						sexp += m->t2->explosionRadius.Describe();
					}
					cachedLines << " Hrz. Range: "+srange
									+'+'+sexp;
				}
				elif (m->turret->explosionRadius.IsFunctionOrConstNonZero())
					cachedLines << " Explosion Radius: "+m->DescribeAttackExplosionRadius();
				cachedLines << " Damage: "+String(m->DescribeDamageLevel());
				int ammoNeed = m->turret->GetAmmoNeedPerShot(researchState);
				if (ammoNeed > 0)
					cachedLines << " "+m->turret->ammoType.GetTagStr()+"Ammo-Usage: "+String(ammoNeed);
	
			}
			if (m->turret->CanAttack())
			{
				cachedLines << " Attacks left: "+String(int(m->CountPossibleAttacks()))+"/"+String(m->GetAttacksPerRound());
				if (m->turret->independentAttackAndMotion)
					cachedLines << "  (ignores movement)";
			}
			if (m->turret->CanDo(Objects::Component::Operation::Construct))
			{
				String opSpeed = m->DescribeOperationSpeed(Objects::Component::Operation::Construct, e.owner);
				cachedLines << " Converts "+Objects::Resource::Minerals.GetTagStr()+opSpeed+" per round";
			}
			if (m->turret->CanDo(Objects::Component::Operation::Repair))
			{
				String opSpeed = m->DescribeOperationSpeed(Objects::Component::Operation::Repair, e.owner);
				cachedLines << " Repairs "+Objects::Resource::Minerals.GetTagStr()+opSpeed+" per round";
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
						cachedLines.Last() += " (x"+String(t->foci.Count())+')';
					if (t->job && t->job->continuationsToJobCompletion)
					{
						int rounds = ceil(m->ContinuationsToRounds(t->job->continuationsToJobCompletion,t->job->GetOperationAffinity()));
						cachedLines.Last() += " ("+String(rounds)+" round"+(rounds != 1 ? "s)" : ")");
					}
				}
				PopCurrentColor();
			}
			#ifndef ONE_COMPOUND_PER_FACTION
				const Objects::Resource::Vector max = m->GetMaxStorageCapacity();
				bool any = false;
				for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
					if (max.level[i] > 0)
						any = true;
				if (any)
				{
	
					cachedLines << "Resources in storage:";
					for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
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
				cachedLines << " Attack Initiative: "+String((int)t->GetAttackInitiative());
				if (t->turret->CanAttack())
				{
					const Objects::Tower*v = t;	//rename
					cachedLines << " Hrz. Range: "+t->DescribeFlatAttackRangeExt(e.owner,t->t2 ? t->t2->horizontalAttackRange : Objects::T2::TModifier());
					cachedLines << " Damage: "+t->turret->damageLevel.Describe(e.GetResearchState());
					cachedLines << " "+v->turret->ammoType.GetTagStr()+"Ammo-Usage: "+String(v->turret->ammoNeedPerShot.Describe(e.GetResearchState()));
				}
				if (t->turret->CanAttack())
					cachedLines << " Attacks left: "+String(int(t->CountPossibleAttacks()))+"/"+String(t->turret->attacksPerRound.Describe(e.GetResearchState()));
				//cachedLines << " Tasks:" << nl;
	
				//cachedLines << "Resources in storage:" << nl;
				//const Objects::Resource::Vector max = t->GetMaxStorageCapacity();
				//for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
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
						cachedLines.Last() += " (?)";
					PopCurrentColor();
				}
				cachedLines << "  Resource consumption:";
				cachedLines << "    (demand/supply)";
				for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
				{
					PushCurrentColor();
					if (ownedByMe && network.total.demand[i] > network.total.supply[i])
						currentColor.rgb &= float3(1, 0.2, 0.2);
					cachedLines << "   " + Objects::Resource::ID(i).GetTagStr()+ " " +String(network.total.demand[i])+  "/" +String( network.total.supply[i] );
					PopCurrentColor();
				}
				cachedLines << "  Resource storage:";
				for (index_t i = 0; i < Objects::Resource::table.GetSize(); i++)
				{
					if (network.total.storageMax[i] > 0)
						cachedLines << "   " +Objects::Resource::ID(i).GetTagStr() + " " + String(network.total.storage[i]) + "/" + String( network.total.storageMax[i]);
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
							cachedLines.Last() += " (x"+String(t->attackQueue.Count())+')';
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
								cachedLines.Last() += " (?)";
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
				Objects::Resource::Vector	delta,storage,storageMax;
				ws->GetResourceVector(delta,storage,storageMax);
	
				const bool isEnabled = ws->IsEnabled();
				if (delta.AnyIsNegative())
				{
					cachedLines << " Consumes";
					if (!isEnabled)
						cachedLines.Last() += " (if enabled)";
					cachedLines.Last() +=':';
					if (!isEnabled)
						cachedLines << "    (demand/compound supply)";
						
	
					for (index_t i = 0; i < delta.GetSize(); i++)
					{
						const Objects::Resource::ID r{i};
						if (delta[i] < 0)
						{
							//cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<-delta.level[i];
							if (isEnabled)
								cachedLines << "  "+r.GetTagStr()+" "+String(-delta[i]);
							else
							{
								PushCurrentColor();
								if (net && delta[i] + net->total.balance[i] < 0)
									currentColor.rgb &= float3(1,0.6,0.5);
								cachedLines << "  "+r.GetTagStr()+" "+String(-delta[i]);
									
								if (net)
								{
									cachedLines.Last() += " / " + String( net->total.balance[i] );
									if (net->total.storage[i])
										cachedLines.Last() += " (" + String(net->total.storage[i])+ " stored)";
								}
								//cachedLines << nl;
								PopCurrentColor();
							}
						}
					}
				}
				const bool anyPositive = delta.AnyIsNonZeroPositive();
				const Objects::Laboratory*lab = dynamic_cast<const Objects::Laboratory*>(ws);

				if (anyPositive || lab)
				{
					cachedLines << " Produces";
					bool isEnabled = ws->IsEnabled();
					if (!isEnabled)
						cachedLines.Last() += " (if enabled)";
					cachedLines.Last() +=':';
					if (lab)
						cachedLines << "  Research: "+String(lab->GetTotalResearchOutput());
				}
				if (anyPositive)
				{
					//cachedLines << " Produces:"<<nl;
					for (index_t i = 0; i < delta.GetSize(); i++)
						if (delta[i] > 0)
							cachedLines << "  "+Objects::Resource::ID(i).GetTagStr()+" "+String(delta[i]);
				}
				if (storageMax.AnyIsNonZeroPositive())
				{
					cachedLines << " Storage:";
					for (index_t i = 0; i < storageMax.GetSize(); i++)
						if (storageMax[i] > 0)
							cachedLines << "  "+Objects::Resource::ID(i).GetTagStr()+" "+String(storage[i])+"/"+String(storageMax[i]);
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
		//textout.Scale(UI::GetScale());
		const float len = GetPanelWidth();
		//if (len == 0)
			//len = textout.GetScaledWidth("012345678901234567890123456789012345");

		const float h = textout.GetScaledHeight();

		float at = UI::GetScale() > 1.f ? 1.f - HexGrid::GetGridConfig().stepY * 0.8f : 0.95f;

		if (doRewrap)
		{
			doRewrap = false;
			foreach (cachedLines,line)
				line->Rewrap(len);
		}


		foreach (cachedLines,l)
		{
			l->Render(len,h,at);
		}

		textout.PopState();
	
	}

	void		Create()
	{
		GlobalEvents::uiScaleChanged << [](){doRewrap = true;};
		GlobalEvents::resourceTableUpdated << SignalChange;
	}

}
}
