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
//#include "luaAttribute.h"
#include <structure/observable.h>

namespace Objects
{
	/**
	Contains one value per research topic denoting the research progress
	*/
	typedef Array<int>	ResearchProgressVector;

	

	/**
	Table containing all attributes in one array per type: float, int, bool
	*/
	class ResearchAttributeTable
	{
	public:
		template <typename T>
			struct Attrib
			{
				T			value;
				String		description;
			};

		Array<Attrib<float> >	floatAttributes;
		Array<Attrib<int> >		intAttributes;
		Array<Attrib<bool> >	boolAttributes;


		template <typename T>
			T				Get(index_t) const = delete;
		template <>
			int				Get<int>(index_t at) const {return intAttributes[at].value;}
		template <>
			bool			Get<bool>(index_t at) const {return boolAttributes[at].value;}
		template <>
			float			Get<float>(index_t at) const {return floatAttributes[at].value;}
		template <typename T>
			const String&	Describe(index_t) const = delete;
		template <>
			const String&	Describe<int>(index_t at) const {return intAttributes[at].description;}
		template <>
			const String&	Describe<bool>(index_t at) const {return boolAttributes[at].description;}
		template <>
			const String&	Describe<float>(index_t at) const {return floatAttributes[at].description;}
	};


	class ResearchState;

	/**
	Group of research topics or child research groups.
	Defined by Lua script or server package.
	With the exception of the root topic, this type is not directly used
	*/
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

	/**
	Individual research topic, or group.
	*/
	class ResearchTopic : public ResearchGroup
	{
		typedef ResearchGroup	Super;
	public:
		typedef Lua::Function<void(Lua::Reference, int)>	FSetProgress;
		typedef Lua::Function<bool(Lua::Reference)> FBool;
		typedef Lua::Function<PResearchTopic(Lua::Reference)> FResearchTopic;
		
		/**
		Name of the icon to be used
		*/
		const String			iconName;

		/**
		Reference to the parent group.
		Always set.
		*/
		const WResearchTopic	parent;

		/**
		Lua function to update the progress of the local research. Empty if this is a group
		*/
		FSetProgress		setProgress;
		

		/**
		Maximum research progress and topic classification.
		If <0, then research is infinite.
		If =0, then this topic is actually a group and cannot be researched.
		If >0, then research is finite and completed once the current progress equals (or exceeds) this value
		*/
		const int				maxProgress;
		/**
		Other research topic that must be completed before the local research can be started.
		Empty if no dependency exists.
		*/
		const WResearchTopic	dependency;

		/**
		Reference to the prototyped entity type being affected by the local research. Resolved by checking the topic icon name.
		Used for visualization. No logical function at this point.
		*/
		PWorldEntity			prototypeReference;

		/**
		Serialized index of the local topic in the linear research topic list, starting at 0.
		Unique among all active topics.
		*/
		index_t					serialIndex=InvalidIndex;

		/**
		Pointer to the component being affected by the local research. Resolved by checking the topic icon name.
		Used for visualization. No logical function at this point.
		*/
		Component::Base			*componentReference=nullptr;


		/**/					ResearchTopic(const PResearchTopic&parent, const String&name, const String&iconName, 
												const FSetProgress&setProgress, 
												int maxProgress,
												const PResearchTopic&dependency
												);
		virtual					~ResearchTopic();

		bool					IsChildOf(const PResearchTopic&other) const {if (!this) return false; if (other.get() == this) return true; PResearchTopic p = parent.lock(); return p && p->IsChildOf(other);}
		bool					IsGroup() const {return maxProgress == 0;}

		int						GetProgress(const ResearchState&) const;

		/**
		Checks whether or not the local research has been completed.
		@return true if the local research is a topic, finite, and the current research state has reached the maximum research progress
		*/
		bool					ResearchCompleted(const ResearchProgressVector&researchState) const
		{
			return maxProgress > 0 && researchState[serialIndex] >= maxProgress;
		}

		/**
		Determines a topic/group name from the local topic name, and its parent name in the form [parent]:[local]
		*/
		String					GetFullName() const
		{
			PResearchTopic p = parent.lock();
			if (p)
				return p->name+":"+name;
			return name;
		}

		/**
		Fetches the full /-separated path of the local topic to the root
		*/
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
		bool					CanResearch(const ResearchProgressVector&researchState, const ArrayRef<const PResearchTopic>&workQueue) const
		{
			if (IsGroup())
				return false;
			if (ResearchCompleted(researchState))
				return false;
			PResearchTopic p = dependency.lock();
			return !p || p->ResearchCompleted(researchState) || workQueue.Contains(p);
		}

		bool					CanResearch(const PFaction&) const;

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
		int						totalContribution = 0,	lastSpeed = 0;
		Ctr::Buffer0<PResearchTopic>	researchQueue;
		bool					factionIsNexus;
		static void				LogLua(const String&msg);
		static PResearchTopic	CreateResearch(const PResearchTopic&parent, const String&name, const String&iconName, 
												const ResearchTopic::FSetProgress&setProgress, 
												int maxProgress,
												const PResearchTopic&dependency
												);
		static void				InnerLoad(const ScriptCache::PScript&script);
		static void				RequireLoad(const ScriptCache::PScript&script);
		void					LoadNexusResearchState();
	public:

		bool					changed = false, queueChanged = false;

		static void				StaticCreate();
		static void				ActivateProgram(const ScriptCache::PScript&script);
		static void				LoadDefaultProgram();

		static const count_t	MaxResearchQueueLength = 4;

		ResearchProgressVector	topicProgressVector;	//!< Progress of each research topic. Also represented in luaResearchState, but depends on what the Lua program actually does
		Lua::Reference			luaResearchState;		//!< Server-only Lua research state to derive attributes from
		ResearchAttributeTable	attributeTable;			//!< Attributes retrieved from Lua research state

