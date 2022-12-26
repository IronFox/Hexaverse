#include "markers.h"
#include "interface.h"
#include "prototypes.h"
#include "../runtimeConfig.h"
#include "vehicle.h"
#include "structure.h"
#include "faction.h"
#include "systems.h"

namespace Objects
{
	
	#ifdef CLIENT
		/*static*/ MarkerStore	MarkerStore::client;
	#endif

	/*static*/ UINT32					EntityDeathMarker::outdatedAboveAge = 3;
		

	void					TMarkerArtifacts::ForeachMobileGhost(const std::function<void(const PMobileEntity&)>&func, IncludeGhosts include) const
	{
		switch (include)
		{
			case IncludeGhosts::All:
				deadMobileGhosts.Foreach(func);
			case IncludeGhosts::HostileContacts:
				livingMobileGhosts.Foreach(func);
			break;
			case IncludeGhosts::EntityDeaths:
				deadMobileGhosts.Foreach(func);
			break;
		}
	}

	void					TMarkerArtifacts::ForeachStructureGhost(const std::function<void(const PStructure&)>&func, IncludeGhosts include) const
	{
		switch (include)
		{
			case IncludeGhosts::All:
				deadStructureGhosts.Foreach(func);
			case IncludeGhosts::HostileContacts:
				livingStructureGhosts.Foreach(func);
			break;
			case IncludeGhosts::EntityDeaths:
				deadStructureGhosts.Foreach(func);
			break;
		}
	}

	void					TMarkerArtifacts::ForeachGhost(const std::function<void(const PWorldEntity&)>&func, IncludeGhosts include) const
	{
		switch (include)
		{
			case IncludeGhosts::All:
				deadMobileGhosts.Foreach(func);
				deadStructureGhosts.Foreach(func);
			case IncludeGhosts::HostileContacts:
				livingMobileGhosts.Foreach(func);
				livingStructureGhosts.Foreach(func);
			break;
			case IncludeGhosts::EntityDeaths:
				deadMobileGhosts.Foreach(func);
				deadStructureGhosts.Foreach(func);
			break;
		}

	}


	/*static*/ PMarker Marker::Instantiate(MarkerType type)
	{
		PMarker m;
		static_assert(MarkerType::N == 4, "Expand switch in Marker::Instantiate");
		switch (type)
		{
			case MarkerType::EntityDeath:
				m.reset(new EntityDeathMarker());
				break;
			case MarkerType::HostileContact:
				m.reset(new HostileContactMarker());
				break;
			case MarkerType::Visited:
				m.reset(new VisitedMarker());
				break;
			case MarkerType::Alliance:
				m.reset(new AllianceMarker());
				break;
		}
		ASSERT_EQUAL1__(m->IsPrivate(), TypeIsPrivate(type),"Marker privacy consistency violation");
		return m;
	}

	/*static*/ bool		Marker::TypeIsPrivate(MarkerType type)
	{
		static_assert(MarkerType::N == 4, "Expand switch in Marker::TypeIsPrivate");
		switch (type)
		{
			case MarkerType::EntityDeath:
			case MarkerType::HostileContact:
			case MarkerType::Visited:
				return true;
			case MarkerType::Alliance:
				return false;
		}
		FATAL__("Unexpected marker type "+String(type.ToString())+" ("+String(type)+")");
		return true;
	}



	Marker::Marker(MarkerType type):Super(TEmpty()),type(type),createdInRound(RuntimeConfig::roundNumber)
	{}

	
	/*virtual*/ void Marker::UpdateAge(const RuntimeConfig::RoundNumber& currentRoundNumber)
	{
		age = currentRoundNumber - createdInRound;
		ASSERT_LESS__(age,10000000);	//overflow otherwise, 10000000 seems a good limit
	}

	
	/*virtual override*/ void EntityMarker::UpdateAge(const RuntimeConfig::RoundNumber& currentRoundNumber)
	{
		Super::UpdateAge(currentRoundNumber);
		#ifdef CLIENT
			if (visualization)
				visualization->health = -(int)(age);
		#endif
	}


