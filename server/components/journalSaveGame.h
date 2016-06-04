#ifndef journalSaveGameH
#define journalSaveGameH

#include <global_string.h>
#include <objects/types.h>
#include <uniqueID.h>
#include <protocol.h>
#include <sphereGrid.h>
#include <fstream>
#include <io/riff_handler.h>
#include "evolver.h"
#include "state.h"
#include "reflection.h"

namespace Journal
{
	/*
	General philosophy:

		Save games are stored as combinations of initial world states and sequential operations on the world.
			Effectively, new factions, destroyed factions, user-destroyed entities, deployment, and entity commands are stored,
			as these commands in combination with repeatable random numbers should make a savegame reconstructible from logs.
			Such savegames are replicated as a whole. Merge operations will always feature a common part of the log, and two
			competing diverging parts.
			In order to merge a game, the world state at the last known common entry is restored, and a merged new state constructed
			from merging the respective competing entity instructions.

		Merges:
			General savegame branch notation:
				A - branch
				A|B - merge of A and B
				A.B - sequence of partial branches, B depending on A. B can not be interpreted in isolation. If A.B then
				C.B, C!=A should be considered invalid due to the near guaranteed probability of this not matching.

			Constraints:
				A|B = B|A
				A|A = A
				(A|B)|A = (A|B)|B = A|B
				(C.A)|C = C.A


		Unique IDs cannot be made merge-robust, because of the different state conditions.
			Rather than trying to make them partially merge resistent, it will be up to the merger to appropriately map them.
			The preservation of unique IDs is not part of the merge constraint ruling.
			A merged state may utilize new unique IDs and still be valid. Therefore two strands may be considered equal even though
			all unique IDs diverge.


		Distinguish between mobile tasks and production jobs:
			Production jobs feature unique ids and can theoretically be reordered, with new jobs inserted inbetween.
				Only the first is critical, since resources may have been poured in, but that state could possibly be stored in the
				respective job. Therefore a finer merge strategy is reasonable here.
			Mobile tasks are stack-like, with no option to remove intermediate tasks or reorder.
				Allowed operations are append new, drop last, and drop all. Due to this, merging should be one stream over the other,
				with no fine-grained merge-strategies applied.

		It remains to be evaluated whether merging should be per entity, or rather per faction.
			If per faction, the faction change series with the most entries is applied, the other completely discarded.
			If per entity, a command-stream is chosen for each entity, individually, and then applied.
			Per entity is possible but not necessarily meaningful, as a scenario where the same faction has been actively controlled
			in concurrent games is either inappropriate client behavior, or some exotic use case that has not been developed.
			
		It should also be mentioned that, while concurrent games can be merged into a single valid game, received client
			information can not be undone. A radar vehicle moving into opposite directions simultaneously may lead to a merged game
			with the vehicle being only in one of these two locations. But the respective sensor information for both states has been
			sent. A custom client may abuse this to illegally acquire sensor information.




		Operations:
			Processing operations:
				Faction destruction:
					Not an operation, but a consequence of surrounding conditions. Not merged by itself.
			
				Production merging:
					A new production job is merged with a possibly existing production job of the opposing branch.
					Identical productions in one branch are stacked, identical productions in opposing branches are merged,
					such that e.g. Terraformaer production job #4 is merged with the same on the opposing branch.
					Merged jobs create an id mapping of the job itself, and of the produced entity.
					The moment of production scheduling is relevant for merging.

				Task merging:
					Non-production tasks 

			Instantanious operations:
				Faction creation:
					Not mergeable but location contraints must be observed. Invalid location leads to relocation and discarding of
					unit commands. If relocation is not possible, the event and all subsequent events of that faction are deopped.
					This is the worst that can happen to a faction.
					Entity spawning during this operation is atomic.

				Manual entity deployment:
					Earliest deployment of entity is favored. Redeployment of entity that has been deployed in the opposing
					branch is ignored.

				Task creation:
					Not recorded until end of round (see processing operations).

		Concept macro control model:
			Entities are directed by fuzzy instructions which are attempted to realize via some AI in combination with rudimentary
			commands. These commands can be implemented even under severe merge issues, as operations are issued based on the
			current and desired states.
	*/


	


	
	template <typename T>
		class ParameterStore
		{
		public:
			struct TEntry
			{
				String	key;
				T		value;

