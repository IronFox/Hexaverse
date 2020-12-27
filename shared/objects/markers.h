#pragma once

#include "types.h"
#include "../sharedContainer.h"
#include "packable.h"
#include "entity.h"
#include "entityType.h"
#include "../roundNumber.h"
#include "../stringTable.h"
#include <general/enumeration.h>
#include <net/containers.h>


namespace Objects
{
	
	CONSTRUCT_ENUMERATION4(IncludeGhosts
		, None
		, HostileContacts
		, EntityDeaths
		, All
	);


	struct TMarkerArtifacts
	{
		SharedContainer<Structure>		deadStructureGhosts,livingStructureGhosts;
		SharedContainer<MobileEntity>	deadMobileGhosts,livingMobileGhosts;

		void							ForeachMobileGhost(const std::function<void(const PMobileEntity&)>&func, IncludeGhosts) const;
		void							ForeachStructureGhost(const std::function<void(const PStructure&)>&func, IncludeGhosts) const;
		void							ForeachGhost(const std::function<void(const PWorldEntity&)>&func, IncludeGhosts) const;

		void							Clear()
		{
			deadMobileGhosts.Clear();
			deadStructureGhosts.Clear();
			livingMobileGhosts.Clear();
			livingStructureGhosts.Clear();
		}

		void							swap(TMarkerArtifacts&other)
		{
			deadMobileGhosts.swap(other.deadMobileGhosts);
			deadStructureGhosts.swap(other.deadStructureGhosts);
			livingMobileGhosts.swap(other.livingMobileGhosts);
			livingStructureGhosts.swap(other.livingStructureGhosts);
		}

		friend void swap(TMarkerArtifacts&a, TMarkerArtifacts&b)
		{
			a.swap(b);
		}

	};


	CONSTRUCT_ENUMERATION4(MarkerType, 
		EntityDeath,		//friendly entity has died here. usually preserved for 2-5 rounds. AI may preserve them longer to establish statistics 
		HostileContact,		//hostile entity was detected here when radar contact was lost. Usually preserved until radar contact is re-established
		Visited,			//marker used by AI to remember previously visited area. refEntityID is either empty of the mobile that was memorized here
		Alliance			//created by users and shared among the respective alliance, server managed and preserved
		);	


	/**
	Persistent scene markers added/removed by the client.
	Markers are visible only to the owning faction, and do not hinder movement or construction.
	Multiple markers can be placed on the same location.
	*/
	class Marker : public BaseEntity, public Packable
	{
		typedef BaseEntity			Super;
	public:
		typedef MarkerType			Type;
		RoundNumber					createdInRound;
		TUniqueID					ownerFactionID;
		const Type					type;
		
		UINT32						age = 0;	//age in rounds, updated during GetOutdated()

		CONSTRUCT_ENUMERATION3(Outdated, Yes, Not, Fail);


		static PMarker				Instantiate(MarkerType type);
		static bool					TypeIsPrivate(MarkerType type);
		

		/**/						Marker(MarkerType type);



		virtual void				UpdateAge(const RoundNumber&roundNumber);


		virtual String				GetTypeName() const override { return "Marker (" + String(type.ToString())+")"; }
		virtual EntityType			GetType() const override {return ConstName::Type_Marker;}
		virtual TUniqueID			GetReferencedEntityID() const {return TUniqueID::Empty();}
		virtual bool				CanOutdateInTime() const {return false;}
		virtual Outdated			CheckOutdated() const {return Outdated::Not;}
		virtual const Grid::LCellIdI&	GetLocation() const {return location;}
		virtual bool				IsEntityMarker() const {return false;}
		/**
		Private markers are not shared with the alliance or spectators,
		and not fed into the marker creation check function.
		*/
		virtual bool				IsPrivate() const {return true;}

		#ifdef CLIENT
			virtual void			Register(TMarkerArtifacts&) {};
			virtual void			Unregister(TMarkerArtifacts&) {};
		#endif
		virtual void				Destroy() {};


		bool						operator==(const Marker&other) const
		{ 
			return id == other.id && location == other.location 
			&& type == other.type && createdInRound == other.createdInRound;
		}
		bool						operator!=(const Marker&other) const {return !operator==(other);}

		virtual void				Pack(D::IWriteStream&s,Target target) const override;
		virtual void				Unpack(D::IReadStream&s) override;
		virtual void				ToSavegame(D::IWriteStream&s) const override;
		virtual void				FromSavegame(D::IReadStream&s) override;


	};


	class EntityMarker : public Marker
	{
		typedef Marker				Super;
	public:
		TUniqueID					refEntityID = TUniqueID::Empty(),	//!< Usage depends on type
									refEntityOwnerID = TUniqueID::Empty();
		EntityType					refEntityType;

		#ifdef CLIENT
			PWorldEntity			visualization;
		#endif


		/**/						EntityMarker(Type t):Super(t)
		{}


		bool						operator==(const EntityMarker&other) const
		{ 
			return Super::operator==(other)
			&& refEntityID == other.refEntityID && refEntityType == other.refEntityType;
		}
		bool						operator!=(const EntityMarker&other) const {return !operator==(other);}

		#ifdef CLIENT
			virtual const Grid::LCellIdI&	GetLocation() const override {return visualization ? visualization->location : Super::GetLocation();}
		#endif
		virtual void				UpdateAge(const RoundNumber&roundNumber);

		virtual TUniqueID			GetReferencedEntityID() const override {return refEntityID;}
		virtual void				Destroy() override;
		virtual bool				IsEntityMarker() const override {return true;}

		virtual bool				DefaultOwnedByMe() const {return false;}


