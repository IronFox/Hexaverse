#if 0

#include <global_root.h>
#include "research.h"

namespace Objects
{



	/*static*/ ResearchState::GlobalState		ResearchState::globalState;

	
	index_t		ResearchState::GlobalState::Register(ConstName cname,Component::Base* componentReference, WorldEntity*prototype, const String&name)
	{
		if (componentReference)
		{
			if (!prototype && componentReference->IsTurret() && ((Component::Turret*)componentReference)->mountType==Component::Turret::MountType::Satellite)
			{
				#ifndef NO_SATELLITES
					foreach (satellitePrototypes,sat)
						if ((*sat)->turret == componentReference)
						{
							prototype = sat->get();
							break;
						}
				#endif
				if (!prototype)
					return InvalidIndex;
			}

			if (componentReference->IsBase() && !Component::baseComponents.Contains(componentReference))
				return InvalidIndex;
			if (componentReference->IsTurret() && !Component::allTurretComponents.Contains(componentReference))
				return InvalidIndex;
		}
		if (prototype)
		{
			if (prototype->IsStructure())
			{
				bool found = false;
				foreach (Component::Terraformer::canBuild,cb)
					if (cb->get() == prototype)
					{
						found = true;
						break;
					}
				if (!found)
					return InvalidIndex;
			}
			#ifndef NO_SATELLITES
				else
					if (prototype->IsSatellite())
					{
						if (!SkyHub::CanProduce((const Satellite*)prototype))
							return InvalidIndex;
					}
			#endif
		}

		if (typeMap[(index_t)cname] != InvalidIndex)
			return typeMap[(index_t)cname];
		index_t rs = types.Count();
		typeMap[(index_t)cname] = rs;
		auto&t = types.Append();
		t.name = name;
		t.targetName = cname;
		t.prototypeReference = prototype;
		t.componentReference = componentReference;

		if (componentReference)
			componentReference->researchSlot[0] = rs;
		return rs;
	}

	/*static*/ void		ResearchState::StaticCreate()
	{
		globalState.researchProgram = Lua::Program::Create();
		globalState.researchProgram->ExecuteFile("../lua/research.lua");
		Lua::Table researchTable = globalState.researchProgram->FindGlobal("ResearchState");
		globalState.stateConstructor = researchTable.GetField("New");



		//using namespace ResearchStateDetail;
		globalState.typeMap.Fill(InvalidIndex);
	}

	/*static*/ void		ResearchState::RegisterUnlockCost(Structure*s, int researchCost)
	{
		index_t at = globalState.Register(s->GetCTypeName(),nullptr,s,s->GetTypeName());
		if (at == InvalidIndex)
			return;
		globalState.types[at].researchCost = researchCost;
		globalState.types[at].unlockableAt = globalState.numUnlockables++;

	}

	/*static*/ void		ResearchState::RegisterResearch(Structure*s, Component::Attribute a)
	{
		index_t at = globalState.Register(s->GetCTypeName(),nullptr,s,s->GetTypeName());
		if (at == InvalidIndex)
			return;
		globalState.types[at].attributeAt[a] = globalState.attributeCounter;
		globalState.attributeCounter++;
	}

	#ifndef NO_SATELLITES
		/*static*/ void		ResearchState::RegisterResearch(Satellite*s, Component::Attribute a)
		{
			index_t at = globalState.Register(s->GetCTypeName(),nullptr,s,s->GetTypeName());
			if (at == InvalidIndex)
				return;
			globalState.types[at].attributeAt[a] = globalState.attributeCounter;
			globalState.attributeCounter++;
		}
	#endif


	/*static*/ void		ResearchState::RegisterUnlockCost(Component::Base&component, int researchCost)
	{
		index_t at = component.researchSlot[0];
		if (at == InvalidIndex)
		{
			at = globalState.Register(component.constName,&component,nullptr,component.typeName);
			if (at == InvalidIndex)
				return;
		}
		globalState.types[at].researchCost = researchCost;
		globalState.types[at].unlockableAt = globalState.numUnlockables++;
	}

