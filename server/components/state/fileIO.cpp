#include <global_root.h>
#include "fileIO.h"
#include <runtimeConfig.h>
#include <objects.h>
#include <protocol.h>
#include <io/file_system.h>
#include "networkState.h"
#include "../reflection.h"
#include "gameState.h"
#include "worldState.h"
#include "../configurationMode.h"
#include "../../../client/version.h"

namespace FileIO
{
	RuntimeConfig::RoundNumber			lastSaveRound;
	namespace Config
	{
		int			saveEveryNRounds = 10;
		bool		saveWhenAllQuit = true;
	}

	namespace Network
	{
		void		HandleSessionClosure()
		{
			lastSaveRound = 0;

		}
	}

	void CheckAutoSave(bool tempIncrementRoundNumber)
	{
		if (Config::saveEveryNRounds > 0 && !((RuntimeConfig::roundNumber.main+1)%FileIO::Config::saveEveryNRounds))
		{
			if (tempIncrementRoundNumber)
				RuntimeConfig::roundNumber.main++;
			FileIO::AutoSave();
			if (tempIncrementRoundNumber)
				RuntimeConfig::roundNumber.main--;
		}
	}


	
	void RefreshWindowTitle()
	{
		String title = String(PRODUCT_NAME)+" server "+ ::Network::serverID.ToNiceString()+" (world "+ World::worldID.ToNiceString()+")";
		if (ConfigurationMode::IsActive())
			title += " INTERACTIVE CONFIGURATION MODE";
		SetConsoleTitleA(title.c_str());
	}





	enum struct ChunkName : UINT16
	{
		None = 0,
		WorldInfoGroup = 1,
			//WorldID,
			//RuntimeConfig::roundNumber,

		ObjectGroup = WorldInfoGroup + 1024,
			VehicleTaskSection,
			//Structure = Vehicle + 256,

		WorldGroup = ObjectGroup + 1024,

		InitialWorldGroup = WorldGroup + 1024,
			//WorldChunk,
		ResourceSection = InitialWorldGroup + 1024,
				//ResourceName,
				//ResourceCoverage,

		//FactionGroup = WorldGroup + 1024,
		FactionSection = ResourceSection + 1024,
				//FactionID,
				//FactionAccessToken,
				//ResourcesDiscovered_Compressed,
				//ResourcesDiscovered_Full,
	};


	static_assert(sizeof(ChunkName) == 2, "Alignment fault");
	static Riff::Chunk* CreateNamedList(Riff::Chunk&parent, ChunkName name)
	{
		return parent.AppendNamedList(name);
	}

	static Riff::Chunk* FindNamedList(Riff::Chunk&parent, ChunkName name)
	{
		Riff::Chunk*a,*b;
		if (parent.FindFirstNamedList(a,b))
			do
			{
				UINT16 key;
				if (a->size() == 2)
				{
					a->Get(&key);
					if (name == (ChunkName)key)
						return b;
				}

			}
			while (parent.FindNextNamedList(a,b));

		return nullptr;
	}


	namespace RID 	//RIFF id constants
	{
		static const Riff::TID	
								mostFactionsAlive(" MFA"),
								worldTypeChunk(" WTC"),
								worldCounterChunk(" WCC"),
								factionAccessToken("ACTK"),
								basesRequired("BSRQ"),
								closedGame("CLGM"),
								doomsdayWeapons("DODW"),
								exposeActionLeftToHostileFactions("EAHF"),
								empDamageLevel("EMPD"),
								entity("ENTI"),
								factionID("ID  "),
								fociFlags("FCIF"),
								foci("FOCI"),
								factionSupportFlags("FSFL"),
								goal("GOAL"),
								location("LOCA"),
								factionMarkers("MARK"),
								mobileEntityType("METY"),
								mobileEntityParentJobIndex("MJPI"),
								factionName("NAME"),
								nexusAI("NEXA"),	//currently only indicates that nexus ai is active
								nexusBases("NEXB"),
								nexusData("NEXD"),
								nexusTag("NEXU"),
								newVehiclesHaveFullAmmo("NVHA"),
								path("PATH"),
								roundNumber("RNDN"),
								resourceCoverage("RSCV"),
								resourcesDiscoveredCompressed("RSDC"),
								resourcesDiscoveredFull("RSDF"),
								resourceName("RSNM"),
								resources("RSRC"),
								noMunitionResources("RSRM"),
								structureEnabled("SENA"),
								towerActionLeft("STAL"),
								startBases("STBS"),
								towerItem("STOI"),
								structureType("STYP"),
								vehiclePlant("SVPL"),				//also used for MobileEntityProducers in general for backwards-compatibility reasons
								vehiclePlantGatheringPoint("SVPG"),	//also used for MobileEntityProducers in general for backwards-compatibility reasons
								vehiclePlantItem("SVPI"),
								taskData("TDAT"),
								taskName("TNME"),
								variableConfiguration("VARC"),
								vehicle("VEHC"),
								vehicleBase("VEHB"),
								vehicleTurret("VEHT"),
								fileVersion("VERS"),
								baseJob("VHBJ"),
								factionWorldCounterChunk("WCCK"),
								worldConfig("WCFG"),
								worldContentTypeCount("WCTC"),
								worldID("WDID"),
								factionWorldTypeChunk("WTCK"),
								terrainExponent("TREX"),
								researchScript("RESS"),
								researchState("REST"),
								activeResearchTopic("ARET"),

								zzzz("zzzz")	//no use, just so that every line can have a comma at the end
								;


	}



	inline static void GetBinary(Riff::Chunk*parent, const Riff::TID&id, void*resultOut, size_t size)
	{
		Riff::Chunk*chk = parent->FindFirst(id);
		ASSERT_NOT_NULL1__(chk, id.ToString());
		ASSERT_EQUAL__(chk->size(),size);
		chk->Get(resultOut);
	}

	template <typename T>
		inline static void GetPOD(Riff::Chunk*parent, const Riff::TID&id, T&resultOut)
		{
			GetBinary(parent,id,&resultOut,sizeof(T));
		}
	
	template <typename T>
		inline static void GetPODs(Riff::Chunk*parent, const Riff::TID&id, ArrayData<T>&resultOut)
		{
			Riff::Chunk*chk = parent->FindFirst(id);
			ASSERT_NOT_NULL1__(chk, id.ToString());
			ASSERT__(chk->IsMultipleOf(sizeof(T)));
			count_t numElements = chk->size() / sizeof(T);
			resultOut.SetSize(numElements);
			chk->Get(resultOut.pointer());
		}

	template <typename T>
		inline static void GetPODs(Riff::Chunk*parent, const Riff::TID&id, BasicBuffer<T>&resultOut)
		{
			Riff::Chunk*chk = parent->FindFirst(id);
			ASSERT_NOT_NULL1__(chk, id.ToString());
			ASSERT__(chk->IsMultipleOf(sizeof(T)));
			count_t numElements = chk->size() / sizeof(T);
			resultOut.SetSize(numElements,true);
			chk->Get(resultOut.pointer());
		}

	inline static bool GetOptionalBinary(Riff::Chunk*parent, const Riff::TID&id, void*resultOut, size_t size)
	{
		Riff::Chunk*chk = parent->FindFirst(id);
		if (!chk)
			return false;
		ASSERT_EQUAL__(chk->size(),size);
		chk->Get(resultOut);
		return true;
	}

	template <typename T>
		inline static bool GetOptionalPOD(Riff::Chunk*parent, const Riff::TID&id, T&resultOut, const T&ifNonExistent)
		{
			bool there = GetOptionalBinary(parent,id,&resultOut,sizeof(T));
			if (there)
				return true;
			resultOut = ifNonExistent;
			return false;
		}

	namespace EntityFlag
	{
		enum EntityFlag : UINT8
		{
			IsUnstable = 0x1,
			IsSleeping = 0x2,

		};
	}

