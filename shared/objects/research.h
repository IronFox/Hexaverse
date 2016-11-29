#pragma once

#include <functional>
#include <interface/serializable.h>
#include <global_string.h>

#include "types.h"
#include "../uniqueID.h"
#include "../stringTable.h"
#include "../luaInterface.h"

namespace Objects
{

	class ResearchTopic
	{
	public:
		typedef Lua::Function<void(Lua::Reference, int)>	FAdvance;
		typedef Lua::Function<int(Lua::Reference)>	FGetProgress;
		typedef Lua::Function<bool(Lua::Reference)> FBool;
		typedef Lua::Function<PResearchTopic(Lua::Reference)> FResearchTopic;
		

		const String			name,iconName;
		const WResearchTopic	parent;
		Buffer0<PResearchTopic>	children;
		FAdvance				advance;
		FGetProgress			currentProgress;
		const int				maxProgress;
		const WResearchTopic	dependency;
		PWorldEntity			prototypeReference;
		index_t					serialIndex=InvalidIndex;
		Component::Base			*componentReference=nullptr;


		/**/					ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
												const FAdvance&adv, 
												const FGetProgress&currentProgress, int maxProgress,
												const PResearchTopic&dependency
												);
		virtual					~ResearchTopic();

		bool					IsChildOf(const PResearchTopic&other) const {if (!this) return false; if (other.get() == this) return true; PResearchTopic p = parent.lock(); return p && p->IsChildOf(other);}
		bool					IsGroup() const {return maxProgress == 0;}
		bool					ResearchCompleted(const Lua::Reference&researchState) const
		{
			return currentProgress(researchState) >= maxProgress;
		}
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






	private:
		int						totalContribution = 0;
		PResearchTopic			activeResearch;
		TUniqueID				factionID;
		bool					factionIsNexus;
		static void				LogLua(const String&msg);
		static PResearchTopic	CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FAdvance&adv, 
												const ResearchTopic::FGetProgress&currentProgress, int maxProgress,
												const PResearchTopic&dependency
												);
	public:

		bool					changed = false;

		static void				StaticCreate();
		Lua::Reference			researchState;	//unknown to C++ code



		struct GlobalState
		{
			Buffer0<PResearchTopic>	rootResearch,allTopics;
			Lua::PProgram		researchProgram;
			Lua::Function<Lua::Reference()>	stateConstructor;
			Lua::Function<Lua::Reference(Lua::Reference)>	exportState;
			Lua::Function<void(Lua::Reference,Lua::Reference)> importState;
		};
		static GlobalState		globalState;



		/**/					ResearchState(const TUniqueID&id, bool isNexus);
		void					SignalIDChange(const TUniqueID&){}

		void					Contribute()	{totalContribution++;}
		void					FinalizeRound(const PFaction&);

		bool					CheckUnlocked(const Component::Base&base, String&errorOut) const	{return true;}
		bool					CheckUnlocked(ConstName, String&errorOut) const	{return true;}
		bool					IsUnlocked(const Component::Base&base) const	{return true;}
		bool					IsUnlocked(ConstName) const	{return true;}
		bool					IsIdle() const {return !GetActiveTopic();}
		PResearchTopic			GetActiveTopic() const	{return activeResearch;}
		bool					SetActiveTopic(const PResearchTopic&active);
		void					SetActiveTopic(index_t serialIndex)	{SetActiveTopic(GetTopic(serialIndex));}
		void					Import(const ArrayData<BYTE>&serializedState);
		void					Export(Array<BYTE>&outSerializedState) const;
		static PResearchTopic	GetTopic(index_t);
		static PResearchTopic	FindTopic(const String&name);
	};

	


}