				void	adoptData(TEntry&other)
				{
					key.adoptData(other.key);
					StrategySelector<T>::Default::move(other.value,value);
				}

			};
			Buffer0<TEntry,Adopt>	entries;

			void	Set(const String&key, const T&value)
			{
				TEntry&e = entries.Append();
				e.key = key;
				e.value = value;
			}

			T&		Set(const String&key)
			{
				TEntry&e = entries.Append();
				e.key = key;
				return e.value;
			}

		
			void	swap(ParameterStore<T>&other)
			{
				entries.swap(other.entries);
			}

			void	Clear()
			{
				entries.Clear();
			}

			const T&	Require(const TCodeLocation&loc, const String&p)	const
			{
				foreach (entries,e)
					if (e->key == p)
						return e->value;
				throw Round::RoundException(loc, "Decoding stage: Unable to retrieve value of "+p+" ("+String(typeid(T).name())+")");
			}

			void	Require(const TCodeLocation&loc, const String&p, T&rsOut)	const
			{
				rsOut = Require(loc,p);
			}

			template <typename T1>
			void	RequireCast(const TCodeLocation&loc, const String&p, T1&rsOut)	const
			{
				rsOut = (T1)Require(loc,p);
			}
			template <typename T1>
			void	RequireEnum(const TCodeLocation&loc, const String&p, T1&rsOut)	const
			{
				T val = Require(loc,p);
				if (!rsOut.Load(val))
				{
					throw Round::RoundException(loc,"Unable to cast value '"+String(val)+"' to "+T1::GetName());
				}
			}


			bool		TryGet(const String&p, T&rs) const
			{
				foreach (entries,e)
					if (e->key == p)
					{
						rs = e->value;
						return true;
					}
				return false;
			}

			count_t		CountMatches(const String&p) const
			{
				count_t rs = 0;
				foreach (entries,e)
					if (e->key == p)
					{
						rs++;
					}
				return rs;
			}

			count_t		GetArray(const String&p, ArrayData<T>&field) const
			{
				count_t cnt = CountMatches(p);
				field.SetSize(cnt);
				if (!cnt)
				{
					return 0;
				}

				index_t at = 0;

				foreach (entries,e)
					if (e->key == p)
					{
						field[at] = e->value;
						at++;
					}

				return cnt;
			}

		};







	class Entry
	{
	public:
		ParameterStore<TUniqueID>			uuidParameters;
		ParameterStore<String>				stringParameters;
		ParameterStore<Grid::LCellIdI>		locationParameters;
		ParameterStore<int>					intParameters;
		ParameterStore<Array<BYTE> >		binaryParameters;
		Buffer0<Entry,Swap>					children;

		//CONSTRUCT_ENUMERATION22(Type,
		//	None,
		//	RoundCommit,
		//	EntityConstructor,
		//	FactionCreation,
		//	ClearEntityCommands,
		//	ClearLastEntityCommand,
		//	DirectToCommand,
		//	ChoiceIntersection,
		//	JobCommand,
		//	NewEntityProduction,
		//	DropEntityProduction,
		//	FactionOnlineChange,
		//	FactionData,
		//	EntityData,
		//	DestroyEntity,
		//	WorldCreation,
		//	LogContinuedAllOffline,
		//	EntityStateCheck,
		//	AutomationChange,
		//	VariableUpdate,
		//	ResourceCoverageChange,
		//	ResourceCoverageKnowledgeLoss
		//);

		//Type		operation;
		String		operation;

		void			swap(Entry&other)
		{
			intParameters.swap(other.intParameters);
			locationParameters.swap(other.locationParameters);
			stringParameters.swap(other.stringParameters);
			uuidParameters.swap(other.uuidParameters);
			binaryParameters.swap(other.binaryParameters);
			children.swap(other.children);
			operation.swap(other.operation);
			//swp(operation,other.operation);
		}

