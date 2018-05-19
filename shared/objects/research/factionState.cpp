#include "factionState.h"
#include "../faction.h"
#include "attributes.h"
#ifdef CLIENT
	#include "../../expectedBehavior.h"
#endif


namespace Objects
{


	#ifndef CLIENT

		void Research::FactionState::LoadNexusResearchState()
		{
			foreach (globalState.allTopics,_t)
			{
				auto&t = **_t;
				if (t.setProgress.IsSet())
				{
					if (t.maxProgress > 0)
						t.setProgress(luaResearchState,t.maxProgress);
				}
			}
			UpdateAttributeTable();
		}

		void					Research::FactionState::AdvanceNexusResearch()
		{
			DBG_ASSERT__(factionIsNexus);

			foreach (globalState.allTopics,_t)
			{
				auto&t = **_t;
				if (t.setProgress.IsSet())
				{
					if (t.maxProgress < 0)
						topicProgressVector[ t.serialIndex ] ++;
				}
			}
			UpdateAttributeTable();
		}


		void					Research::FactionState::FinalizeRound(const PFaction&f)
		{
			attributeTable.ClearChanges();

			while (researchQueue.IsNotEmpty() && totalContribution)
			{
				int left;
				const auto&res = researchQueue.First();
				int&progress = topicProgressVector[ res->serialIndex ];
				if (res->maxProgress >= 0)
				{
					left = res->maxProgress - progress;
					if (left <= 0)	//already done...?
					{
						LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
						res->setProgress(luaResearchState,res->maxProgress);
						researchQueue.Erase(0);
						queueChanged = true;
						globalState.onQueueChange(owner);
						continue;
					}
					topicProgressChanged = true;
					if (totalContribution < left)	//no chance to get done
					{
						progress += totalContribution;
						res->setProgress(luaResearchState,progress);
						break;
					}
					totalContribution -= left;
					progress = res->maxProgress;
					res->setProgress(luaResearchState,res->maxProgress);
					LogEvent(EventClass::Preset::important,owner->shared_from_this(),"Research "+res->name+" completed");
					researchQueue.Erase(0);
					queueChanged = true;
					globalState.onQueueChange(owner);
					continue;
				}
				progress += totalContribution;
				res->setProgress(luaResearchState,progress);
				topicProgressChanged = true;

				if (researchQueue.Count() > 1)
				{
					LogEvent(EventClass::Preset::common,owner->shared_from_this(),"Multiple research topics defined after "+res->name+". Cycling");
					auto topic = res;
					researchQueue.Erase(0);
					researchQueue.Append(topic);
					queueChanged = true;
					globalState.onQueueChange(owner);
				}

				break;
			}
			lastSpeed = totalContribution;
			totalContribution = 0;
			if (topicProgressChanged)
				UpdateAttributeTable();
		}


		//void					Research::FactionState::ImportResearchState(const Research::TopicProgressVector&archived)
		//{
		//	if (archived.Count() != globalState.allTopics.Count())
		//		return;

		//	topicProgressVector = archived;
		//	luaResearchState = globalState.stateConstructor();

		//	foreach (globalState.allTopics,t)
		//	{
		//		if ((*t)->setProgress.IsSet())
		//			(*t)->setProgress(luaResearchState,archived[t-globalState.allTopics.pointer()]);
		//	}

		//	UpdateAttributeTable();
		//}

	
		void					Research::FactionState::ImportLuaState(const ArrayRef<const BYTE>&serializedState)
		{
			if (!globalState.researchProgram)
			{
				return;
			}
			try
			{
				#ifdef CLIENT
					World::actual.InvalidateSensory();
				#else
					owner->knownWorld.InvalidateSensory();
				#endif

				Lua::Reference deserialized = globalState.researchProgram->DeserializeToReference(ByteReadStream(serializedState));
				globalState.importState(luaResearchState,deserialized);

				ASSERT_EQUAL__(globalState.allTopics.Count(), topicProgressVector.Count());
				foreach (globalState.allTopics,t)
				{
					const index_t at = t - globalState.allTopics.pointer();
					if ((*t)->getProgress.IsSet())
						topicProgressVector[at] = (*t)->getProgress(luaResearchState);
					else
						topicProgressVector[at] = -1;
				}

			}
			catch (const std::exception&e)
			{
				LogEvent(EventClass::Preset::error,e.what());
			}
			UpdateAttributeTable();
		}
	
	
		void					Research::FactionState::ExportLuaState(Array<BYTE>&outSerializedState) const
		{
			//Buffer0<BYTE> data;
			if (globalState.exportState.IsEmpty() || luaResearchState.IsEmpty())
			{
				outSerializedState.Clear();
				return;
			}
			try
			{
				Lua::Reference exported = globalState.exportState(luaResearchState);
				ASSERT__(exported.IsSet());
				ByteStreamBuffer stream;
				exported.Serialize(stream);
				outSerializedState = stream.GetWrittenData();
			}
			catch (const std::exception&e)
			{
				LogEvent(EventClass::Preset::error,e.what());
				outSerializedState.Clear();
			}
		}

		void Research::FactionState::UpdateAttributeTable()
		{
			attributeTable.Update(luaResearchState);
			topicProgressChanged = true;	//implied
		}
	

	#else

		/*static*/ void				Research::FactionState::BeginServerTopicUpdate()
		{
			globalState.rootResearch.Clear();
			globalState.allTopics.Clear();
		}