	/*virtual override*/ void				EntityMarker::Destroy()
	{
		#ifdef CLIENT
			#ifdef HAS_WINDOW
				ApplicationInterface::OnVanish(visualization,RemovalType::Vanished,true);
			#endif
			visualization.reset();
		#endif
	}

	/*virtual override*/ Marker::Outdated			EntityDeathMarker::CheckOutdated() const
	{
		return age <= outdatedAboveAge ? Outdated::Not : Outdated::Yes;
	}


	#ifdef CLIENT
		/*virtual override*/ void				EntityDeathMarker::Register(TMarkerArtifacts&a)
		{
			Super::Register(a);

			if (visualization)
			{
				if (visualization->IsMobile())
					a.deadMobileGhosts << To<MobileEntity>::Cast(visualization);
				elif (visualization->IsStructure())
					a.deadStructureGhosts << To<Structure>::Cast(visualization);
				else
					LogEvent(EventSignificance::NonFatalError,"Unable to classify ghost for '"+visualization->GetName()+"'");
			}
		}

		/*virtual override*/ void				EntityDeathMarker::Unregister(TMarkerArtifacts&a)
		{
			if (visualization)
			{
				if (visualization->IsMobile())
					a.deadMobileGhosts.FindAndErase(visualization);
				elif (visualization->IsStructure())
					a.deadStructureGhosts.FindAndErase(visualization);
				else
					LogEvent(EventSignificance::NonFatalError,"Unable to classify ghost for '"+visualization->GetName()+"'");
			}


			Super::Unregister(a);
		}

		/*virtual override*/ void				HostileContactMarker::Register(TMarkerArtifacts&a)
		{
			Super::Register(a);

			if (visualization->IsMobile())
				a.livingMobileGhosts << To<MobileEntity>::Cast(visualization);
			elif (visualization->IsStructure())
				a.livingStructureGhosts << To<Structure>::Cast(visualization);
			else
				LogEvent(EventSignificance::NonFatalError,"Unable to classify ghost for '"+visualization->GetName()+"'");
		}

		/*virtual override*/ void				HostileContactMarker::Unregister(TMarkerArtifacts&a)
		{
			if (visualization->IsMobile())
				a.livingMobileGhosts.FindAndErase(visualization);
			elif (visualization->IsStructure())
				a.livingStructureGhosts.FindAndErase(visualization);
			else
				LogEvent(EventSignificance::NonFatalError,"Unable to classify ghost for '"+visualization->GetName()+"'");


			Super::Unregister(a);
		}
	#endif



	bool				MarkerStore::GetOutdated(BasicVector<PMarker>&outOutdated, const RuntimeConfig::RoundNumber& currentRoundNumber)
	{
		outOutdated.Clear();
		bool fail = false;
		all.Foreach([currentRoundNumber,&fail,&outOutdated](const PMarker&m)
		{
			if (fail)
				return;
			m->UpdateAge(currentRoundNumber);
			switch (m->CheckOutdated())
			{
				case Marker::Outdated::Fail:
					LogEvent(EventSignificance::NonFatalError,nullptr,"Marker "+m->GetName()+" failed outdated check. Aborting scan");
					fail = true;
					outOutdated.Clear();
					break;
				case Marker::Outdated::Yes:
					LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered outdated after "+String(m->age)+" round(s)");
					outOutdated << m;
					break;
			}

			/*
			switch (m->type)
			{
				case Type::EntityDeaths:
						
					if (m->age > deathOutdatedAfterRounds)
					{
						LogEvent(EventSignificance::Debug,nullptr,"Marker "+m->GetName()+" considered outdated after "+String(m->age)+" round(s)");

						outOutdated << *m;
					}
				break;
				case Type::HostileContact:
				{
					if (m->age < 2 || !m->visualization) //don't delete hostile markers in round < 2: grid sensory might be outdated
						continue;
					if (World::actual.SensoryIsInvalid())
					{
						//it's actually outdated and known to be outdated
						outOutdated.Clear();
						return false;
					}

					if (World::actual.GridCanSee(m->location,m->visualization->GetHeight(),m->visualization->IsBroad()))	//I see you. Or should, anyway, but you're gone
						outOutdated << *m;
				}
				break;

			}
			*/

		});

		return !fail;
	}