	/*static*/ void		ResearchState::RegisterResearch(Component::Base&component, Component::Attribute a)
	{
		index_t at = component.researchSlot[0];
		if (at == InvalidIndex)
		{
			at = globalState.Register(component.constName,&component,nullptr,component.typeName);
			if (at == InvalidIndex)
				return;
		}
		globalState.types[at].attributeAt[a] = component.researchSlot[a+1] = globalState.attributeCounter;
		globalState.attributeCounter++;
	}

	/*static*/ void		ResearchState::RegisterDependency(Component::Base&component, Component::Base&dependentOn)
	{
		index_t at = component.researchSlot[0];
		if (at == InvalidIndex)
		{
			at = globalState.Register(component.constName,&component,nullptr,component.typeName);
			if (at == InvalidIndex)
				return;
		}
		globalState.types[at].dependentOn = dependentOn.constName;
	}

	/*static*/ void		ResearchState::RegisterDependency(Component::Base&component, const PStructure&dependentOn)
	{
		index_t at = component.researchSlot[0];
		if (at == InvalidIndex)
		{
			at = globalState.Register(component.constName,&component,nullptr,component.typeName);
			if (at == InvalidIndex)
				return;
		}
		globalState.types[at].dependentOn = dependentOn->GetCTypeName();
	}

	/**/			ResearchState::ResearchState()
	{
		ASSERT__(globalState.attributeCounter > 0);
		unlockProgress.SetSize(globalState.numUnlockables);
		unlockProgress.Fill(0);
		improveProgress.SetSize(globalState.attributeCounter);
		improveProgress.Fill(0);
	}

	/**/			ResearchState::ResearchState(const TUniqueID&parentID,bool isNexus)
	{
		ASSERT__(globalState.attributeCounter > 0);
		ownerID = parentID;
		unlockProgress.SetSize(globalState.numUnlockables);
		unlockProgress.Fill(isNexus ? 0xFFFFFFFF : 0);
		improveProgress.SetSize(globalState.attributeCounter);
		improveProgress.Fill(0);
	}

	bool			ResearchState::CheckUnlocked(const Component::Base&b,String&errorOut)	const
	{
		if (b.researchSlot[0] == InvalidIndex)
			return true;
		const auto&t = globalState.types[b.researchSlot[0]];
		index_t at = t.unlockableAt;
		if (at == InvalidIndex)
			return true;
		if (unlockProgress[at] >= t.researchCost)
			return true;
		errorOut = t.name+" has not been unlocked yet. Research progress: "+String(unlockProgress[at])+"/"+String(t.researchCost);
		return false;
	}