		/*static*/ void				Research::FactionState::DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex)
		{
			Topic::Create(parentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,parentTopicIndex,CLOCATION) : nullptr,topicName,iconName,Research::Topic::FSetProgress(),Research::Topic::FGetProgress(), maxProgress,dependentTopicIndex != InvalidIndex ? Expect::SafeGet(globalState.allTopics,dependentTopicIndex,CLOCATION):nullptr);
		}
	#endif


	Research::FactionState::FactionState( Faction*owner, bool isNexus ):owner(owner)
	{
		factionIsNexus = isNexus;
		
		topicProgressVector.SetSize(globalState.allTopics.Count());
		topicProgressVector.Fill(0);
		#ifndef CLIENT
		if (globalState.script)
		{
			luaResearchState = globalState.stateConstructor();
			if (isNexus)
				LoadNexusResearchState();
			else
				UpdateAttributeTable();
		}
		#endif
	}
	
	bool					Research::TopicGroup::Insert(const Research::PTopic&t)
	{
		if (Find(t->name))
			return false;
		map.Set(t->name,t);
		children << t;
		return true;
	}


	Research::PTopic			Research::TopicGroup::Find(const StringRef&name) const
	{
		//Research::PTopic t;
		//map.Query(name,t);
		//return t;
		foreach (children,c)
			if ((*c)->name == name)	//i suspect this is actually faster for slow numbers of topics per family, since it does not force a String copy
				return *c;
		return Research::PTopic();
	}


	static Research::PTopic FindTopicOfPrototypeIn(const Research::PTopic&topic, const PWorldEntity&proto)
	{
		foreach (topic->children,_c)
		{
			const auto&t = *_c;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}

	static Research::PTopic FindTopicOfPrototypeIn(const Research::PTopic&topic, const Component::Base*proto)
	{
		foreach (topic->children,_c)
		{
			const auto&t = *_c;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->componentReference == proto)
				return t;
		}
		return nullptr;
	}

	#if 0
	/*static*/ Research::PTopic	Research::FactionState::FindTopicOfPrototype(const Component::Base*proto)
	{
		foreach (globalState.allTopics,_t)
		{
			const auto&t = *_t;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->componentReference == proto)
				return t;
		}
		return nullptr;
	}

	/*static*/ Research::PTopic	Research::FactionState::FindTopicOfPrototype(const PWorldEntity&proto)
	{
		foreach (globalState.allTopics,_t)
		{
			const auto&t = *_t;
			if (t->IsGroup())
			{
				const auto&rs = FindTopicOfPrototypeIn(t,proto);
				if (rs)
					return rs;
			}
			if (t->prototypeReference == proto)
				return t;
		}
		return nullptr;
	}
	#endif

	/*static*/ Research::PTopic	Research::FindTopic(const StringRef&path)
	{
		Ctr::Array<StringRef>	components;
		explode('/',path,components);
		Research::PTopic	current = globalState.rootResearch.Find(components.First());
		for (index_t i = 1; i < components.Count(); i++)
		{
			if (!current)
				return Research::PTopic();
			current = current->Find(components[i]);
		}
		return current;
	}

	/*static*/ Research::PTopic	Research::GetTopic(index_t serialIndex)
	{
		return serialIndex < globalState.allTopics.Count() ? globalState.allTopics[serialIndex] : Research::PTopic();
	}

	bool	Research::FactionState::ResearchIsPlanned(const Research::PTopic&topic) const
	{
		return researchQueue.Contains(topic);
	}

	bool	Research::FactionState::ResearchQueueIsFull() const
	{
		return researchQueue.Count() >= MaxResearchQueueLength;
	}

	bool	Research::FactionState::AppendResearchTask(const Research::PTopic&active)
	{
		if (!active || ResearchQueueIsFull() || !active->CanResearch(topicProgressVector,researchQueue.ToRef()) || researchQueue.Contains(active))
			return false;
		researchQueue << active;
		return true;
	}

	bool					Research::FactionState::DropResearchTask(const Research::PTopic&active)
	{
		if (!active)
			return false;
		index_t at = researchQueue.GetIndexOf(active);
		if (at == InvalidIndex)
			return false;
		researchQueue.Erase(at);
		for (index_t i = at; i < researchQueue.Count();)
		{
			if (!researchQueue[i]->CanResearch(topicProgressVector,researchQueue.SubRef(0,i)))
			{
				LogEvent(EventClass::Preset::operationAborted,owner->shared_from_this(),"Unable to continue research "+researchQueue[i]->GetFullName()+". Preconditions lost. Aborting operation");
				researchQueue.Erase(i);
			}
			else
				i++;
		}
		return true;
	}


	void		Research::FactionState::RestoreResearchQueue(const String&source)
	{
		researchQueue.Clear();
		Ctr::Array<StringRef> paths;
		explode(';',source,paths);
		foreach (paths,p)
			AppendResearchTask(FindTopic(*p));
		globalState.onQueueChange(owner);
	}

	String		Research::FactionState::ExportResearchQueue()
	{
		if (researchQueue.IsEmpty())
			return "";
		StringBuffer buffer;
		foreach (researchQueue,q)
		{
			if (buffer.IsNotEmpty())
				buffer << ';';
			buffer << (*q)->GetPath();
		}
		return buffer.CopyToString();
	}

	void		Research::FactionState::ExportResearchQueue(Ctr::Array<UINT32>&outRS)
	{
		outRS.SetSize(researchQueue.Count());
		for (index_t i = 0; i < researchQueue.Count(); i++)
			outRS[i] = ((UINT32)(researchQueue[i]->serialIndex));
	}
	void					Research::FactionState::RestoreResearchQueue(const Ctr::Array<UINT32>&q)
	{
		researchQueue.Clear();
		foreach (q,e)
			AppendResearchTask(GetTopic(*e));
		globalState.onQueueChange(owner);
	}






}