	PMarker				MarkerStore::Create(MarkerType type, const ArrayRef<BYTE>&data, bool fromSavegame, const std::function<bool(const PMarker&)>&check)
	{
		MemReadStream stream(data.GetPointer(),data.GetContentSize());
		return Create(type,stream,fromSavegame, check);
	}


	PMarker				MarkerStore::Create(MarkerType type, D::IReadStream&s, bool fromSavegame, const std::function<bool(const PMarker&)>&check)
	{
		LogEvent(EventSignificance::Common,"Creating marker of type "+String(type.ToString()));
		PMarker m = Marker::Instantiate(type);


		if (fromSavegame)
			m->FromSavegame(s);
		else
			m->Unpack(s);

		if (check && !check(m))
			return nullptr;


		all << m;
		typed.Set(type) << m;


		doesOutdate.Set(type) = m->CanOutdateInTime();

		if (m->IsEntityMarker())
			referenceIDMap.Set(m->GetReferencedEntityID(),m);
		idMap.Set(m->id,m);
		m->UpdateAge(RuntimeConfig::roundNumber);
		Objects::BuildStructureSystem(m->location,m->system);

		#ifdef CLIENT
			m->UpdateNiceName();
		#endif

		#ifdef CLIENT
			m->Register(clientArtifacts);
		#endif

		return m;
	}

	PMarker				MarkerStore::Destroy(const TUniqueID&id)
	{
		PMarker m;
		if (!idMap.QueryAndUnset(id, m))
			return nullptr;

		#ifdef CLIENT
			m->Unregister(clientArtifacts);
		#endif
		m->Destroy();

		all.FindAndErase(m);
		typed[m->type].FindAndErase(m);
		if (m->IsEntityMarker())
			referenceIDMap.Unset(m->GetReferencedEntityID());
		return m;
	}

	PMarker			MarkerStore::FindByLocation(const Grid::LCellIdI&loc)	const
	{
		return all.Scan([loc](const PMarker&m)
		{
			return m->GetLocation() == loc;
		});
	}

	PMarker			MarkerStore::FindByLocation(MarkerType type, const Grid::LCellIdI&loc)	const
	{
		auto list = typed.QueryPointer(type);
		if (!list)
			return nullptr;
		return list->Scan([loc](const PMarker&m)
		{
			return m->GetLocation() == loc;
		});
	}

	PMarker			MarkerStore::FindByLocation(MarkerType type, const TUniqueID& ownerFactionID, const Grid::LCellIdI&loc)	const
	{
		auto list = typed.QueryPointer(type);
		if (!list)
			return nullptr;
		return list->Scan([loc, ownerFactionID](const PMarker&m)
		{
			return m->GetLocation() == loc && m->ownerFactionID == ownerFactionID;
		});
	}


	PMarker			MarkerStore::FindByReference(const TUniqueID&referenceID)	const
	{
		PMarker m;
		if (referenceIDMap.Query(referenceID, m))
			return m;
		return nullptr;
	}

	PMarker			MarkerStore::FindByID(const TUniqueID&entityID) const
	{
		PMarker m;
		if (idMap.Query(entityID, m))
			return m;
		return nullptr;
	}

	void				MarkerStore::GetDeletionCandidates(BasicVector<TCandidate>&candidates) const
	{
		candidates.Clear();


		all.Foreach([&candidates](const PMarker&m)
		{
			candidates.Append().markerID = m->id;
			candidates.Last().priority = m->age;
		});

		std::sort(candidates.begin(),candidates.end());
		candidates.Truncate(MaxMarkersPerFaction / 10);
	}