		void			Clear()
		{
			intParameters.Clear();
			locationParameters.Clear();
			stringParameters.Clear();
			uuidParameters.Clear();
			binaryParameters.Clear();
			children.Clear();
			operation = "";
		}

		void			MoveAppend(Entry&child, count_t indention)
		{
			if (indention == 0)
				children.MoveAppend(child);
			else
			{
				ASSERT__(children.IsNotEmpty());
				children.last().MoveAppend(child,indention-1);
			}

		}

		void			Write(std::ofstream&, count_t indention)	const;
		count_t			Read(std::ifstream&);
	};






	class Round
	{
	public:

		static StringRef	OperationNameOf(const std::type_info&);


		class RoundException : public Except::Exception
		{
			typedef Except::Exception	Super;
		public:

			/**/			RoundException(const TCodeLocation&loc, const String&message):Super(loc,message)	{}

			const char*		GetCoreMessage() const {return Except::RootException::what();}
		};


		class Event
		{
		public:
			//typedef Entry::Type	Type;
			//const Type		type;

			virtual			~Event() {}


			virtual Event*	Instantiate() const = 0;
			virtual void	Decode(const Entry&e)=0;
			void			ExportTo(Entry&) const;
		protected:
			virtual void	Compile(Entry&)	const=0;
		};

		typedef std::shared_ptr<Event>	PEvent;


		class GlobalEvent : public Event
		{
		public:
			virtual void	Execute() const=0;
			bool			TryExecute() const;
		};
		typedef std::shared_ptr<GlobalEvent>	PGlobalEvent;

		class FactionEvent : public Event
		{
		public:
			virtual void	Execute(const Objects::PFaction&) const=0;
			bool			TryExecute(const Objects::PFaction&f) const;
		};
		typedef std::shared_ptr<FactionEvent>	PFactionEvent;

		template <typename Derivative>
			class FactionEventT : public FactionEvent
			{
			public:
				virtual Event*Instantiate() const override {return new Derivative();}
			};

		class EntityEvent : public Event
		{
		public:
			virtual void	Execute(const Objects::PWorldEntity&) const=0;
			bool			TryExecute(const Objects::PWorldEntity&f) const;
		};
		typedef std::shared_ptr<EntityEvent>	PEntityEvent;

		template <typename Derivative>
			class EntityEventT : public EntityEvent
			{
			public:
				virtual Event*Instantiate() const override {return new Derivative();}
			};


		class ResourceCoverageChange : public GlobalEvent
		{
		public:
			UINT32			stackID;
			Objects::Resource	resource;
			Grid::Resource::coverage_t	newCoverage;
			/**/			ResourceCoverageChange(UINT32 stackID=0, Objects::Resource res=Objects::Resource(),  Grid::Resource::coverage_t	newCoverage=0)
							:stackID(stackID),resource(res),newCoverage(newCoverage)	{}

			Event*			Instantiate() const override {return new ResourceCoverageChange();}
			void			Decode(const Entry&e) override;
			void			Execute()	const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};


		class ResourceCoverageKnowledgeLoss : public FactionEvent
		{
		public:
			UINT32			stackID;
			/**/			ResourceCoverageKnowledgeLoss(UINT32 stackID=0):stackID(stackID)	{}