	static void Export(Riff::Chunk*parent, const Objects::WorldEntity&e, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		factions.Set(e.owner);
		static ByteStream stream;
		stream.Clear();


		stream.Append(e.id);
		stream.Append(e.owner->id);
		stream.Append(float(e.health) / float(e.GetMaxHealth()));

		UINT8 flags = 0;
		if (e.unstable)
			flags |= EntityFlag::IsUnstable;
		if (e.IsSleeping())
			flags |= EntityFlag::IsSleeping;
		stream.Append(flags);
		
		parent->AppendBlock(RID::entity,stream.pointer(),stream.GetFillLevel());
		parent->AppendBlock(RID::location)->SetPODData(e.location);
		if (e.empDamageLevel)
			parent->AppendBlock(RID::empDamageLevel)->SetPODData(e.empDamageLevel);
	}

	static void Import(Riff::Chunk*parent, Objects::WorldEntity&e, const GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&ownerMap)
	{
		using namespace ApplicationInterface;
		Riff::Chunk*source = parent->FindFirst(RID::entity);
		ASSERT_NOT_NULL__(source);

		source->OpenStream();
		ASSERT__(source->Stream(e.id));
		TUniqueID	ownerID;
		ASSERT__(source->Stream(ownerID));
		if (globalVersionOfCurrentFile < FileVersions::CurrentEntityHealthIsFloat)
		{
			ASSERT__(source->Stream(e.health));
		}
		else
		{
			float fhealth;
			ASSERT__(source->Stream(fhealth));
			e.health = (int)round(float(e.GetMaxHealth()) * fhealth);
		}
		e.health = std::min(e.health,e.GetMaxHealth());
		if (globalVersionOfCurrentFile < FileVersions::EntitiesHaveFlags)
		{
			ASSERT__(source->Stream(e.unstable));
		}
		else
		{
			UINT8 flags;
			ASSERT__(source->Stream(flags));
			e.unstable = (flags & EntityFlag::IsUnstable) != 0;
			float t = 0;
			e.SetSleep((flags&EntityFlag::IsSleeping) !=0,t,false);
		}
		ASSERT__(source->CloseStream());
		ASSERT__(ownerMap.Query(ownerID,e.owner));

		GetOptionalPOD(parent,RID::empDamageLevel,e.empDamageLevel,0);


		//e.SetupSystem();
	}


	static void Export(Riff::Chunk*parent, const Objects::ResourceVector&v)
	{
		if (v.IsNotEmpty())
		{
			#ifdef NO_MUNITION
				parent->AppendBlock(RID::noMunitionResources,v.level,sizeof(v.level));
			#else
				parent->AppendBlock(RID::resources,v.level,sizeof(v.level));
			#endif
		}
	}

	static void Import(Riff::Chunk*parent, Objects::ResourceVector&v)
	{
		Riff::Chunk*source = parent->FindFirst(RID::resources);
		if (source)
		{
			Objects::ResourceVectorT<Objects::ResourceWithMunition> v2;
			ASSERT_EQUAL__(source->size(),sizeof(v2.level));
			memcpy(v2.level,source->pointer(),sizeof(v2.level));
			v.LoadCompatible(v2);
		}
		else
		{
			source = parent->FindFirst(RID::noMunitionResources);
			if (source)
			{
				Objects::ResourceVectorT<Objects::ResourceWithoutMunition> v2;
				ASSERT_EQUAL__(source->size(),sizeof(v2.level));
				memcpy(v2.level,source->pointer(),sizeof(v2.level));
				v.LoadCompatible(v2);
			}
			else
				v.Clear();
		}
	}


	namespace FocusFlag
	{
		enum flag_t : BYTE
		{
			IsDetailed = 0x1,
			IsIntermediate = 0x2
		};
	}

	static void Export(Riff::Chunk*taskChunk, const Queue<Objects::MobileEntity::Task::Focus>&foci)
	{
		static Array<Grid::LCellIdI>	fociArray;
		static Array<BYTE>	flagArray;
		if (foci.IsEmpty())
			return;
		fociArray.SetSize(foci.Count());
		flagArray.SetSize(foci.Count());
		for (index_t i = 0; i < foci.Count(); i++)
		{
			const auto&source = foci[i];
			fociArray[i] = source.target;
			flagArray[i] = (source.isDetailed ? FocusFlag::IsDetailed : 0) | (source.isIntermediate ? FocusFlag::IsIntermediate : 0);
		}
		taskChunk->AppendBlock(RID::foci,fociArray.pointer(),fociArray.GetContentSize());
		taskChunk->AppendBlock(RID::fociFlags,flagArray.pointer(),flagArray.GetContentSize());
	}

	static void Import(Riff::Chunk*taskChunk, Queue<Objects::MobileEntity::Task::Focus>&outFoci)
	{
		Riff::Chunk*source = taskChunk->FindFirst(RID::foci);
		Riff::Chunk*source1 = taskChunk->FindFirst(RID::fociFlags);
		outFoci.Clear();
		if (source)
		{
			const count_t numFoci = source->size() / sizeof(Grid::LCellIdI);
			const Grid::LCellIdI*foci = (const Grid::LCellIdI*)source->pointer();
			for (index_t i = 0; i < numFoci; i++)
				outFoci.Push().target = foci[i];
			if (source1)
			{
				ASSERT_EQUAL__(numFoci,source1->size());
				const BYTE*fociFlags = source1->pointer();
				for (index_t i = 0; i < numFoci; i++)
				{
					BYTE flags = fociFlags[i];
					outFoci[i].isDetailed = (flags & FocusFlag::IsDetailed) != 0;
					outFoci[i].isIntermediate = (flags & FocusFlag::IsIntermediate) != 0;
				}
			}
		}
	}

	static void Export(Riff::Chunk*taskChunk, const BasicBuffer<Objects::PathNode>&path)
	{
		static_assert(sizeof(Objects::PathNode)==sizeof(Grid::LCellIdI)+sizeof(int), "Struct alignment fault" );
		if (path.IsEmpty())
			return;
		taskChunk->AppendBlock(RID::path,path.pointer(),path.GetContentSize());
	}

	static void Import(Riff::Chunk*taskChunk, BasicBuffer<Objects::PathNode>&outPath)
	{
		Riff::Chunk*source = taskChunk->FindFirst(RID::path);
		outPath.Clear();
		if (source)
		{
			const count_t numEntries = source->size() / sizeof(Objects::PathNode);
			const Objects::PathNode*entries = (const Objects::PathNode*)source->pointer();
			//outPath.SetSize(numEntries,false);
			outPath.Append(entries,numEntries);
		}
	}

	static void Export(Riff::Chunk*taskChunk, const Objects::PVehicleJob&job)
	{
		if (job)
		{
			Riff::Chunk*baseChunk = taskChunk->AppendBlock(RID::baseJob);
			static ByteStream	stream;
			stream.Clear();
			stream << job->roundsToJobCompletion << job->resourceCostLeft;// << job->jobAutoCompletion << job->movesToFocusWhenDone;
			stream << job->ioID;
			baseChunk->SetData(stream);

			Objects::PVehicleJob parent = job->parent.lock();
			if (parent)
				taskChunk->AppendBlock(RID::mobileEntityParentJobIndex)->SetPODData(parent->ioID);

			Riff::Chunk*jobChunk = taskChunk->AppendBlock(job->GetDataKey());
			size_t dataSize = job->GetSerialSize(false);
			if (dataSize)
				ASSERT2__(SerializeToMemory(*job,jobChunk->SetData(dataSize),(ISerializable::serial_size_t)dataSize,false), job->GetName(),dataSize);
			//this fails sometimes. gotta investigate
		}
	}
	static void Import(Riff::Chunk*taskChunk, Objects::PVehicleJob&job)
	{
		job.reset();
		Riff::Chunk*source0 = taskChunk->FindFirst(RID::baseJob);
		if (!source0)
			return;

		for (index_t i = 0; i < Objects::VehicleJob::BaseJob::allJobPrototypes.Count(); i++)
		{
			auto jobType = Objects::VehicleJob::BaseJob::allJobPrototypes[i];
			Riff::Chunk*source = taskChunk->FindFirst(jobType->GetDataKey());
			if (source)
			{
				job = jobType->Instantiate();
				ASSERT2__(DeserializeFromMemory(*job,source->pointer(),source->size(),false), job->GetName(),source->size());


				source0->OpenStream();
				ASSERT__(source0->Stream(job->roundsToJobCompletion));
				ASSERT__(source0->Stream(job->resourceCostLeft));
				if (source0->size() == 12)
				{
					ASSERT__(source0->Stream(job->ioID));
				}
				//ASSERT__(source0->Stream(job->jobAutoCompletion));
				//ASSERT__(source0->Stream(job->movesToFocusWhenDone));
				ASSERT__(source0->CloseStream());


				GetOptionalPOD(taskChunk,RID::mobileEntityParentJobIndex,job->ioParentID,(UINT32)0);

				return;
			}
		}
		String found;
		Riff::Chunk*f = taskChunk->First();
		if (f)
			do
			{
				if (f->IsID(RID::baseJob))
					continue;
				found += " "+String(f->GetID().ToString());
			}
			while (f = taskChunk->Next());
		FATAL__("Unable to find matching job implementation. Found"+found);
	}

