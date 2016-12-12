#pragma once

#include <functional>
#include <interface/serializable.h>
#include <global_string.h>
#include <container/hashtable.h>

#include "types.h"
#include "../uniqueID.h"
#include "../stringTable.h"
#include "../luaInterface.h"
#include "../scriptCache.h"

namespace Objects
{

	class ResearchGroup
	{
	public:
		typedef Buffer0<PResearchTopic>	List;
		typedef List::iterator			iterator;
		typedef List::const_iterator	const_iterator;


		List					children;
		StringTable<PResearchTopic>	map;
		const String			name;

		/**/					ResearchGroup(const String&name):name(name)	{}
		bool					Insert(const PResearchTopic&t);
		PResearchTopic			Find(const StringRef&name) const;
		PResearchTopic			Find(const String&name) const {return Find(name.ref());}
		void					Clear()	{children.Clear(); map.Clear();}
		count_t					Count() const {return children.Count();}

		iterator				begin() {return children.begin();}
		iterator				end() {return children.end();}
		const_iterator			begin() const {return children.begin();}
		const_iterator			end() const {return children.end();}
	};

	class ResearchTopic : public ResearchGroup
	{
		typedef ResearchGroup	Super;
	public:
		typedef Lua::Function<void(Lua::Reference, int)>	FSetProgress;
		typedef Lua::Function<int(Lua::Reference)>	FGetProgress;
		typedef Lua::Function<bool(Lua::Reference)> FBool;
		typedef Lua::Function<PResearchTopic(Lua::Reference)> FResearchTopic;
		

		const String			iconName;
		const WResearchTopic	parent;
		FSetProgress			setProgress;
		FGetProgress			getProgress;
		const int				maxProgress;
		const WResearchTopic	dependency;
		PWorldEntity			prototypeReference;
		index_t					serialIndex=InvalidIndex;
		Component::Base			*componentReference=nullptr;


		/**/					ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
												const FSetProgress&setProgress, 
												const FGetProgress&getProgress, int maxProgress,
												const PResearchTopic&dependency
												);
		virtual					~ResearchTopic();

		bool					IsChildOf(const PResearchTopic&other) const {if (!this) return false; if (other.get() == this) return true; PResearchTopic p = parent.lock(); return p && p->IsChildOf(other);}
		bool					IsGroup() const {return maxProgress == 0;}
		bool					ResearchCompleted(const Lua::Reference&researchState) const
		{
			return maxProgress > 0 && getProgress(researchState) >= maxProgress;
		}

		String					GetFullName() const
		{
			PResearchTopic p = parent.lock();
			if (p)
				return p->name+":"+name;
			return name;
		}
		String					GetPath() const
		{
			PResearchTopic p = parent.lock();
			if (p)
				return p->GetPath() + '/'+name;
			return name;
		}

		/**
		Checks whether or not the local topic is researchable in its current state.
		A topic is researchable if it's generally researchable (i.e. not a group), its dependencies are completed,
		and it can and has not been completed.
		*/
		bool					CanResearch(const Lua::Reference&researchState) const
		{
			if (IsGroup())
				return false;
			if (ResearchCompleted(researchState))
				return false;
			PResearchTopic p = dependency.lock();
			return !p || p->ResearchCompleted(researchState);
		}
	};

	/**
	@brief Research progress of a given faction

	Serialization is used only to communicate with clients.
	IO export/import must use string names instead
	*/
	class ResearchState
	{
	public:
		typedef std::function<float(float)>	ExtraModulationFunction;


		typedef const SHA256::HashContainer&	TScriptID;



	private:
		int						totalContribution = 0;
		PResearchTopic			activeResearch;
		bool					factionIsNexus;
		static void				LogLua(const String&msg);
		static PResearchTopic	CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FSetProgress&setProgress, 
												const ResearchTopic::FGetProgress&getProgress, int maxProgress,
												const PResearchTopic&dependency
												);
		static void				InnerLoad(const ScriptCache::PScript&script);
		static void				RequireLoad(const ScriptCache::PScript&script);
	public:

		bool					changed = false;

		static void				StaticCreate();
		static void				ActivateProgram(const ScriptCache::PScript&script);
		static void				LoadDefaultProgram();




		Lua::Reference			researchState;	//unknown to C++ code
		Array<BYTE>				serialData;


		struct GlobalState
		{
			ScriptCache::PScript	script,fallback;
			Buffer0<PResearchTopic>	allTopics;
			ResearchGroup		rootResearch;
			Lua::PProgram		researchProgram;
			Lua::Function<Lua::Reference()>	stateConstructor;
			Lua::Function<Lua::Reference(Lua::Reference)>	exportState;
			Lua::Function<void(Lua::Reference,Lua::Reference)> importState;

			/**/				GlobalState():rootResearch(""){}
		};
		static GlobalState		globalState;

		static const TScriptID&	GetScriptIdentifier()	{return globalState.script->identifier;}

		Faction*const			owner;


		/**/					ResearchState(Faction*owner, bool isNexus);
		/**/					ResearchState(const ResearchState&)=delete;
		void					operator=(const ResearchState&)=delete;

		void					Contribute()	{totalContribution++;}
		void					FinalizeRound(const PFaction&);

		bool					CheckUnlocked(const Component::Base&base, String&errorOut) const	{return true;}
		bool					CheckUnlocked(ConstName, String&errorOut) const	{return true;}
		bool					IsIdle() const {PResearchTopic t = GetActiveTopic(); return !t || t->ResearchCompleted(researchState);}
		PResearchTopic			GetActiveTopic() const	{return activeResearch;}
		index_t					GetActiveTopicIndex() const {return activeResearch ? activeResearch->serialIndex : InvalidIndex;}
		bool					SetActiveTopic(const PResearchTopic&active);
		void					SetActiveTopic(index_t serialIndex)	{SetActiveTopic(GetTopic(serialIndex));}
		void					Import(const ArrayRef<const BYTE>&serializedState);
		void					Export(Array<BYTE>&outSerializedState) const;
		static PResearchTopic	GetTopic(index_t);
		static PResearchTopic	FindTopic(const String&path);
	};

	


}