		struct TSerialState
		{
			Array<BYTE>		serialState;
			String			researchQueue;

			void			swap(TSerialState&other)
			{
				serialState.swap(other.serialState);
				researchQueue.swap(other.researchQueue);
			}

			friend void		swap(TSerialState&a,TSerialState&b)
			{
				a.swap(b);
			}
		};


		struct GlobalState
		{
			bool					zeroTableChanged = false;
			ResearchAttributeTable	zeroTable;
			Lua::Reference			zeroState;

			ScriptCache::PScript	script,	//currently loaded program
									program0,	//absolute fallback program to be used if all fails
									factoryDefault;	//program defined in install directory
			Buffer0<PResearchTopic>	allTopics;
			ResearchGroup			rootResearch;
			Lua::PProgram			researchProgram;
			Lua::Function<Lua::Reference()>	stateConstructor;
			Lua::Function<Lua::Reference(Lua::Reference)>	exportState;
			Lua::Function<void(Lua::Reference,Lua::Reference)> importState;
			Observable<>			onScriptChange;
			Observable<Faction*>	onQueueChange;

			/**/				GlobalState():rootResearch(""){}
		};
		static GlobalState		globalState;

		static const TScriptID&	GetScriptIdentifier()	{return globalState.script->identifier;}

		Faction*const			owner;


		/**/					ResearchState(Faction*owner, bool isNexus);
		/**/					ResearchState(const ResearchState&)=delete;
		void					operator=(const ResearchState&)=delete;

		/**
		Handles changes necessary to advance all endless research topics by one. Should only be used on Nexus faction
		*/
		void					AdvanceNexusResearch();
		void					Contribute(int c)	{totalContribution+=c;}
		void					FinalizeRound(const PFaction&);

		bool					CheckUnlocked(const Component::Base&base, String&errorOut) const	{return true;}
		bool					CheckUnlocked(ConstName, String&errorOut) const	{return true;}
		bool					IsIdle() const {return researchQueue.IsEmpty();}
		ArrayRef<const PResearchTopic>GetResearchQueue() const	{return researchQueue.ToRef();}
		PResearchTopic			GetActiveTopic() const {return researchQueue.IsNotEmpty() ? researchQueue.First() : PResearchTopic();}
		String					ExportResearchQueue();
		void					ExportResearchQueue(Ctr::Array<UINT32>&);
		void					RestoreResearchQueue(const String&paths);
		void					RestoreResearchQueue(const Ctr::Array<UINT32>&);
		bool					ResearchIsPlanned(const PResearchTopic&)	const;
		bool					AppendResearchTask(const PResearchTopic&);
		bool					AppendResearchTask(index_t serialIndex)	{return AppendResearchTask(GetTopic(serialIndex));}
		bool					DropResearchTask(const PResearchTopic&);
		bool					DropResearchTask(index_t serialIndex) 	{return DropResearchTask(GetTopic(serialIndex));}
		bool					ResearchQueueIsFull() const;

		int						GetResearchSpeed() const	{return lastSpeed;}

		/**
		Updates this->attributeTable from the current Lua research state.
		Automatically called by ImportLuaState(), and FinalizeRound()
		*/
		void					UpdateAttributeTable();
		static void				UpdateAttributeTable(const Lua::Reference&luaResearchState, ResearchAttributeTable&table);
		void					ImportLuaState(const ArrayRef<const BYTE>&serializedState);
		void					ExportLuaState(Array<BYTE>&outSerializedState) const;

		/**
		Checks whether either a const value or lua function is defined the specified int attribute index
		*/
		static bool				LuaIntAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified bool attribute index
		*/
		static bool				LuaBoolAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified float attribute index
		*/
		static bool				LuaFloatAttributeIsSet(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified int attribute index
		*/
		static bool				LuaIntAttributeIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified bool attribute index
		*/
		static bool				LuaBoolIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified float attribute index
		*/
		static bool				LuaFloatAttributeIsFunctionOrConstNonZero(index_t);

		template <typename T>
			static bool			LuaAttributeIsSet(index_t) = delete;
		template <>
			static bool			LuaAttributeIsSet<int>( index_t idx ) {return LuaIntAttributeIsSet(idx);}
		template <>
			static bool			LuaAttributeIsSet<bool>( index_t idx ) {return LuaBoolAttributeIsSet(idx);}
		template <>
			static bool			LuaAttributeIsSet<float>( index_t idx ) {return LuaFloatAttributeIsSet(idx);}

		template <typename T>
			static bool			LuaAttributeIsFunctionOrConstNonZero(index_t) = delete;
		template <>
			static bool			LuaAttributeIsFunctionOrConstNonZero<int>( index_t idx ) {return LuaIntAttributeIsFunctionOrConstNonZero(idx);}
		template <>
			static bool			LuaAttributeIsFunctionOrConstNonZero<bool>( index_t idx ) {return LuaBoolIsFunctionOrConstNonZero(idx);}
		template <>
			static bool			LuaAttributeIsFunctionOrConstNonZero<float>( index_t idx ) {return LuaFloatAttributeIsFunctionOrConstNonZero(idx);}



		static PResearchTopic	GetTopic(index_t);
		static PResearchTopic	FindTopic(const String&path)	{return FindTopic(path.ref());}
		static PResearchTopic	FindTopic(const StringRef&path);
		static PResearchTopic	FindTopicOfPrototype(const PWorldEntity&proto);


		static void				BeginServerTopicUpdate();
		static void				DeclareServerTopic(index_t parentTopicIndex, const String&topicName, const String&iconName, int maxProgress, index_t dependentTopicIndex);
	};

	


}