	static void Export(Riff::Chunk*parent, const Riff::TID&id, const String&outString)
	{
		parent->AppendBlock(id,outString.c_str(),outString.length());
	}

	static void Import(Riff::Chunk*parent, const Riff::TID&id, String&outString)
	{
		Riff::Chunk*tname = parent->FindFirst(id);
		ASSERT_NOT_NULL__(tname);
		outString.setLength(tname->size());
		memcpy(outString.mutablePointer(),tname->pointer(),tname->size());
	}
	static bool ImportOptional(Riff::Chunk*parent, const Riff::TID&id, String&outString)
	{
		Riff::Chunk*tname = parent->FindFirst(id);
		if (!tname)
			return false;
		outString.setLength(tname->size());
		memcpy(outString.mutablePointer(),tname->pointer(),tname->size());
		return true;
	}


	static void Export(Riff::Chunk*taskChunk, const Objects::Vehicle::Task&t)
	{
		Export(taskChunk,t.foci);
		Export(taskChunk,t.path);
		Export(taskChunk,RID::taskName,t.name);

		static ByteStream stream;
		stream.Clear();
		stream << (UINT16)t.mode << (UINT16)t.type;
		taskChunk->AppendBlock(RID::taskData,stream.pointer(),stream.GetFillLevel());
		Export(taskChunk,t.job);
	}

	static void Import(Riff::Chunk*taskChunk, Objects::Vehicle::Task&t)
	{
		Import(taskChunk,t.foci);
		Import(taskChunk,t.path);
		Import(taskChunk,RID::taskName,t.name);

		Riff::Chunk*tdata = taskChunk->FindFirst(RID::taskData);
		ASSERT_NOT_NULL__(tdata);
		tdata->OpenStream();
		UINT16 i16;
		ASSERT__(tdata->Stream(i16));
		t.mode = (Objects::Vehicle::mode_t)i16;
		ASSERT__(tdata->Stream(i16));
		t.type = (Objects::Vehicle::Task::type_t)i16;
		ASSERT__(tdata->CloseStream());
		Import(taskChunk,t.job);
	}


	NETSTRUCT4(WorkItemSerializable, 
				Package::NetString<256>, baseName, 
				Package::NetString<256>, turretName, 
				NET(TUniqueID), productID, 
				NET(TUniqueID), id);

	static void Export(Riff::Chunk*parent, const Objects::MobileEntityProducer::WorkItem&item)
	{
		WorkItemSerializable helper;
		if (item.producing->IsPureVehicle())
		{
			Objects::PVehicle v = To<Objects::Vehicle>::Cast(item.producing);
			helper.baseName = v->base->typeName;
			helper.turretName = v->turret->typeName;
		}
		else
		{
			helper.baseName = item.producing->GetTypeName();
		}
		helper.id = item.id;
		helper.productID = item.producing->id;
		serial_size_t size = helper.GetSerialSize(false);
		ASSERT1__(SerializeToMemory(helper,parent->AppendBlock(RID::vehiclePlantItem)->SetData(size),size,false),size);
	}
	static void Import(Riff::Chunk*source, Objects::MobileEntityProducer::WorkItem&item, const Objects::PFaction&owner)
	{
		ASSERT1__(source->IsID(RID::vehiclePlantItem),source->GetID().ToString());

		WorkItemSerializable helper;

		ASSERT1__(DeserializeFromMemory(helper,source->pointer(),source->size(),false),source->size());
		if (helper.turretName.IsNotEmpty())
		{
			item.producing = Objects::AssembleVehicle(owner,helper.productID,Grid::LCellIdI(0,0),helper.baseName,helper.turretName);
		}
		else
		{
			Objects::PWorldEntity proto = Objects::GetNonVehiclePrototypeByName(helper.baseName);
			ASSERT1__(proto,helper.baseName);
			ASSERT1__(proto->IsMobile(),proto->GetTypeName());
			item.producing = To<Objects::MobileEntity>::Cast(proto->CreateNewInstance(owner,Grid::LCellIdI(0,0),0));
		}
		item.id = helper.id;
	}

	static void Export(Riff::Chunk*parent, const Objects::MobileEntityProducer&p)
	{
		parent->AppendBlock(RID::vehiclePlant)->SetPODData(p.productionDone);
		if (p.gatheringPoint.current != Grid::LCellIdI::Invalid)
			parent->AppendBlock(RID::vehiclePlantGatheringPoint)->SetPODData(p.gatheringPoint.current);
		if (p.queue.IsNotEmpty())
		{
			Riff::Chunk*jobList = parent->AppendBlock(RIFF_LIST);
			foreach (p.queue,job)
				Export(jobList,*job);
		}
	}

	static void Import(Riff::Chunk*parent, Objects::MobileEntityProducer&producer, const Objects::PWorldEntity&selfReference)
	//const Objects::OwnerParameter&owner)
	{
		Riff::Chunk*vehiclePlantChunk = parent->FindFirst(RID::vehiclePlant);
		if (!vehiclePlantChunk)
		{
			LogEvent(EventClass::Preset::fileError,selfReference.get(),"Warning: Data chunk missing in file: MobileEntityProducer state. Skipping");
			return;
		}
//		ASSERT_NOT_NULL__(vehiclePlantChunk);
		ASSERT_EQUAL__(vehiclePlantChunk->size(),sizeof(producer.productionDone));
		vehiclePlantChunk->Get(&(producer.productionDone));

		GetOptionalPOD(parent,RID::vehiclePlantGatheringPoint,producer.gatheringPoint.current,Grid::LCellIdI::Invalid);


		Riff::Chunk*list = parent->FindFirst(RIFF_LIST);
		while (list)
		{
			//if (list->
			Riff::Chunk*entry = list->FindFirst(RID::vehiclePlantItem);
			if (entry)
			{
				do
				{
					Import(entry,producer.queue.Append(),selfReference->owner);
				}
				while (entry = list->FindNext(RID::vehiclePlantItem));

				break;
			}
			list = parent->FindNext(RIFF_LIST);
		}

	}


	enum class MobileEntityFlag : UINT16
	{
		None = 0x0,
		HasStartedJob = 0x1,
		IsAutomated = 0x2,
		IsStored = 0x4


	};

	void	operator|=(MobileEntityFlag&a, MobileEntityFlag b)
	{
		a = (MobileEntityFlag)((UINT16)a | (UINT16)b);
	}
	bool	operator&(MobileEntityFlag a, MobileEntityFlag b)
	{
		return ((UINT16)a & (UINT16)b) != 0;
	}

	Buffer0<Objects::PMobileEntity>	insertIntoHangar;