		virtual void				Pack(D::IWriteStream&s,Target target) const override;
		virtual void				Unpack(D::IReadStream&s) override;
		virtual void				ToSavegame(D::IWriteStream&s) const override;
		virtual void				FromSavegame(D::IReadStream&s) override;


	};

	class VisitedMarker : public Marker
	{
		typedef Marker				Super;
	public:
		/**/						VisitedMarker():Super(MarkerType::Visited)
		{}


	};


	class EntityDeathMarker : public EntityMarker
	{
		typedef EntityMarker		Super;
	public:
		static UINT32				outdatedAboveAge;

		/**/						EntityDeathMarker():Super(MarkerType::EntityDeath)
		{}

		virtual bool				DefaultOwnedByMe() const override{return true;}

		virtual bool				CanOutdateInTime() const override {return true;}
		virtual Outdated			CheckOutdated() const override;
		#ifdef CLIENT
			virtual void			Register(TMarkerArtifacts&) override;
			virtual void			Unregister(TMarkerArtifacts&) override;
		#endif

	};


	class HostileContactMarker : public EntityMarker
	{
		typedef EntityMarker		Super;
	public:

		/**/						HostileContactMarker():Super(MarkerType::HostileContact)
		{}

		virtual bool				CanOutdateInTime() const override {return true;}
		#ifdef CLIENT
			virtual void			Register(TMarkerArtifacts&) override;
			virtual void			Unregister(TMarkerArtifacts&) override;
		#endif

	};


	class AllianceMarker : public Marker
	{
		typedef Marker				Super;
	public:
		/**/						AllianceMarker():Super(MarkerType::Alliance)
		{}

		Package::NetString<64>		text;


		virtual bool				IsPrivate() const override {return false;}

		
		virtual void				Pack(D::IWriteStream&s,Target target) const override;
		virtual void				Unpack(D::IReadStream&s) override;
		virtual void				ToSavegame(D::IWriteStream&s) const override;
		virtual void				FromSavegame(D::IReadStream&s) override;
	};




	/**
	Preserves markers of a certain scope (e.g. a faction or the entire client)
	*/
	class MarkerStore
	{
	public:
		static const count_t		MaxMarkersPerFaction = 256;

		PMarker						Destroy(const TUniqueID&id);

		PMarker						FindByReference(const TUniqueID&referenceID)	const;


		void						Add(const PMarker&);

		struct TCandidate
		{
			TUniqueID				markerID;
			UINT32					priority;

			bool					operator<(const TCandidate&other) const
			{
				return priority > other.priority;	//biggest first, usually sorted by age
			}
		};

		/**
		Creates a new marker in the local store

		@param type Type of the marker to instantiate
		@param data Serialized marker data from file or network
		@param fromSavegame Set true if the provided serial data has been retrieved directly from a server save-game
		@param check Consistency check applied to the deserialized marker. If false, then the marker is discarded and null is returned

		*/
		PMarker						Create(MarkerType type, const ArrayRef<BYTE>&data, bool fromSavegame, const std::function<bool(const PMarker&)>&check = nullptr);
		PMarker						Create(MarkerType type, D::IReadStream&s, bool fromSavegame, const std::function<bool(const PMarker&)>&check = nullptr);
		PMarker						FindByID(const TUniqueID&referenceID)	const;
		PMarker						FindByLocation(const Grid::LCellIdI&)	const;
		PMarker						FindByLocation(MarkerType type, const Grid::LCellIdI&)	const;
		PMarker						FindByLocation(MarkerType type, const TUniqueID& ownerFactionID, const Grid::LCellIdI&)	const;
		void						GetDeletionCandidates(BasicBuffer<TCandidate>&candidates) const;
		void						Clear();
		count_t						Count() const {return all.Count();}
		bool						IsEmpty() const {return all.IsEmpty();}
		bool						IsNotEmpty() const {return all.IsNotEmpty();}


		/**
		Retrieves all markers that should be deleted because they are outdated or otherwise invalid.
		If grid visibility must be checked but cannot be retrieved right now, the method fails and returns false.
		The caller should try again later if this happens.
		@param[out] outOutdated List of resulting markers to erase. Empty if the method returns false
		@param currentRoundNumber Round number to use for age checks
		@return True if a list could be produced, false otherwise. Unavailable grid sensory can cause this method to fail
		*/
		bool						GetOutdated(BasicBuffer<PMarker>&outOutdated, const RoundNumber&currentRoundNumber);


		bool						HasAnyOfType(MarkerType t) const {return typed.IsSet(t);}

		void						Foreach(MarkerType t, const std::function<void(const PMarker&)>func) const
		{
			auto list = typed.QueryPointer(t);
			if (list)
				list->Foreach(func);
		}

		void						Foreach(const std::function<void(const PMarker&)>func) const
		{
			all.Foreach(func);
		}

		void						swap(MarkerStore&other)
		{
			all.swap(other.all);
			typed.swap(other.typed);
			doesOutdate.swap(other.doesOutdate);
			idMap.swap(other.idMap);
			referenceIDMap.swap(other.referenceIDMap);
			#ifdef CLIENT
				clientArtifacts.swap(other.clientArtifacts);
			#endif
		}

		friend void swap(MarkerStore&a, MarkerStore&b)
		{
			a.swap(b);
		}

		#ifdef CLIENT

			static MarkerStore		client;

			const TMarkerArtifacts&	GetArtifacts() const {return clientArtifacts;}
		#endif

	private:
		SharedContainer<Marker>							all;
		HashTable<MarkerType,SharedContainer<Marker> >	typed;
		HashTable<MarkerType,bool>				doesOutdate;
		HashTable<TUniqueID, PMarker>		idMap, referenceIDMap;

		#ifdef CLIENT
			TMarkerArtifacts	clientArtifacts;
		#endif
	};



}
