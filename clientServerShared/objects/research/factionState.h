#pragma once

#include <functional>
#include <container/vectorList.h>
#include "globalState.h"

namespace Objects
{
	namespace Research
	{
		namespace D = DeltaWorks;

		/**
		@brief Research progress of a given faction

		Serialization is used only to communicate with clients.
		IO export/import must use string names instead
		*/
		class FactionState
		{
		public:
			typedef std::function<float(float)>	ExtraModulationFunction;





		private:
			int						totalContribution = 0,	lastSpeed = 0;
			Vector<PTopic>			researchQueue;
			bool					factionIsNexus;
			index_t					resourceTableEntry;
			static void				LogLua(const String&msg);

		public:

			bool					topicProgressChanged = false,
									queueChanged = false;



			TopicProgressVector		topicProgressVector;	//!< Progress of each research topic. Also represented in luaResearchState, but depends on what the Lua program actually does
			Attributes::Table		attributeTable;			//!< Attributes retrieved from Lua research state





			#ifndef CLIENT
				typedef const SHA256::HashContainer&	TScriptID;
				Lua::Reference		luaResearchState;		//!< Server-only Lua research state to derive attributes from
				static const TScriptID&	GetScriptIdentifier()	{return globalState.script->identifier;}
			#endif

			Faction*const			owner;


			/**/					FactionState(Faction*owner, bool isNexus);
			/**/					FactionState(const FactionState&)=delete;
			virtual					~FactionState();
			void					operator=(const FactionState&)=delete;

			/**
			Handles changes necessary to advance all endless research topics by one. Should only be used on Nexus faction
			*/
			void					AdvanceNexusResearch();
			void					Contribute(int c)	{totalContribution+=c;}

			bool					IsIdle() const {return researchQueue.IsEmpty();}
			ArrayRef<const PTopic>	GetResearchQueue() const	{return researchQueue.ToRef().Const();}
			PTopic					GetActiveTopic() const {return researchQueue.IsNotEmpty() ? researchQueue.First() : PTopic();}
			String					ExportResearchQueue();
			void					ExportResearchQueue(Ctr::Array<UINT32>&);
			void					RestoreResearchQueue(const String&paths);
			void					RestoreResearchQueue(const Ctr::Array<UINT32>&);
			bool					ResearchIsPlanned(const PTopic&)	const;
			bool					AppendResearchTask(const PTopic&);
			bool					AppendResearchTask(index_t serialIndex)	{return AppendResearchTask(GetTopic(serialIndex));}
			bool					DropResearchTask(const PTopic&);
			bool					DropResearchTask(index_t serialIndex) 	{return DropResearchTask(GetTopic(serialIndex));}
			bool					ResearchQueueIsFull() const;

			int						GetResearchSpeed() const	{return lastSpeed;}

			#ifndef CLIENT
				void				FinalizeRound(const PFaction&);
				/**
				Updates this->attributeTable from the current Lua research state.
				Automatically called by ImportLuaState(), and FinalizeRound()
				*/
				void				UpdateAttributeTable();
				void				ImportLuaState(const ArrayRef<const BYTE>&serializedState);
				void				ExportLuaState(Array<BYTE>&outSerializedState) const;

				void				LoadNexusResearchState();

			#endif

	



			
			//static PTopic			FindTopicOfPrototype(const PWorldEntity&proto);	//misleading. multiple topics often share the same prototype
			//static PTopic			FindTopicOfPrototype(const Component::Base*);	//misleading. multiple topics often share the same prototype

			#ifdef CLIENT
				static void			BeginServerTopicUpdate();
				static void			DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex);
			#endif
		};

	}


}