	static void Export(Riff::Chunk*parent, Objects::MobileEntity&v, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		Objects::PHangar store = v.storedIn.lock();
		if (store)
		{
			v.location = store->ToEntity().location;
		}
		v.TranslateLocationForExport();
		Export(parent,(Objects::WorldEntity&)v,factions);
		v.TranslateLocationFromExport();
		#ifndef ONE_COMPOUND_PER_FACTION
			Export(parent,v.currentStorage);
		#endif
		Export(parent,v.completeOnMove);
		UINT32 idAt = 0;
		foreach (v.myTasks,task)
		{
			if (task->job)
				task->job->ioID = idAt++;
		}
		foreach (v.myTasks,task)
		{
			Riff::Chunk*t = parent->AppendNamedList(ChunkName::VehicleTaskSection);
			Export(t,*task);
		}

		const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&v);
		if (p)
		{
			Export(parent,*p);
		}

		static ByteStream stream;
		stream.Clear();
		stream.Append(v.attackActionLeft);
		stream.Append(v.movementActionLeft);
		stream.Append(v.constructionRoundCounter);

		MobileEntityFlag flags = MobileEntityFlag::None;
		if (v.isAutomated)
			flags |= MobileEntityFlag::IsAutomated;
		if (v.jobStarted)
			flags |= MobileEntityFlag::HasStartedJob;
		if (store)
			flags |= MobileEntityFlag::IsStored;

		stream.Append(flags);
		//stream.Append(v.hasConstructedThisRound);
		//stream.Append(v.hasMoved);
		
		parent->AppendBlock(RID::vehicle,stream.pointer(),stream.GetFillLevel());
		if (v.IsPureVehicle())
		{
			Objects::Vehicle&veh = (Objects::Vehicle&)v;
			Export(parent,RID::vehicleTurret,veh.turret->typeName);
			Export(parent,RID::vehicleBase,veh.base->typeName);
		}
		else
		{
			Export(parent,RID::mobileEntityType,v.GetTypeName());
		}