	void				MarkerStore::Clear()
	{
		all.Clear();
		typed.Clear();
		idMap.Clear();
		referenceIDMap.Clear();
		doesOutdate.Clear();
		#ifdef CLIENT
			clientArtifacts.Clear();
		#endif
	}

	void				MarkerStore::SignalFactionColorChange(const Objects::PFaction&f)
	{
		#ifdef CLIENT
			auto list = typed.QueryPointer(MarkerType::Alliance);
			if (!list)
				return;

			list->Foreach([this,f](const PMarker&m)
			{
				PAllianceMarker am = To<AllianceMarker>::Cast(m);
				if (am->ownerFactionID == f->id)
				{
					index_t idx = clientArtifacts.allianceDefs.GetIndexOf(am->allianceDef);
					if (idx != InvalidIndex)
					{
						clientArtifacts.allianceDefs[idx].color = AllianceMarker::GetColorOf(f);
					}
				}
			});
		#endif
	}



	String						EntityMarker::DescribeEntity() const
	{
		if (refEntityID.IsEmpty())
			return "-";
		return "e"+ToString(refEntityID)+" ("+ToString(refEntityType)+")";
	}

	


	/*virtual override*/	void		Marker::ToSavegame(IWriteStream&s) const
	{
		s.WritePrimitive(id);
		s.WritePrimitive(createdInRound);
		s.WritePrimitive(location);
	}

	/*virtual override*/	void		EntityMarker::ToSavegame(IWriteStream&s) const
	{
		Super::ToSavegame(s);
		s.WritePrimitive(this->refEntityID);
		s.WritePrimitive(this->refEntityOwnerID);
		using Serialization::Serialize;
		Serialize(s,ToString(refEntityType));
	}


	/*virtual override*/	void		Marker::FromSavegame(IReadStream&inStream)
	{
		inStream.ReadPrimitive(id);
		#ifndef CLIENT
			if (ApplicationInterface::globalVersionOfCurrentFile < ApplicationInterface::FileVersions::MarkersAreSerialized)
			{
				UINT8 typei;
				inStream.ReadPrimitive(typei);
				if (type != Type::Reinterpret(typei))
					throw Except::IO::DriveAccess::FileFormatFault("Misinterpreted contained marker type. Local instance is "+String(type.ToString())+", but file stream says "+String(Type::Reinterpret(typei))+" ("+String(typei)+")");
			}
		#endif
		inStream.ReadPrimitive(createdInRound);
		inStream.ReadPrimitive(location);
	}

	/*virtual override*/	void		EntityMarker::FromSavegame(IReadStream&inStream)
	{
		Super::FromSavegame(inStream);
		String t0;
		inStream.ReadPrimitive(this->refEntityID);

		#ifndef CLIENT
			if (ApplicationInterface::globalVersionOfCurrentFile >= ApplicationInterface::FileVersions::MarkersHaveOwner)
			{
				inStream.ReadPrimitive(this->refEntityOwnerID);
			}
			else
		#endif
		this->refEntityOwnerID = TUniqueID::Empty();
		using Serialization::Deserialize;
		Deserialize(inStream,t0);
		this->refEntityType = DecodeType(t0);
		//this->referenceEntityType.Deserialize(inStream, fixedSize);
	}


	/*static*/ float4 AllianceMarker::GetColorOf(const PFaction&f)
	{
		return float4(1.f - (1.f - f->color) * 0.5f,1);
		//return float4(f->activeEntityColor,1);
	}

	
	#ifdef CLIENT
		/*virtual override*/ void			AllianceMarker::Register(TMarkerArtifacts&a)
		{
			ASSERT__(!allianceDef.owner);
			PFaction f = Faction::FindByID(ownerFactionID);
			allianceDef.owner = f;
			ASSERT__(f);
			allianceDef.marker = shared_from_this();


			auto&def = a.allianceDefs.Append(allianceDef);

			def.color = AllianceMarker::GetColorOf(f);
			def.system = system;
			def.labelCenter = system.w.xyz + system.z.xyz * 2.f;
		}