	bool			ResearchState::CheckUnlocked(ConstName name,String&errorOut)	const
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
			return true;
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		index_t at = t.unlockableAt;
		if (at == InvalidIndex)
			return true;
		if (unlockProgress[at] >= t.researchCost)
			return true;
		errorOut = t.name+" has not been unlocked yet. Research progress: "+String(unlockProgress[at])+"/"+String(t.researchCost);
		return false;
	}

	UINT32			ResearchState::GetResearchOf(ConstName name, Component::Attribute attrib)	const
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
		{
			return 0;
		}
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		index_t at = t.attributeAt[attrib];
		if (at == InvalidIndex)
		{
			return 0;
		}
		return improveProgress[at];
	}

	void			ResearchState::GetResearchOf(ConstName name, UINT32&current, UINT32&max)	const
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
		{
			current = 0;
			max = 0;
			return;
		}
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		index_t at = t.unlockableAt;
		if (at == InvalidIndex)
		{
			current = 0;
			max = 0;
			return;
		}
		current = unlockProgress[at];
		max = t.researchCost;
	}


	bool			ResearchState::IsUnlocked(ConstName name) const
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
			return true;
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		index_t at = t.unlockableAt;
		if (at == InvalidIndex)
			return true;
		if (unlockProgress[at] >= t.researchCost)
			return true;
		return false;
	}


	/*static*/ String		ResearchState::_Describe(float fc, float baseValue, ModulationType type/*=ModulationType::DefaultFloat*/, ExtraModulationFunction mod/*=ExtraModulationFunction()*/)
	{
		String rs(mod ? mod(baseValue) : baseValue);
		if (fc != 1.f)
		{
			if (type == ModulationType::InvertedFloat || type == ModulationType::InvertedCeilInt)
				fc = 1.f / fc;

			float modulated = fc * baseValue;
			if (mod)
				modulated = mod(modulated);
			if (type == ModulationType::FloorInt)
				modulated = floor(modulated);
			else
				if (type == ModulationType::CeilInt || type == ModulationType::InvertedCeilInt)
					modulated = ceil(modulated);
				else
					modulated = round(modulated * 10)/10;
			rs =  String(modulated)+ " ("+rs+"x"+String(round(fc*10)/10)+")";
		}
		return rs;
	}

	String			ResearchState::Describe(ConstName constName, float baseValue, Component::Attribute attrib, ModulationType type/*=ModulationType::DefaultFloat*/, ExtraModulationFunction mod/*=ExtraModulationFunction()*/)	const
	{
					
		float fc = GetFactorFor(constName,attrib);
		return _Describe(fc, baseValue,type,mod);
		
	}

	String			ResearchState::Describe(const Component::Base&base, float baseValue, Component::Attribute attrib, ModulationType type/*=ModulationType::DefaultFloat*/, ExtraModulationFunction mod/*=ExtraModulationFunction()*/)	const
	{
		float fc = GetFactorFor(base,attrib);
		return _Describe(fc, baseValue,type,mod);
	}



	/*static*/ float			ResearchState::GetFactor(int x)
	{
		//return log(std::max(1.f,M_EF+0.1f*(level)));
		static const float fc = 0.15f;	//level >3000 = x2, level 1 = x1.02, level 175 = x1.5
		return (x > 0 ? log((float(x)* fc+1.f))*fc : exp(float(x)* fc * fc) -1) + 1.f;
	}


	float			ResearchState::GetFactorFor(ConstName name, Component::Attribute attrib)	const
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
			return 1.f;
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		index_t at = t.attributeAt[attrib];
		if (at == InvalidIndex)
			return 1.f;
		int level = improveProgress[at];
		if (level == 0)
			return 1.f;
		return GetFactor(level);
	}


	float			ResearchState::GetFactorFor(const Component::Base&base, Component::Attribute attrib)	const
	{
		index_t at = base.researchSlot[attrib+1];
		if (at == InvalidIndex)
			return 1.f;
		int level = improveProgress[at];
		if (level == 0)
			return 1.f;
		return GetFactor(level);
	}

	/*static*/ bool		ResearchState::IsGenerallyResearchable(ConstName name)
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
			return false;
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		return t.researchCost > 0;
	}

	/*static*/ bool		ResearchState::IsGenerallyResearchable(ConstName name, Component::Attribute attrib)
	{
		if (globalState.typeMap[(index_t)name] == InvalidIndex)
			return false;
		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		return t.attributeAt[attrib] != InvalidIndex;
	}

	bool		ResearchState::IsCurrentlyResearchable(ConstName name, String*dependentOnOut/*=nullptr*/) const
	{
		if (!IsGenerallyResearchable(name))
			return false;

		const auto&t = globalState.types[globalState.typeMap[(index_t)name]];
		if (t.dependentOn != ConstName::Empty)
		{
			if (!IsUnlocked(t.dependentOn))
			{
				if (dependentOnOut)
				{
					index_t depAt = globalState.typeMap[(index_t)t.dependentOn];
					if (depAt != InvalidIndex)
						*dependentOnOut = globalState.types[depAt].name;
				}
				return false;
			}
		}
		index_t at = t.unlockableAt;
		if (at != InvalidIndex
			&& unlockProgress[at] >= t.researchCost)
			return false;
		return true;
	}

	bool		ResearchState::IsCurrentlyResearchable(ConstName name, Component::Attribute attrib) const
	{
		if (!IsUnlocked(name))
			return false;
		return IsGenerallyResearchable(name,attrib);
	}

	ResearchState::Task			ResearchState::GetCurrentTask() const
	{
		if (IsCurrentlyResearchable(currentResearch, currentResearchAttribute))
			return Task::Improve;
		if (IsCurrentlyResearchable(currentResearch))
			return Task::Unlock;
		return Task::None;
	}


	#ifdef CLIENT
		void		ResearchState::Update(const ResearchState&update, const PFaction&parent)
		{
			currentResearch = update.currentResearch;
			currentResearchAttribute = update.currentResearchAttribute;

			bool changed = GetFactorFor(currentResearch,currentResearchAttribute) != update.GetFactorFor(currentResearch,currentResearchAttribute);

			float t =0;
			Component::Turret*turret = nullptr;
			if (changed && currentResearchAttribute == Component::Attribute::ScanningRange)
			{
				turret = Component::GetTurretByName(currentResearch);
				if (turret)
				{
					for (const auto&s : structures)
					{
						if (s->owner == parent && s->IsTower())
						{
							PTower tower = To<Tower>::Cast(s);
							if (tower->turret == turret)
							{
								tower->UnregisterSensory(t,false);
							}
						}
					}
					for (const auto&m : vehicles)
					{
						if (m->owner == parent && m->turret == turret)
							m->UnregisterSensory(t,false);
					}
					#ifndef NO_SATELLITES
						for (const auto&m : satellites)
						{
							if (m->owner == parent && m->turret == turret)
								m->UnregisterSensory(m->location,t,false);
						}
					#endif
				}
				else
					for (const auto&s : structures)
					{
						if (s->GetCTypeName() == currentResearch)
							s->UnregisterSensory(t,false);
					}
			}

			unlockProgress = update.unlockProgress;
			improveProgress = update.improveProgress;


			if (changed && currentResearchAttribute == Component::Attribute::ScanningRange)
			{
				turret = Component::GetTurretByName(currentResearch);
				if (turret)
				{
					for (const auto&s : structures)
					{
						if (s->owner == parent && s->IsTower())
						{
							PTower tower = To<Tower>::Cast(s);
							if (tower->turret == turret)
							{
								tower->RegisterSensory(t,false);
							}
						}
					}
					for (const auto&m : vehicles)
					{
						if (m->owner == parent && m->turret == turret)
							m->RegisterSensory(t,false);
					}
					#ifndef NO_SATELLITES
						for (const auto&m : satellites)
						{
							if (m->owner == parent && m->turret == turret)
								m->RegisterSensory(m->location,t,false);
						}
					#endif
				}
				else
					for (const auto&s : structures)
					{
						if (s->GetCTypeName() == currentResearch)
							s->RegisterSensory(t,false);
					}
			}

		}

	#else

		void			ResearchState::Contribute()
		{
			contribution++;
		}

		void			ResearchState::FinalizeRound(const PFaction&parent)
		{
			if (!contribution)
				return;
			if (currentResearch == ConstName::Empty)
			{
				LogEvent(EventClass::Preset::error,parent,"No research goal configured. "+String(contribution)+" research point(s) lost");
				contribution = 0;
				return;
			}
			index_t tAt = globalState.typeMap[(index_t)currentResearch];
			if (tAt == InvalidIndex)
			{
				LogEvent(EventClass::Preset::error,parent,"Invalid research goal configured. "+String(contribution)+" research point(s) lost");
				contribution = 0;
				return;
			}
			index_t aAt = InvalidIndex;
			{
				const auto&t = globalState.types[tAt];
				if (t.unlockableAt != InvalidIndex)
				{
					if (unlockProgress[t.unlockableAt] < t.researchCost)
					{
						if (t.dependentOn != ConstName::Empty)
							if (!IsUnlocked(t.dependentOn))
							{
								LogEvent(EventClass::Preset::error,parent,"Research requirements not met. "+String(contribution)+" research point(s) lost");
								contribution = 0;
								return;
							}
						unlockProgress[t.unlockableAt] += contribution;
						if (unlockProgress[t.unlockableAt] >= t.researchCost)
						{
							LogEvent(EventClass(EventSignificance::Important,EventFlavor::Gain),parent,"Research completed. "+t.name+" is now available.");
							currentResearch = ConstName::Empty;
						}
						changed = true;
						contribution = 0;
						return;
					}
				}
				aAt = t.attributeAt[currentResearchAttribute];
				if (aAt == InvalidIndex)
				{
					LogEvent(EventClass::Preset::error,parent,"Invalid improvement goal configured. "+String(contribution)+" research point(s) lost");
					contribution = 0;
					return;
				}
			}
			float t = 0;

			Component::Turret*turret = nullptr;
			if (currentResearchAttribute == Component::Attribute::ScanningRange)
			{
				turret = Component::GetTurretByName(currentResearch);
				if (turret)
				{
					foreach (parent->towers,_t)
					{
						PTower tower = _t->lock();
						if (tower && tower->turret == turret)
						{
							tower->UnregisterSensory(t,false);
						}
					}
					foreach (parent->allMobiles,_m)
					{
						PMobileEntity m = _m->lock();
						if (m && m->turret == turret)
							m->UnregisterSensory(t,false);
					}
				}
				else
					foreach (parent->structures,_s)
					{
						PStructure s = _s->lock();
						if (s && s->GetCTypeName() == currentResearch)
							s->UnregisterSensory(t,false);
					}
			}

			improveProgress[aAt] +=contribution;
			changed = true;
			contribution = 0;

			if (currentResearchAttribute == Component::Attribute::ScanningRange)
			{
				t += timeEpsilon;
				if (turret)
				{
					foreach (parent->towers,_t)
					{
						PTower tower = _t->lock();
						if (tower && tower->turret == turret)
						{
							tower->RegisterSensory(t,false);
						}
					}
					foreach (parent->allMobiles,_m)
					{
						PMobileEntity m = _m->lock();
						if (m && m->turret == turret)
							m->RegisterSensory(t,false);
					}
				}
				else
					foreach (parent->structures,_s)
					{
						PStructure s = _s->lock();
						if (s && s->GetCTypeName() == currentResearch)
							s->RegisterSensory(t,false);
					}
				parent->CheckChanges(t);
			}

		}
	#endif


	/*virtual override*/	serial_size_t	ResearchState::GetSerialSize(bool export_size) const
	{
		return	sizeof(ownerID)
				+1 /*current type*/
				+1 /*current attribute*/
				+unlockProgress.GetContentSize()
				+improveProgress.GetContentSize();
	}

	/*virtual override*/	bool			ResearchState::Serialize(IWriteStream&outStream, bool export_size) const
	{
		return outStream.WritePrimitive(ownerID)
			&& outStream.WritePrimitive((BYTE)currentResearch) && outStream.WritePrimitive((BYTE)currentResearchAttribute)
			&& outStream.Write(unlockProgress.pointer(),unlockProgress.GetContentSize())
			&& outStream.Write(improveProgress.pointer(),improveProgress.GetContentSize());
	}

	/*virtual override*/	bool			ResearchState::Deserialize(IReadStream&inStream, serial_size_t fixed_size)
	{
		BYTE currentRes,currentResAttrib;
		if (	inStream.ReadPrimitive(ownerID)
			&& inStream.ReadPrimitive(currentRes) && inStream.ReadPrimitive(currentResAttrib)
			&& inStream.Read(unlockProgress.pointer(),unlockProgress.GetContentSize())
			&& inStream.Read(improveProgress.pointer(),improveProgress.GetContentSize()))
		{
			if ((count_t)currentRes >= (count_t)ConstName::NumTypes)
				return false;
			currentResearch = (ConstName)currentRes;
			return currentResearchAttribute.Load(currentResAttrib);
		}
		return false;
	}





}

#endif