		Export(parent,v.completeOnMove);
	}

	static bool Import(Riff::Chunk*parent, Riff::Chunk*vehicleChunk,const Objects::PMobileEntity&v, GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&factions)
	{
		//Grid::LCellIdI location;
		GetPOD(parent,RID::location,v->location);
		v->TranslateLocationFromExport();
		v->SetupSystem();
		 

		Import(parent,(Objects::WorldEntity&)*v,factions);
		#ifndef ONE_COMPOUND_PER_FACTION
			Import(parent,v->currentStorage);
			v->currentStorage.Cap(v->GetMaxStorageCapacity());
		#endif
		Import(parent,v->completeOnMove);
		Riff::Chunk *name,*data;
		if (parent->FindFirstNamedList(name,data))
			do
			{
				if (name->DataMatch(ChunkName::VehicleTaskSection))
				{
					Import(data,v->myTasks.Push());
				}
				else
				{
					FATAL__("key mismatch");
				}
			}
			while (parent->FindNextNamedList(name,data));
		foreach (v->myTasks,t)
			if (t->job && t->job->ioParentID)
			{
				bool found = false;
				foreach (v->myTasks,t2)
					if (t2->job && t2->job->ioID == t->job->ioParentID)
					{
						t->job->parent = t2->job;
						found = true;
						break;
					}
				if (!found)
					LogEvent(EventClass::Preset::error,"Warning unable to find parent #"+String(t->job->ioParentID)+" of "+t->name);
			}

		Objects::MobileEntityProducer*p = dynamic_cast<Objects::MobileEntityProducer*>(v.get());
		if (p)
		{
			Import(parent,*p,v);
		}


		vehicleChunk->OpenStream();		
		ASSERT__(vehicleChunk->Stream(v->attackActionLeft));
		ASSERT__(vehicleChunk->Stream(v->movementActionLeft));
		//ASSERT__(vehicleChunk->Stream(v.hasConstructedThisRound));
		//ASSERT__(vehicleChunk->Stream(v.hasMoved));
		ASSERT__(vehicleChunk->Stream(v->constructionRoundCounter));

		MobileEntityFlag flags;
		ASSERT__(vehicleChunk->Stream(flags));
		v->jobStarted = flags & MobileEntityFlag::HasStartedJob;
		v->isAutomated = flags & MobileEntityFlag::IsAutomated;
		ASSERT__(vehicleChunk->CloseStream());
		if (flags & MobileEntityFlag::IsStored)
		{
			insertIntoHangar << v;
		}

		Import(parent,v->completeOnMove);

		return !(flags & MobileEntityFlag::IsStored);
	}


	NETSTRUCT2(AttackItemSerializable, NET(bool), autoTarget, NET(Grid::LCellIdI), destination);

	

	static void Export(Riff::Chunk*parent, const Objects::Tower::AttackTask&item)
	{
		AttackItemSerializable helper;
		helper.destination = item.target;
		helper.autoTarget = item.autoTarget;
	//	helper.id = item.id;
		serial_size_t size = helper.GetSerialSize(false);
		ASSERT1__(SerializeToMemory(helper,parent->AppendBlock(RID::towerItem)->SetData(size),size,false),size);
	}
	static void Import(Riff::Chunk*source, Objects::Tower::AttackTask&item)
	{
		ASSERT1__(source->IsID(RID::towerItem),source->GetID().ToString());
		AttackItemSerializable helper;
		ASSERT__(DeserializeFromMemory(helper,source->pointer(),source->size(),false));
		item.target = helper.destination;
//		item.id = helper.id;
		item.autoTarget = helper.autoTarget;
	}



	static void Export(Riff::Chunk*parent, Objects::Structure&s, GenericHashSet<Objects::PFaction,SharedPtrHash>&factions)
	{
		Export(parent,(Objects::WorldEntity&)s,factions);
		Export(parent,RID::structureType,s.GetTypeName());
		const Objects::WorkingStructure*w = dynamic_cast<const Objects::WorkingStructure*>(&s);
//		state.enabled = !w || w->IsEnabled();
		if (w)
		{
			Objects::ResourceVector dontCare,resources;
			w->GetResourceVector(dontCare, resources, dontCare);
			Export(parent,resources);
			parent->AppendBlock(RID::structureEnabled)->SetPODData(w->IsEnabled());

			const Objects::MobileEntityProducer*p = dynamic_cast<const Objects::MobileEntityProducer*>(&s);
			if (p)
			{
				Export(parent,*p);
			}
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&s);
			if (t)
			{
				parent->AppendBlock(RID::towerActionLeft)->SetPODData(t->attackActionLeft);
				if (t->attackQueue.IsNotEmpty())
				{
					Riff::Chunk*jobList = parent->AppendBlock(RIFF_LIST);
					foreach (t->attackQueue,job)
						Export(jobList,*job);
				}
			}
			const Objects::NexusPort*n = dynamic_cast<const Objects::NexusPort*>(&s);
			if (n)
			{
				UINT32 data[2];
				data[0] = (UINT32)n->resistence;
				data[1] = n->lastSpawnRound.main;
				parent->AppendBlock(RID::nexusData)->SetData(data,sizeof(data));
			}

		}
		
	}

	
	static Objects::PStructure Import(Riff::Chunk*parent, GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>&factions)
	{
		using namespace ApplicationInterface;
		String typeName;
		Import(parent,RID::structureType,typeName);
		Grid::LCellIdI location;
		GetPOD(parent,RID::location,location);
		if (globalVersionOfCurrentFile <= 1038)
		{
			if (typeName == "VehicleStore")
				typeName = "Vehicle Store";
			elif (typeName == "NexusPort")
				typeName = "Nexus Port";
		}
		Objects::PStructure proto = Objects::GetPrototypeByName(typeName);

		if (globalVersionOfCurrentFile <= 1038)
		{
			if (!proto)
			{
				if (typeName == "Artillery Tower" || typeName == "Cannon Tower")
				{
					proto = Objects::GetPrototypeByName(ConstName::Type_LauncherTower);
				}
			}
		}

		if (!proto)
		{
			LogEvent(EventClass::Preset::fileError,"Warning: Unable to import structure of type '" +typeName+"', which is not supported by the local version");
			return Objects::PStructure();
		}
//		ASSERT1__(proto,typeName);


		Objects::PStructure rs = To<Objects::Structure>::Cast( proto->CreateNewInstance(Objects::PFaction(),location,0));



		Import(parent,(Objects::WorldEntity&)*rs,factions);

		Objects::WorkingStructure*w = dynamic_cast<Objects::WorkingStructure*>(rs.get());
//		state.enabled = !w || w->IsEnabled();
		if (w)
		{
			Objects::ResourceVector resources;
			Import(parent,resources);
			w->SetResourceStorage(resources);
			Riff::Chunk*enabled = parent->FindFirst(RID::structureEnabled);
			if (enabled)
			{
				bool isEnabled = *(const bool*)enabled->pointer();
				{
					float t =0;
					if (isEnabled)
						w->SetEnabled(isEnabled,t,false,Objects::WorkingStructure::ForceEnable,false);
					else
						w->SetEnabledOverride(false);
				}
			}
			else
			{
				LogEvent(EventClass::Preset::fileError,"Warning: Unable to find 'enabled' chunk for "+rs->GetName()+". Cannot determine enabled-state");
			}
			Objects::MobileEntityProducer*p = dynamic_cast<Objects::MobileEntityProducer*>(w);
	//		Objects::MobileEntityProducer*p = w->ToProducer();
			//Objects::VehiclePlant*p = dynamic_cast<Objects::VehiclePlant*>(rs.get());
			if (p)
			{
				Import(parent,*p,rs);
			}
			Objects::Tower*tower = dynamic_cast<Objects::Tower*>(rs.get());
			if (tower)
			{
				Riff::Chunk*towerActionChunk = parent->FindFirst(RID::towerActionLeft);
				ASSERT_NOT_NULL__(towerActionChunk);
				ASSERT_EQUAL__(towerActionChunk->size(),sizeof(tower->attackActionLeft));
				towerActionChunk->Get(&(tower->attackActionLeft));
				Riff::Chunk*list = parent->FindFirst(RIFF_LIST);
				if (list)
				{
					Riff::Chunk*entry = list->First();
					if (entry)
						do
						{
							Import(entry,tower->attackQueue.Push());
						}
						while (entry = list->Next());
				}
	
			}

			Objects::NexusPort*n = dynamic_cast<Objects::NexusPort*>(rs.get());
			if (n)
			{
				UINT32 data[2];

				GetBinary(parent,RID::nexusData,data,sizeof(data));

				n->resistence = (int)data[0];
				n->lastSpawnRound = data[1];

				n->OnLoad();
			}
		}


		return rs;
		
	}


	void SerializeToChunk(const ISerializable&data, Riff::Chunk*target)
	{
		static Array<BYTE> buffer;
		ISerializable::serial_size_t size = data.GetSerialSize(false);
		if (buffer.size() < size)
			buffer.SetSize(size);
		ASSERT1__(SerializeToMemory(data,buffer.pointer(),size,false),size);
		target->SetData(buffer.pointer(),size);
	}

	void	WriteToListChunk(Grid::WorldData&inData, Riff::Chunk&outChunk)
	{
		for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
		{
			TCP::Serial<Protocol::WorldDelta>	delta;
			delta.Ref().timestamp.rightNow = true;
			Protocol::GenerateWorldDelta(nullptr, &inData, delta.Ref(),k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::LocalCopy);
			SerializeToChunk(delta.Ref().data0,outChunk.AppendBlock(RID::worldTypeChunk));
		}
	}


	void	ReadFromListChunk(const Riff::Chunk&list, Grid::WorldData&outData)
	{
		Protocol::TypeDeltaArray	typeArray;

		const Riff::Chunk*child;
		BYTE atFamily = 0;
		if (child = list.GetFirstChild())
			do
			{
				if (child->IsID(RID::worldTypeChunk))
				{
					ASSERT__(DeserializeFromMemory(typeArray,child->pointer(),child->size(),false));
					ASSERT_LESS__(atFamily, BaseGrid::NumFamilies);
					outData.ApplyFamilyDelta(atFamily,typeArray,false,true);

					atFamily++;
				}
				else
					FATAL__("Unexpected chunk id "+String(child->GetID().ToString()));
			}
			while (child = child->GetNextSibling());
		ASSERT_EQUAL__(atFamily,BaseGrid::NumFamilies);
	}

	void	Load(const PathString&filename)
	{
		RuntimeConfig::loadingPhase = true;
		insertIntoHangar.Clear();

		Riff::Container file;
		bool failed = false;
		String filenameName = "'"+String(FileSystem::ExtractFileNameExt(filename))+ "'";
		if (!file.LoadFromFile(filename.c_str()))
		{
			LogEvent(EventClass::Preset::error,"File not found");
			failed = true;
		}
		else
		{
			Riff::Chunk*vers = file.FindFirst(RID::fileVersion);
			if (!vers)
			{
				LogEvent(EventClass::Preset::error,"Unable to read version number");

				failed = true;
			}
			else
			{
				UINT32 vid;
				if (vers->size() != 4)
				{
					LogEvent(EventClass::Preset::error,"Version size mismatch");

					failed = true;
				}
				else
				{
					vers->Get(&vid);
					if (vid < 1035)
					{
						LogEvent(EventClass::Preset::error,"Version not supported anymore. Expected >= 1035, found "+String(vid));

						failed = true;
					}
					else
						if (vid > ApplicationInterface::SupportedFileVersion)
						{
							LogEvent(EventClass::Preset::error,"Unsupported version: "+String(vid)+". Versions are supported up to "+String(ApplicationInterface::SupportedFileVersion));
							failed = true;
						}
						else
							ApplicationInterface::globalVersionOfCurrentFile = vid;
				}

			}
			UINT32 contentTypeCount = 0;
			GetOptionalPOD(&file,RID::worldContentTypeCount,contentTypeCount,(UINT32)12);
			if (contentTypeCount != Grid::Content::NumberOfPossibilities)
			{
				LogEvent(EventClass::Preset::error,"Grid content type count extracted from file is "+String(contentTypeCount)+". Local count is "+String(Grid::Content::NumberOfPossibilities));
				failed = true;
			}
			UINT32 exponent = 6;
			GetOptionalPOD(&file, RID::terrainExponent,exponent,6U);
			if (exponent != Grid::HighExponent)
			{
				LogEvent(EventClass::Preset::error,"Grid exponent extracted from file is "+String(exponent)+". Local exponent is "+String(Grid::HighExponent));
				failed = true;
			}
		}


		if (failed)
		{
			LogEvent(EventClass::Preset::error,"Unable to restore saved world state from '"+String(filename)+"'.");
			return;
		}


		LogEvent(EventSignificance::Uncommon,"Restoring saved world state from '"+String(filename)+"'.");
		bool wasOpen = ::Network::CloseSession();

		bool nexusFound = false, worldInfoFound = false;



		{
			Riff::Chunk*chunk = file.FindFirst(RID::researchScript);
			if (chunk)
			{
				SHA256::HashContainer id;
				SHA256::Hash(chunk->pointer(),chunk->size(),id);
				ScriptCache::PScript script = ScriptCache::Lookup(id);
				if (!script)
				{
					Array<BYTE>	data;
					chunk->Get(data);
					script = ScriptCache::DefineFromFleetingCompressedData(id,data,false);
				}
				if (script)
					Objects::ResearchState::ActivateProgram(script);
				else
					Objects::ResearchState::LoadDefaultProgram();
			}
		}

		GenericHashTable<TUniqueID,Objects::PFaction,TUniqueID>	factionMap;
		for (int round = 0; round < 2; round++)
		{
			Riff::Chunk*name,*list;
			if (file.FindFirstNamedList(name,list))
				do
				{
					ASSERT_EQUAL__(name->size(),2);
					UINT16 nameID;
					name->Get(&nameID);
					switch ((ChunkName)nameID)
					{
						case ChunkName::WorldInfoGroup:
							if (round == 0)
							{
								ASSERT__(!worldInfoFound);
								worldInfoFound = true;
								GetPOD(list,RID::worldID,World::worldID);
								GetPOD(list,RID::roundNumber,RuntimeConfig::roundNumber);
								GetOptionalPOD(list,RID::mostFactionsAlive,Game::mostFactionsAlive,(UINT32)0);

								Game::Config::Nexus::enableAI = list->FindFirst(RID::nexusAI) != nullptr;
								RuntimeConfig::empZones = false;

								if (ApplicationInterface::globalVersionOfCurrentFile < ApplicationInterface::FileVersions::FirstVersionWithVariables)
								{
									GetPOD(list,RID::closedGame,Game::Config::closedGame);
									GetOptionalPOD(list,RID::nexusBases,Game::Config::Nexus::haveBases,false);
									UINT16 goal;
									GetPOD(list,RID::goal,goal);
									if (!RuntimeConfig::gameGoal.Load(goal))
									{
										if (goal == 2)
										{
											RuntimeConfig::empZones = true;
											RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::ConquerNexus;
										}
										else
										{
											LogEvent(EventSignificance::NonFatalError,"Loaded unexpected goal index "+String(goal));
											RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::None;
										}
									}
									GetPOD(list,RID::startBases,Game::Config::startBases);
									GetPOD(list,RID::basesRequired,Game::Config::basesRequired );
									GetOptionalPOD(list,RID::newVehiclesHaveFullAmmo,Objects::newVehiclesHaveFullAmmo,true);
									GetOptionalPOD(list,RID::exposeActionLeftToHostileFactions,Game::Config::exposeActionLeftToHostileFactions,true);
									{
										UINT8 v;
										GetOptionalPOD(list,RID::doomsdayWeapons,v,(UINT8)0);
										RuntimeConfig::doomsdayWeapons.Load(v);
									}
									GetOptionalPOD(list,RID::factionSupportFlags,Game::Config::factionSupportFlags,Protocol::GameFlag::AllFactionSupport);
								}
								else
								{
									Riff::Chunk*chk = list->FindFirst(RID::variableConfiguration);
									ASSERT_NOT_NULL__(chk);
									String variables(chk->pointer(),chk->size());
									Array<String> lines;
									explode('\n',variables,lines);
									foreach (lines,line)
									{
										if (line->IsEmpty())
											continue;
										index_t tabAt = line->find(tab);
										ASSERT__(tabAt);
										if (tabAt)
										{
											String key = "/"+line->subStringRef(0,tabAt-1);
											StringRef value = line->subStringRef((int)tabAt);
		
											CLI::PVariable v = Reflection::state.FindVar(key);
											if (!v)
											{
												LogEvent(EventClass::Preset::error,"State::Load(): Unable to find variable '"+key+"' for update");
												continue;
											}
											if (!v->Set(value))
											{
												LogEvent(EventClass::Preset::error,"State::Load(): Unable to update variable '"+key+"' to '"+value+"'. Supported values are "+v->GetSupportedValues());
												if (v->name == "gameGoal" && value == "EMPConquerNexus")
												{
													RuntimeConfig::empZones = true;
													RuntimeConfig::gameGoal = RuntimeConfig::GameGoal::ConquerNexus;
												}
											}
										}
									}


								}
								//Grid::TSharedPlanetConfig persistentPlanetInfo;
								GetPOD(list,RID::worldConfig,(Grid::TSharedPlanetConfig&)Grid::currentPlanetConfig);
								Grid::CellStack::bottom.content = (BYTE)Grid::currentPlanetConfig.innerCoreType;


							}
						break;
						case ChunkName::ObjectGroup:
							if (round == 1)
							{
						
								Riff::Chunk*child;
								if (child = list->First())
									do
									{
										if (child->IsID(RIFF_LIST))
										{
											Riff::Chunk*vehicleChunk = child->FindFirst(RID::vehicle);
											if (vehicleChunk)
											{
												String base,turret;
												if (!ImportOptional(child,RID::mobileEntityType,base))
												{
													Import(child,RID::vehicleBase,base);
													Import(child,RID::vehicleTurret,turret);
												}
												Objects::PMobileEntity v = Objects::AssembleMobileEntity(Objects::PFaction(),TUniqueID(),Grid::LCellIdI(0,0),base,turret);
												if (!v)
												{
													LogEvent(EventSignificance::NonFatalError,"Unable to assemble mobile entity ("+base+","+turret+")");
												}

												bool doInsert = v && Import(child,vehicleChunk,v,factionMap);
												#if defined CHEAT_FULL_STORAGE && !defined ONE_COMPOUND_PER_FACTION
													 v->currentStorage = v->GetMaxStorageCapacity();
												#endif
												if (doInsert)
												{
													float t =0;
													Objects::InsertConstructed(v,t,true,false);
												}
												else
												{
													//Objects::storedVehicles << v;
													//v->owner->storedVehicles << v;
													//Objects::idMap.Set(v->id,v);
													//v->isInserted = v->isInsertedIntoGrid = false;
												}
											}
											else
											{
												//child->FindFirst(RID::vehiclePlant),child->FindFirst(RID::towerActionLeft)
												Objects::PStructure s = Import(child,factionMap);
												if (s)
												{
													float t =0;
													Objects::InsertConstructed(s,t,false,false);
												}
											}
										}
										else
											FATAL__("Encountered unexpected block type in object-group: "+String(child->GetID().ToString()));
									}
									while (child = list->Next());


								Objects::RefreshCompoundCoverage(0);

							}
						break;
						case ChunkName::WorldGroup:
							if (round == 0)
							{
								ReadFromListChunk(*list,Grid::actualWorld);
							}
						break;
						case ChunkName::InitialWorldGroup:
							if (round == 0)
							{
								ReadFromListChunk(*list,Grid::initialWorld);
							}
						break;
						case ChunkName::ResourceSection:
							if (round == 0)
							{
								String resourceName;
								Import(list,RID::resourceName,resourceName);
								Grid::Resource&res = Grid::Resource::Require(resourceName);
								GetBinary(list,RID::resourceCoverage,res.coverage,sizeof(res.coverage));
							}
						break;
						case ChunkName::FactionSection:
							if (round == 0)
							{
								Objects::PFaction faction;
								TUniqueID factionID;
								GetPOD(list,RID::factionID,factionID);
							
								bool isNexus = false;
								if (list->FindFirst(RID::nexusTag))
								{
									isNexus = true;
									ASSERT__(!nexusFound);
									nexusFound = true;
									Objects::Faction::RecreateNexus(factionID);
									faction = Objects::Faction::nexus;
									LogEvent(EventSignificance::Common,"Loading nexus faction");
								}
								else
								{
									faction.reset(new Objects::Faction(factionID));
									Objects::Faction::all << faction;

									faction->currentState.FlushAllData();
									//foreach (faction->currentState,stat)
									//	stat->ZeroInit();
								}
								GetPOD(list,RID::factionAccessToken,faction->accessToken);

								Riff::Chunk*researchStateChunk = list->FindFirst(RID::researchState);
								if (researchStateChunk)
								{
									faction->researchState.Import(researchStateChunk->ReferenceData());


									Riff::Chunk*activeResearchChunk = list->FindFirst(RID::activeResearchTopic);
									if (activeResearchChunk)
										faction->researchState.SetActiveTopic(Objects::ResearchState::FindTopic(String((const char*)activeResearchChunk->pointer(),activeResearchChunk->size())));
								}

								Riff::Chunk*nameChunk = list->FindFirst(RID::factionName);
								if (nameChunk)
								{
									faction->name.setLength(nameChunk->size());
									nameChunk->Get(faction->name.mutablePointer());
								}
								else
									faction->name = isNexus ? "<Nexus>" : faction->id.ToString();
							
								if (ApplicationInterface::globalVersionOfCurrentFile >= 1038)
								{
									Riff::Chunk*markerChunk = list->FindFirst(RID::factionMarkers);
									if (markerChunk)
									{
										Array<Objects::Marker>	markers;
										ASSERT__(DeserializeFromMemory(markers, markerChunk->pointer(), markerChunk->size(), false));
										faction->RestoreMarkers(markers);
									}
								}

								factionMap.Set(faction->id,faction);
								if (!isNexus)
									::Network::tokenMap.Set(faction->accessToken,faction);	//dont even allow access if nexus

								//BYTE k = 0;
								//Protocol::WorldDelta	delta;
								Riff::Chunk*child;

								Protocol::TypeDeltaArray	typeArray;
								//Riff::Chunk*child;
								BYTE atFamily = 0;

								if (child = list->First())
									do
									{
									
											//factionWorldTypeChunk("WTCK"),
											//factionWorldCounterChunk("WCCK"),

										if (child->IsID(RID::factionWorldTypeChunk))
										{
											ASSERT__(DeserializeFromMemory(typeArray,child->pointer(),child->size(),false));
											ASSERT_LESS__(atFamily, BaseGrid::NumFamilies);
											faction->currentState.ApplyFamilyDelta(atFamily,typeArray,false,false);
											atFamily++;
										}
										//elif (child->IsID(RID::factionWorldCounterChunk))
										//{
										//	ASSERT__(DeserializeFromMemory(countArray,child->pointer(),child->size(),false));
										//	ASSERT_LESS__(atFamily, BaseGrid::NumFamilies);
										//	faction->currentState.ApplyFamilyDelta(atFamily,typeArray,countArray,false,false);

										//	atFamily++;
										//}
								
									}
									while (child = list->Next());
								ASSERT_EQUAL__(atFamily,BaseGrid::NumFamilies);


								Riff::Chunk*compressed = list->FindFirst(RID::resourcesDiscoveredCompressed);
								if (compressed)
								{
									const UINT32*indices = (const UINT32*)compressed->pointer();
									const count_t numIndices = compressed->size() / sizeof(UINT32);
									for (UINT32 i = 0; i < numIndices; i++)
										faction->visibility.resourcesDiscovered[indices[i]] = true;
								}
								else
								{
									Riff::Chunk*raw = list->FindFirst(RID::resourcesDiscoveredFull);
									if (raw)
									{
										ASSERT_EQUAL__(raw->size(),faction->visibility.resourcesDiscovered.GetContentSize());
										memcpy(faction->visibility.resourcesDiscovered.pointer(),raw->pointer(),raw->size());
									}
								}
							}
						break;
						default:
							FATAL__("Unsupported object group encountered: "+String(nameID));

					}

				}
				while (file.FindNextNamedList(name,list));
		}





		for (const Objects::PMobileEntity&v : insertIntoHangar)
		{
			Objects::PHangar s = To<Objects::Hangar>::DynamicCast(Objects::GetEntity(v->location));
			ASSERT2__(s,v->GetName(),v->location.ToString());
			s->StoreLocallyFromImport(v);
		}
		insertIntoHangar.Clear();

		Objects::ForeachWorldEntity([](const Objects::PWorldEntity&w)
		{
			w->ResetAllChange();
		},false);

		lastSaveRound = RuntimeConfig::roundNumber;
		ASSERT__(worldInfoFound);
		//ASSERT__(nexusFound);	if it has no units, it is not stored



		if (RuntimeConfig::empZones)
		{
			count_t numZones = Game::Config::CountAccessibleZones();

			int unlocked = 0;
			for (index_t i = 0; i < numZones; i++)
			{
				if (Game::Config::Nexus::haveBases && BaseGrid::IsIsolatedCorner(unlocked))
				{
					unlocked++;
				}
				unlocked++;
			}
				

			World::SetUnlockedEMPZones(unlocked);
		}

		Reflection::LoadOverrides();

		LogEvent(EventSignificance::Uncommon,"Restored world '"+World::worldID.ToNiceString()+"'/"+World::worldID.ToString()+" in round "+RuntimeConfig::roundNumber.ToString());
		RefreshWindowTitle();

		RuntimeConfig::loadingPhase = false;


		if (wasOpen)
			::Network::RestartSession(true);

	}


	PathString GetAutoSavePath(index_t index)
	{
		return RuntimeConfig::ConfigFile("saves/auto/snapshot") + PathString(index)+"."+savFileExtension;
	}

	void AutoSave()
	{
		PathString base = RuntimeConfig::ConfigFile("saves/auto/snapshot");
		FileSystem::CreateDirectory(FileSystem::ExtractFileDir(base));
		if (FileSystem::IsFile(base+"0."+savFileExtension))
		{
			FileSystem::UnlinkFile(base+"9."+savFileExtension);
			for (int i = 8; i >= 0; i--)
				FileSystem::MoveFile(base+PathString(i)+'.'+savFileExtension,base+PathString(i+1)+'.'+savFileExtension);
		}
		Save(base+"0."+savFileExtension);

		#ifdef SAVE_FILE_TEST
			State::Load(base+"0."+savFileExtension);
			State::Save("test1."+savFileExtension);

			Riff::File a,b;
			ASSERT__(a.Open((base+"0."+savFileExtension).c_str()));
			ASSERT__(b.Open("test1."+savFileExtension));
			Buffer<Riff::TID>	stack;
			Compare(a,b,stack);
		#endif

		lastSaveRound = RuntimeConfig::roundNumber;
	}

	void	WriteActualWorldToContainer(Riff::Chunk&container)
	{
		Riff::Chunk*world = CreateNamedList(container,ChunkName::WorldGroup);
		WriteToListChunk(Grid::actualWorld,*world);
		Riff::Chunk*initialWorld = CreateNamedList(container,ChunkName::InitialWorldGroup);
		WriteToListChunk(Grid::initialWorld,*initialWorld);
	}

	void	Save( const PathString&name)
	{
		static_assert(sizeof(TUniqueID) == TUniqueID::NumBytes, "Alignment fault" );

		Timer::Time t0 = timer.Now();

		Riff::Container container;
		UINT32 vid = ApplicationInterface::SupportedFileVersion;
		container.AppendBlock(RID::fileVersion,&vid,sizeof(vid));
		container.AppendBlock(RID::worldContentTypeCount)->SetPODData((UINT32)Grid::Content::NumberOfPossibilities);
		container.AppendBlock(RID::terrainExponent)->SetPODData((UINT32)Grid::HighExponent);



		Riff::Chunk*general = CreateNamedList(container,ChunkName::WorldInfoGroup);
		general->AppendBlock(RID::worldID)->SetPODData(World::worldID);
		general->AppendBlock(RID::roundNumber)->SetPODData(RuntimeConfig::roundNumber);
		general->AppendBlock(RID::worldConfig)->SetPODData((const Grid::TSharedPlanetConfig&)Grid::currentPlanetConfig);
		general->AppendBlock(RID::mostFactionsAlive)->SetPODData(Game::mostFactionsAlive);

		{
			static StringBuffer variableBuffer;
			variableBuffer.Clear();
			Reflection::VisitAllVariables([](const Reflection::Variable::PBase&v)
			{
				if (v->WriteToSavegame())
				{
					variableBuffer << v->GetPath()<<tab<<v->ToString()<<nl;
				}
			});
			general->AppendBlock(RID::variableConfiguration,variableBuffer.pointer(),variableBuffer.GetContentSize());
		}

		if (Objects::ResearchState::globalState.script)
			general->AppendBlock(RID::researchScript)->SetData(Objects::ResearchState::globalState.script->compressed);

		//general->AppendBlock(RID::factionSupportFlags)->SetPODData(State::Config::factionSupportFlags);
		//general->AppendBlock(RID::closedGame)->SetPODData(State::Config::closedGame);
		//general->AppendBlock(RID::nexusBases)->SetPODData(State::Config::Nexus::haveBases);
		//general->AppendBlock(RID::goal)->SetPODData((UINT16)RuntimeConfig::gameGoal);
		//if (State::Config::Nexus::enableAI)
			//general->AppendBlock(RID::nexusAI);
		//general->AppendBlock(RID::startBases)->SetPODData(State::Config::startBases);
		//general->AppendBlock(RID::basesRequired )->SetPODData(State::Config::basesRequired );
		//general->AppendBlock(RID::newVehiclesHaveFullAmmo)->SetPODData(Objects::newVehiclesHaveFullAmmo);
		//general->AppendBlock(RID::exposeActionLeftToHostileFactions )->SetPODData(State::Config::exposeActionLeftToHostileFactions );
		//general->AppendBlock(RID::doomsdayWeapons)->SetPODData((UINT8)RuntimeConfig::doomsdayWeapons);

		GenericHashSet<Objects::PFaction,SharedPtrHash>	allFactions;

		WriteActualWorldToContainer(container);


		Riff::Chunk*objects = CreateNamedList(container,ChunkName::ObjectGroup);
		Objects::ForeachMobileEntityIncludingStored([objects,&allFactions](const Objects::PMobileEntity&_v)
		{
			Objects::MobileEntity&v = *_v;
			Export(objects->AppendBlock(RIFF_LIST),v,allFactions);
		},false);



		foreach (Objects::structures,_s)
		{
			Objects::Structure&s = **_s;
			Export(objects->AppendBlock(RIFF_LIST),s,allFactions);
		}


		foreach (Grid::Resource::all,res)
		{
			Riff::Chunk*resSec = CreateNamedList(container,ChunkName::ResourceSection);
			resSec->AppendBlock(RID::resourceName)->SetData(res->GetName().c_str(),res->GetName().length());
			resSec->AppendBlock(RID::resourceCoverage)->SetData(res->coverage,sizeof(res->coverage));
		}










		//Riff::Chunk*factions = CreateNamedList(container,ChunkName::FactionGroup);
		allFactions.VisitAllKeys([&container](const Objects::PFaction&faction)
		{
			Riff::Chunk*factionSection = CreateNamedList(container,ChunkName::FactionSection);
		
			if (faction == Objects::Faction::nexus)
			{
				factionSection->AppendBlock(RID::nexusTag);
			}

			factionSection->AppendBlock(RID::factionID)->SetPODData(faction->id);	//faction id
			factionSection->AppendBlock(RID::factionName)->SetData(faction->name.c_str(),faction->name.length());

			factionSection->AppendBlock(RID::factionAccessToken)->SetPODData(faction->accessToken);	//faction access token


			if (faction->researchState.researchState.IsSet())
			{
				Array<BYTE> data;
				faction->researchState.Export(data);
				factionSection->AppendBlock(RID::researchState)->SetData(data);

				auto res = faction->researchState.GetActiveTopic();
				if (res)
				{
					String topic = res->GetPath();
					factionSection->AppendBlock(RID::activeResearchTopic)->SetData(topic.c_str(),topic.length());
				}
			}


			if (faction->markers.IsNotEmpty())
			{
				Riff::Chunk*markerChunk = factionSection->AppendBlock(RID::factionMarkers);
				SerializeToChunk(faction->markers.CopyToArray(), markerChunk);
			}



			for (BYTE k = 0; k < BaseGrid::NumFamilies; k++)
			{
				TCP::Serial<Protocol::WorldDelta>	delta;
				delta.Ref().timestamp.rightNow = true;
				Protocol::GenerateWorldDelta(nullptr,&faction->currentState, delta.Ref(),k,true,k+1 == BaseGrid::NumFamilies,Protocol::WorldDeltaType::LocalCopy);
				delta.Ref().timestamp.atTime = 0;

				SerializeToChunk(delta.Ref().data0, factionSection->AppendBlock(RID::factionWorldTypeChunk));
			}

			static Buffer0<UINT32>	final;
			final.Clear();

			for (index_t i = 0; i < faction->visibility.resourcesDiscovered.Count(); i++)
			{
				if (faction->visibility.resourcesDiscovered[i])
					final << (UINT32)i;
			}

			if (final.IsNotEmpty())
			{
				if (final.Count()*4 < faction->visibility.resourcesDiscovered.Count())
					factionSection->AppendBlock(RID::resourcesDiscoveredCompressed)->SetData(final.pointer(),final.Count()*4);	//resource discovered - compressed
				else
					factionSection->AppendBlock(RID::resourcesDiscoveredFull)->SetData(faction->visibility.resourcesDiscovered);	//resource discovered - full
			}


		});
		//LogEvent(EventType::SessionRelated,"World snapshot created in "+String(timer.GetSecondsSince(t0))+" seconds");


		if (container.SaveToFile(name.c_str()))
		{
			LogEvent(EventSignificance::Debug,"World snapshot written to "+String(name)+" in "+String(timer.GetSecondsSince(t0))+" seconds");
			LogEvent(EventSignificance::Uncommon,"World snapshot created in "+String(timer.GetSecondsSince(t0))+" seconds");
		}
		else
			LogEvent(EventClass::Preset::error,"Unable to write world to "+String(name));
		//Grid::SaveToFiles(name+".grid");
		//Objects::SaveToFiles(name+".objects");




	}




	ChunkName Compare1(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent="")
	{
		ASSERT2__(a.GetID() == b.GetID(),a.GetID().ToString(),b.GetID().ToString());
		std::cout << indent << a.GetID().ToString() << std::endl;
		if (a.IsID(Riff::RIFF_NAME_CHUNK))
		{
			ASSERT__(a.GetSize() == 2);
			ASSERT__(b.GetSize() == 2);
			UINT16 na,nb;
			a.Get(&na);
			b.Get(&nb);
			ASSERT_EQUAL__(na,nb);
			std::cout << indent << "<name="<<na<<">" << std::endl;
			return (ChunkName)na;

		}
		if (a.IsID(RIFF_LIST) || a.IsID(RIFF_RIFF))
		{
			std::cout << indent << "<list>" << std::endl;
			idStack << a.GetID();
			a.Enter();
			b.Enter();
				bool f0 = a.First();
				bool f1 = b.First();
				ChunkName lastName;
				ASSERT_EQUAL__(f0,f1);
				if (f0 && f1)
					do
					{
						ChunkName cn = Compare1(a,b,idStack,indent+" ");
						if (cn != ChunkName::None)
							lastName = cn;

						f0 = a.Next();
						f1 = b.Next();
						ASSERT_EQUAL__(f0,f1);
					}
					while (f0&&f1);

			a.Exit();
			b.Exit();
			idStack.EraseLast();
		}
		else
		{
			static Buffer<BYTE>	data0,data1;
			data0.Clear(); data1.Clear();
			a.Get(data0.AppendRow(a.GetSize()));
			b.Get(data1.AppendRow(b.GetSize()));
			ASSERT_EQUAL__(a.GetSize(),b.GetSize());
			std::cout << indent << "<data>["<<a.GetSize()<<"]" << std::endl;
			ASSERT__(!memcmp(data0.pointer(),data1.pointer(),data0.GetContentSize()));
		}


		return ChunkName::None;
	}


	void Compare(Riff::File&a, Riff::File&b, Buffer<Riff::TID>&idStack, const String&indent/*=""*/)
	{
		bool f0 = a.First();
		bool f1 = b.First();
		ChunkName lastName;
		ASSERT_EQUAL__(f0,f1);
		if (f0 && f1)
			do
			{
				ChunkName cn = Compare1(a,b,idStack,indent+" ");
				if (cn != ChunkName::None)
					lastName = cn;

				f0 = a.Next();
				f1 = b.Next();
				ASSERT_EQUAL__(f0,f1);
			}
			while (f0&&f1);
	}



}