			Event*			Instantiate() const override {return new ResourceCoverageKnowledgeLoss();}
			void			Decode(const Entry&e) override;
			void			Execute(const Objects::PFaction&)	const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class ChangeInterplanetarySensory : public FactionEventT<ChangeInterplanetarySensory>
		{
		public:
			UINT32			lowStackID;
			int				radius;
			int				depth;
			bool			lose;

			/**/			ChangeInterplanetarySensory(bool lose=false, UINT32 lowStackID=0, int radius=10, int depth=0):lose(lose),lowStackID(lowStackID),radius(radius),depth(depth)	{}
			void			Decode(const Entry&e) override;
			void			Execute(const Objects::PFaction&)	const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class EntityStateCheck : public EntityEvent
		{
			static int		GetJobMineralsLeft(const Objects::PWorldEntity&e);
			static int		GetStoredMinerals(const Objects::PWorldEntity&e);
			static Grid::LCellIdI		GetNextTaskFocus(const Objects::PWorldEntity&e);
			static count_t		GetTaskListLength(const Objects::PWorldEntity&e);
			static Objects::CursorMode		GetNextTaskMode(const Objects::PWorldEntity&e);
			static bool		IsEnabled(const Objects::PWorldEntity&e);
		public:
			Grid::LCellIdI	location,nextTaskFocus;
			int				health=0,empDamageLevel=0,storedMinerals=0,jobMineralsLeft=0;
			count_t			taskListLength=0;
			Objects::CursorMode	nextTaskMode;
			bool			isEnabled;

			/**/			EntityStateCheck()	{}
			/**/			EntityStateCheck(const Objects::PWorldEntity&);
			Event*			Instantiate() const override {return new EntityStateCheck();}
			void			Decode(const Entry&e) override;
			void			Execute(const Objects::PWorldEntity&)	const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class CommitRound : public GlobalEvent
		{
		public:
			UINT32			roundNumber;
			String			evolverName;
			UINT32			fileDataVersion;

			/**/			CommitRound(const String&evolverName="", UINT32 roundNumber=0)
							:roundNumber(roundNumber),evolverName(evolverName),fileDataVersion(SupportedFileVersion) {}
			virtual Event*	Instantiate() const override {return new CommitRound("",0);};
			virtual void	Decode(const Entry&e) override;
			virtual void	Execute() const override {};

		protected:
			virtual void	Compile(Entry&)	const override;
		};


		class UpdateVariable : public GlobalEvent
		{
		public:
			String			key,value;

			/**/			UpdateVariable(const String&key="", const String&value="")
							:key(key),value(value) {}
			virtual Event*	Instantiate() const override {return new UpdateVariable("","");};
			virtual void	Decode(const Entry&e) override;
			virtual void	Execute() const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class PureEntityDescriptor
		{
		public:
			String			baseType,
							turretType;


			/**/			PureEntityDescriptor() {}
			/**/			PureEntityDescriptor(const Objects::PWorldEntity&);
			void			Decode(const Entry&e);
			Objects::PWorldEntity	Instantiate(const Objects::PFaction&f, const TUniqueID&id, const Grid::LCellIdI&loc) const;
			void			Compile(Entry&)	const;
		};

		class CreateEntity : public FactionEvent
		{
		public:
			PureEntityDescriptor	desc;
			Grid::LCellIdI	location;
			TUniqueID		entityID;
			bool			enableWorkingStructure;

			/**/			CreateEntity()	{}
			/**/			CreateEntity(const Objects::PWorldEntity&);
			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new CreateEntity();};
			virtual void	Execute(const Objects::PFaction&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class CreateFaction : public GlobalEvent
		{
		public:
			CONSTRUCT_ENUMERATION3(FactionType,
				Undefined,
				Regular,
				Nexus
				);

			/**/			CreateFaction() {}
			/**/			CreateFaction(const Objects::Faction&, FactionType factionType);

			FactionType		factionType;
			CryptographicID	accessToken;
			TUniqueID		factionID;
//			Buffer0<PEntityConstructor>	newEntities;
			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new CreateFaction();};
			virtual void	Execute() const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		template <class Derivative>
		class EntitySignal : public EntityEventT<Derivative>
		{
		public:
			virtual void	Decode(const Entry&e) override {};

		protected:
			virtual void	Compile(Entry&)	const override	{};
		};

		class ConstructionAbort : public EntityEventT<ConstructionAbort>
		{
		public:
			TUniqueID		taskID;


			/**/			ConstructionAbort()	{}
			/**/			ConstructionAbort(const TUniqueID&taskID):taskID(taskID)	{}

			virtual void	Execute(const Objects::PWorldEntity&) const override;
			virtual void	Decode(const Entry&e) override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class RestoreCoreLink : public EntitySignal<RestoreCoreLink>
		{
		public:
			virtual void	Execute(const Objects::PWorldEntity&) const override;
		};

		class ClearEntityCommands : public EntitySignal<ClearEntityCommands>
		{
		public:
			/**/			ClearEntityCommands() {}
			virtual void	Execute(const Objects::PWorldEntity&) const override;
		};

		class ClearLastEntityCommand : public EntitySignal<ClearLastEntityCommand>
		{
		public:
			/**/			ClearLastEntityCommand() {}
			virtual void	Execute(const Objects::PWorldEntity&) const override;
		};

		class DestroyEntity : public EntityEvent
		{
		public:
			Objects::RemovalType	type;

			/**/			DestroyEntity(Objects::RemovalType type=Objects::RemovalType()):type(type) {}
			virtual Event*	Instantiate() const override {return new DestroyEntity();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;
			virtual void	Decode(const Entry&e) override;

		protected:
			virtual void	Compile(Entry&)	const override;

		};

		class EnableStructure : public EntityEventT<EnableStructure>
		{
		public:
			bool			doEnable;

			/**/			EnableStructure(bool doEnable = true) : doEnable(doEnable)	{}
			virtual void	Execute(const Objects::PWorldEntity&) const override;
			virtual void	Decode(const Entry&e) override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class ChangeAutomation: public EntityEvent
		{
		public:
			bool			isNowAutomated;

			/**/			ChangeAutomation(bool isNowAutomated=false):isNowAutomated(isNowAutomated)	{}
			virtual Event*	Instantiate() const override {return new ChangeAutomation();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;
			virtual void	Decode(const Entry&e) override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class ChangeSleep: public EntityEvent
		{
		public:
			bool			isNowSleeping;

			/**/			ChangeSleep(bool isNowSleeping=false):isNowSleeping(isNowSleeping)	{}
			virtual Event*	Instantiate() const override {return new ChangeSleep();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;
			virtual void	Decode(const Entry&e) override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};


		/**
		Place-holder for later
		*/
		class CreateWorld : public GlobalEvent
		{
			Grid::TPlanetConfig	cfg;
			/**/			CreateWorld(const Grid::TPlanetConfig&cfg):cfg(cfg) {}
			/**/			CreateWorld() {}
			virtual Event*	Instantiate() const override {return new CreateWorld();};
			virtual void	Execute() const override {}
			virtual void	Decode(const Entry&e) override {};

		protected:
			virtual void	Compile(Entry&)	const override {};
		};
		
		class ChoiceIntersection : public EntityEvent
		{
		public:
			TUniqueID		deployingWhat=TUniqueID::Empty();
			Objects::CursorMode mode;
			Grid::LCellIdI	target;
			
			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new ChoiceIntersection();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class DirectToCommand : public EntityEvent
		{
		public:
			bool			leftClick, detailed;
			Objects::CursorMode mode;
			Array<Grid::LCellIdI>	targets;
			
			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new DirectToCommand();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};


		class JobCommand : public EntityEvent
		{
		public:
			Riff::TID		jobID;
			Array<BYTE>		jobPayload;
			Grid::LCellIdI	focus = Grid::LCellIdI::Invalid;

			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new JobCommand();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};



		class EntityProduction : public EntityEvent
		{
		public:
			TUniqueID		taskID;

			virtual void	Compile(Entry&)	const override;
			virtual void	Decode(const Entry&e) override;
		};

		class NewEntityProduction : public EntityProduction
		{
			typedef EntityProduction	Super;
		public:
			index_t				insertAt;
			PureEntityDescriptor	desc;
			TUniqueID		newEntityID;

			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new NewEntityProduction();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class DropEntityProduction : public EntityProduction
		{
		public:
			virtual Event*	Instantiate() const override {return new DropEntityProduction();};
			virtual void	Execute(const Objects::PWorldEntity&) const override;
		};


		class FactionOnlineChange : public FactionEvent
		{
		public:
			bool			nowOnline;

			/**/			FactionOnlineChange(bool nowOnline=false):nowOnline(nowOnline)	{}
			virtual void	Decode(const Entry&e) override;
			virtual Event*	Instantiate() const override {return new FactionOnlineChange(false);};
			virtual void	Execute(const Objects::PFaction&) const override	{};

		protected:
			virtual void	Compile(Entry&)	const override;
		};

		class CreateMarker : public FactionEventT<CreateMarker>
		{
		public:
			TUniqueID		markerID;
			Objects::Marker::Type	type;
			UINT32			createdInRound;
			Grid::LCellIdI	location;
			Protocol::MarkerReferenceEntity	referenceEntity;

			/**/			CreateMarker(const TUniqueID&markerID, Objects::Marker::Type t, UINT32 createdInRound, const Grid::LCellIdI&location, const Protocol::MarkerReferenceEntity&referenceEntity)
							:markerID(markerID),type(t),createdInRound(createdInRound),location(location),referenceEntity(referenceEntity)	{}
			/**/			CreateMarker()	{};
			virtual void	Decode(const Entry&e) override;
			virtual void	Execute(const Objects::PFaction&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;
		};

	
		class DestroyMarker : public FactionEventT<DestroyMarker>
		{
		public:
			TUniqueID		markerID;


			/**/			DestroyMarker()	{}
			/**/			DestroyMarker(const TUniqueID&id): markerID(id)	{}
			virtual void	Decode(const Entry&e) override;
			virtual void	Execute(const Objects::PFaction&) const override;

		protected:
			virtual void	Compile(Entry&)	const override;

		};
		

		/**
		General-purpose storage for round events
		*/
		template <typename EventType>
		class EventStorage : public Buffer0<std::shared_ptr<EventType> >
		{
			typedef Buffer0<std::shared_ptr<EventType> >	Super;
		public:
			index_t				FindLast(const std::type_info&t) const
			{
				for (index_t at = Super::Count()-1; at < Super::Count(); --at)
				{
					if (typeid(*Super::at(at)) == t)
						return at;
				}
				return InvalidIndex;
			}
			template <typename T>
				index_t			FindLast() const	{return FindLast(typeid(T));}

			static index_t		FindMax(index_t a, index_t b) {if (a == InvalidIndex) return b; if (b == InvalidIndex) return a; return std::max(a,b);}
			template <typename T0, typename T1, typename ... T>
				static index_t	FindMax(T0 a0, T1 a1, T...a) {return FindMax(a0, FindMax(a1, a...));}

			template <typename ... T>
				index_t			FindLastOf() const	{ return FindMax(FindLast(typeid(T))...);  }
			count_t				Count(const std::type_info&) const
			{
				count_t count = 0;
				for (const auto&ev : *this)
					if (typeid(*ev) == t)
						count++;
				return count;
			}
			count_t				ClearAll(const std::type_info&t)
			{
				count_t rs = 0;
				for (index_t i = 0; i < Super::Count(); i++)
					if (typeid(*Super::at(i)) == t)
					{
						Super::Erase(i);
						i--;
						rs++;
					}
				return rs;
			}
			template <typename T>
				count_t			ClearAll() {return ClearAll(typeid(T));}
			bool				ClearLast(const std::type_info&t)	{index_t at = FindLast(t); if (at != InvalidIndex) {Super::Erase(at); return true;} return false;}
		};


		class EntityStrand : public EventStorage<EntityEvent>
		{
		public:
		};

		class FactionStrand
		{
		public:
			EventStorage<FactionEvent>			factionEvents;
			GenericHashTable<TUniqueID,EntityStrand,TUniqueID,Copy,Swap>	entityStrands;

			void			swap(FactionStrand&other)
			{
				entityStrands.swap(other.entityStrands);
				factionEvents.swap(other.factionEvents);
			}
		};


		typedef GenericHashTable<TUniqueID,FactionStrand,TUniqueID,Copy,Swap>	FactionStrands;
		FactionStrands					factionStrands;
		EventStorage<GlobalEvent>				globalEvents;
		//StringTable<String>					variableUpdates;
		
		UINT32	roundNumber=0;
		UINT32	fileDataVersion=SupportedFileVersion;
		Evolver::Abstract	*evolver = nullptr;
		//const char*	kernelName=nullptr;


		static StringTable<PEvent>	eventMap;
		
		void	Decode(const Entry&roundRootEntry);


		/**
		Logs the creation of a new faction.
		Must be called AFTER a faction has been created and all new world entities have been spawned.
		*/
		void	LogFactionCreation(const Objects::PFaction&f, Round::CreateFaction::FactionType type);
		void	RegisterClearEntityCommands(bool execute, const Objects::PWorldEntity&);
		void	RegisterDropLastEntityCommand(bool execute, const Objects::PWorldEntity&);
		void	RegisterDirectEntityTo(bool execute, const Objects::PWorldEntity&, Objects::CursorMode mode, const ArrayRef<Grid::LCellIdI>&target, bool detailed, bool mouseLeftClick);
		void	RegisterChoiceIntersection(bool execute, const Objects::PWorldEntity&,const Grid::LCellIdI&target, Objects::CursorMode mode, const Objects::PWorldEntity&deployingWhat);
		void	LogJobCommandStart(const Objects::PWorldEntity&, const Objects::VehicleJob::BaseJob&, const Grid::LCellIdI&focus);
		void	LogFactionOffline(const Objects::PFaction&);
		void	LogFactionOnline(const Objects::PFaction&);
		void	LogFactionOnline(const Objects::PFaction&,bool nowOnline);
		void	RegisterProductionJobStart(bool execute, const Objects::PWorldEntity&e, const Objects::PMobileEntityProducer&asProducer, const ConstName*parameters, count_t parameterCount);
		void	RegisterEntityDestruction(bool execute, const Objects::PWorldEntity&, Objects::RemovalType t);
		void	LogEntityCreation(const Objects::PWorldEntity&);
		void	RegisterAutomationChange(bool execute, const Objects::PMobileEntity&, bool isNowAutomated);
		void	RegisterSleepChange(bool execute, const Objects::PWorldEntity&, bool isNowSleeping);
		void	RegisterPersistentVariableChange(bool execute, const Reflection::Variable::PBase&);
		void	RegisterResourceCoverageChange(bool execute, UINT32 stackID, Objects::Resource res, Grid::Resource::coverage_t newCoverage);
		void	RegisterResourceCoverageKnowledgeLoss(bool execute, const Objects::PFaction&, UINT32 stackID);


		void	Register(bool execute, const Objects::PFaction&f, Round::FactionEvent*newEvent);
		void	Register(bool execute, const Objects::PWorldEntity&e, Round::EntityEvent*newEvent);
		void	Register(bool execute, Round::GlobalEvent*newEvent);

		void	Clear();

		void	AppendEntityVerification();

		/**
		Applies the local round to the global state.
		@param process Set true to also perform round-processing using the locally referenced kernel.
		*/
		void	Apply(bool process);


		static void StaticCreate();
	};



	class Storage
	{
	public:

		Buffer0<Entry,Swap>	entries;


		void	Load(const PathString&);
		void	Load(std::ifstream&);
		void	Write(const PathString&) const;
		void	Write(std::ofstream&) const;
	
		/**
		Sequentially applies and processes each round.
		*/
		void		ApplyToGlobalState() const;
		static void ApplyToGlobalState(const Entry&,bool process);
	};

	namespace Helper
	{
		void	WriteToFile(const Grid::Resource&data, const PathString&filename);
		void	LoadFromFile(Grid::Resource&data, const PathString&filename);
	
		void	WriteToFile(Grid::WorldData&data, const PathString&filename);
		void	LoadFromFile(Grid::WorldData&data, const PathString&filename);
	}


	class Writer
	{
	private:
		std::ofstream	logStream;
	public:
		/**
		Initializes a new journal based on the current data.
		Should be called after the planet has been generated, but before any factions or units have been created (except nexus buildings).
		Any existing facitons and their units will be logged automatically via LogFactionCreation().
		*/
		void	InitNew(const PathString&);

		/**
		Continues an existing log file.
		*/
		void	ContinueExisting(const PathString&);
		void	Continue(const Storage&, const Grid::WorldData&world, const Grid::WorldData&initialWorld, const PathString&writeTo);

		void	Log(const Journal::Entry&);
		void	Log(const Round::Event&);

		void	Commit(const Round&round);
		void	CommitAndClear(Round&round)	{Commit(round); round.Clear();}
	};

	extern Writer globalWriter;
	extern Round currentRound;

	Round::CreateFaction::FactionType	GetType(const Objects::PFaction&f);



	void	Create();
	void	Destroy();
}

#endif