		/*virtual override*/ void			AllianceMarker::Unregister(TMarkerArtifacts&a)
		{
			a.allianceDefs.FindAndErase(allianceDef);
			allianceDef = TAllianceDef();
		}

	#endif

	/*virtual override*/	void				AllianceMarker::Pack(D::IWriteStream&s,Target target) const
	{
		Super::Pack(s,target);
		using Serialization::Serialize;
		Serialize(s,text);
	}

	/*virtual override*/	void				AllianceMarker::Unpack(D::IReadStream&s)
	{
		Super::Unpack(s);
		using Serialization::Deserialize;
		Deserialize(s,text);
	}

	/*virtual override*/	void				AllianceMarker::ToSavegame(D::IWriteStream&s) const
	{
		Super::ToSavegame(s);
		using Serialization::Serialize;
		Serialize(s,text);
	}

	/*virtual override*/	void				AllianceMarker::FromSavegame(D::IReadStream&s)
	{
		Super::FromSavegame(s);
		using Serialization::Deserialize;
		Deserialize(s,text);
	}


	

	/*virtual override*/	void		Marker::Pack(IWriteStream&s, Target) const
	{
		s.WritePrimitive(id);
		s.WritePrimitive(ownerFactionID);
		s.WritePrimitive(createdInRound);
		s.WritePrimitive(location);
	}

	/*virtual override*/	void		EntityMarker::Pack(IWriteStream&s, Target t) const
	{
		Super::Pack(s, t);
		s.WritePrimitive(this->refEntityID);
		s.WritePrimitive(this->refEntityOwnerID);
		using Serialization::Serialize;
		Serialize(s,ToString(refEntityType));
	}


	/*virtual override*/	void		Marker::Unpack(IReadStream&inStream)
	{
		inStream.ReadPrimitive(id);
		inStream.ReadPrimitive(ownerFactionID);
		inStream.ReadPrimitive(createdInRound);
		inStream.ReadPrimitive(location);
	}

	/*virtual override*/	void		EntityMarker::Unpack(IReadStream&inStream)
	{
		Super::Unpack(inStream);
		String t0;
		inStream.ReadPrimitive(this->refEntityID);
		inStream.ReadPrimitive(this->refEntityOwnerID);
		using Serialization::Deserialize;
		Deserialize(inStream,t0);
		this->refEntityType = DecodeType(t0);


		#ifdef CLIENT

			PWorldEntity prototype = Objects::Prototypes::all.Find(refEntityType);
			if (prototype)
			{
				ASSERT1__(prototype, refEntityType);
				visualization = (prototype->CreateNewInstance(OwnerParameter(), location, 0));
			}
			else
			{
				//vehicle
				if (refEntityType.HasTurret())
				{
					Component::VehicleBase* base = Component::baseComponents.Find(refEntityType.baseType);
					Component::Turret* turret = Component::vehicleTurretComponents.Find(refEntityType.turretType);
					ASSERT1__(base, refEntityType);
					ASSERT1__(turret, refEntityType);
					visualization.reset(new Vehicle(OwnerParameter(), location, base, turret));
				}
				else
					LogEvent(EventSignificance::NonFatalError,"Unable to create ghost for type "+refEntityType.baseType.ToNumericString());
			}

			if (visualization)
			{
				visualization->isOwnedByMe = (visualization->owner && visualization->owner->isOwnedByMe) 
						|| (!visualization->owner && DefaultOwnedByMe());
				visualization->UpdateNiceName();

				if (visualization->IsMobile())
					To<MobileEntity>::Cast(visualization)->UpdateSystem();
				visualization->id = refEntityID;
				if (refEntityOwnerID.IsNotEmpty())
					visualization->owner = Faction::GetOrCreate(refEntityOwnerID);
				else
					LogEvent(EventSignificance::NonFatalError,"Ghost entity has no owner: "+ToString(refEntityID));

			}

		#endif



	}

}

