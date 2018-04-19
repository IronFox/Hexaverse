#pragma once

#include <functional>
#include <interface/serializable.h>
#include <global_string.h>
#include <container/hashtable.h>

#include "types.h"
#include "../uniqueID.h"
#include "../stringTable.h"
#ifndef CLIENT
	#include "../lua/luaInterface.h"
	#include "../lua/scriptCache.h"
#endif
//#include "luaAttribute.h"
#include <structure/observable.h>

namespace Objects
{
	namespace Research
	{
		/**
		Contains one value per research topic denoting the research progress
		*/
		typedef Array<int>	TopicProgressVector;

		namespace Attributes
		{

			struct Flags
			{
				enum flag_t
				{
					HasFunctionFlag = 0x1,
					IsConst = 0x2,


					MaxValue = 0x3,
				};

				typedef UINT8			serial_type_t;
			};

	
			template <typename T>
				class TypeMap
				{
				public:
					typedef Flags::serial_type_t	flag_t;
					struct Attrib
					{
						T				value;
						String			description;
						flag_t			flags;


						bool			IsSet() const
						{
							return flags != 0;
						}

						bool			IsFunctionOrConstNonZero() const
						{
							return IsSet() && (((flags & Flags::HasFunctionFlag)!=0) || value != 0);
						}
					};
					Array<Attrib>		attributes;
					Buffer0<index_t>	changedSinceLastRound;

					void				ClearChanges()
					{
						changedSinceLastRound.Clear();
					}

					Attrib&				ForceUpdate(index_t at)
					{
						changedSinceLastRound << at;
						return attributes[at];
					}

					bool				IsSet( index_t at ) const
					{
						if (attributes.IsEmpty())
							return false;
						return Expect::SafeGet(attributes,at,CLOCATION).IsSet();
					}

					bool				IsFunctionOrConstNonZero( index_t at ) const
					{
						if (attributes.IsEmpty())
							return false;
						return Expect::SafeGet(attributes,at,CLOCATION).IsFunctionOrConstNonZero();
					}

					bool				CheckUpdate(index_t at, T newValue, const String&newDesc, flag_t flags)
					{
						Attrib&a = attributes[at];
						if (a.description == newDesc && a.value == newValue && a.flags == flags)
							return false;
						a.value = newValue;
						a.description = newDesc;
						a.flags = flags;
						changedSinceLastRound << at;
						return true;
					}

					T				SafeGet(index_t at)	const
					{
						if (at == InvalidIndex)
							return (T)0;
						if (at >= attributes.Count())
						{
							ASSERT__(attributes.IsEmpty());
							return (T)0;
						}
						return attributes[at].value;
					}

					const String&	SafeDesc(index_t at) const
					{
						if (at == InvalidIndex)
						{
							static String rs = "???";
							return rs;
						}
						if (at >= attributes.Count())
						{
							ASSERT__(attributes.IsEmpty());
							static String rs = "[null]";
							return rs;
						}
						return attributes[at].description;
					}

				};


			/**
			Table containing all attributes in one array per type: float, int, bool
			*/
			class Table
			{
			public:
				TypeMap<float>	floats;
				TypeMap<int>		ints;
				TypeMap<bool>		bools;


				void				ClearChanges()
				{
					floats.ClearChanges();
					ints.ClearChanges();
					bools.ClearChanges();
				}

				bool				AnyChanged() const
				{
					return
						floats.changedSinceLastRound.Count() > 0
						||
						ints.changedSinceLastRound.Count() > 0
						||
						bools.changedSinceLastRound.Count() > 0
						;
				}


				template <typename T>
					T				Get(index_t) const = delete;
				template <>
					int				Get<int>(index_t at) const {return ints.SafeGet(at);}
				template <>
					bool			Get<bool>(index_t at) const {return bools.SafeGet(at);}
				template <>
					float			Get<float>(index_t at) const {return floats.SafeGet(at);}
				template <typename T>
					const String&	Describe(index_t) const = delete;
				template <>
					const String&	Describe<int>(index_t at) const {return ints.SafeDesc(at);}
				template <>
					const String&	Describe<bool>(index_t at) const {return bools.SafeDesc(at);}
				template <>
					const String&	Describe<float>(index_t at) const {return floats.SafeDesc(at);}

			private:

			};
		}


		class FactionState;

		/**
		Group of research topics or child research groups.
		Defined by Lua script or server package.
		With the exception of the root topic, this type is not directly used
		*/
		class TopicGroup
		{
		public:
			typedef Buffer0<PTopic>	List;
			typedef List::iterator			iterator;
			typedef List::const_iterator	const_iterator;


			List					children;
			StringTable<PTopic>	map;
			const String			name;

			/**/					TopicGroup(const String&name):name(name)	{}
			bool					Insert(const PTopic&t);
			PTopic					Find(const StringRef&name) const;
			PTopic					Find(const String&name) const {return Find(name.ref());}
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
		class Topic : public TopicGroup
		{
			typedef TopicGroup	Super;
		public:
			#ifndef CLIENT
				typedef Lua::Function<void(Lua::Reference, int)>	FSetProgress;
				typedef Lua::Function<int(Lua::Reference)>	FGetProgress;
				typedef Lua::Function<bool(Lua::Reference)> FBool;
				typedef Lua::Function<PTopic(Lua::Reference)> FResearchTopic;
			#else
				typedef int	FSetProgress;
				typedef int	FGetProgress;
				typedef int FBool;
				typedef int FResearchTopic;
			#endif
		
			/**
			Name of the icon to be used
			*/
			const String			iconName;

			/**
			Reference to the parent group.
			Always set.
			*/
			const WTopic			parent;

			#ifndef CLIENT
				/**
				Lua function to update the progress of the local research. Empty if this is a group
				*/
				FSetProgress		setProgress;
				FGetProgress		getProgress;
			#endif
		

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
			const WTopic			dependency;

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


			/**/					Topic(const PTopic&parent, const String&name, const String&iconName, 
													const FSetProgress&setProgress, const FGetProgress&getProgress,
													int maxProgress,
													const PTopic&dependency
													);
			virtual					~Topic();

			bool					IsChildOf(const PTopic&other) const {if (!this) return false; if (other.get() == this) return true; PTopic p = parent.lock(); return p && p->IsChildOf(other);}
			bool					IsGroup() const {return maxProgress == 0;}

			int						GetProgress(const FactionState&) const;

			/**
			Checks whether or not the local research has been completed.
			@return true if the local research is a topic, finite, and the current research state has reached the maximum research progress
			*/
			bool					ResearchCompleted(const TopicProgressVector&researchState) const
			{
				return maxProgress > 0 && researchState[serialIndex] >= maxProgress;
			}

			/**
			Determines a topic/group name from the local topic name, and its parent name in the form [parent]:[local]
			*/
			String					GetFullName() const
			{
				PTopic p = parent.lock();
				if (p)
					return p->name+":"+name;
				return name;
			}

			/**
			Fetches the full /-separated path of the local topic to the root
			*/
			String					GetPath() const
			{
				PTopic p = parent.lock();
				if (p)
					return p->GetPath() + '/'+name;
				return name;
			}

			/**
			Checks whether or not the local topic is researchable in its current state.
			A topic is researchable if it's generally researchable (i.e. not a group), its dependencies are completed,
			and it can and has not been completed.
			*/
			bool					CanResearch(const TopicProgressVector&researchState, const ArrayRef<const PTopic>&workQueue) const
			{
				if (IsGroup())
					return false;
				if (ResearchCompleted(researchState))
					return false;
				PTopic p = dependency.lock();
				return !p || p->ResearchCompleted(researchState) || workQueue.Contains(p);
			}

			bool					CanResearch(const PFaction&) const;

		};


		struct GlobalState
		{
			Attributes::Table	zeroTable;

			#ifndef CLIENT
				Lua::Reference		zeroState;
				ScriptCache::PScript	script,	//currently loaded program
									program0,	//absolute fallback program to be used if all fails
									factoryDefault;	//program defined in install directory
			#endif
			Buffer0<PTopic>		allTopics;
			TopicGroup			rootResearch;
			#ifndef CLIENT
				Lua::PProgram		researchProgram;
				Lua::Function<Lua::Reference()>	stateConstructor;
				Lua::Function<Lua::Reference(Lua::Reference)>	exportState;
				Lua::Function<void(Lua::Reference,Lua::Reference)> importState;
			#endif
			Observable<>		onScriptChange;
			Observable<Faction*>onQueueChange;

			/**/				GlobalState():rootResearch(""){}
		};
		extern GlobalState		globalState;

		PTopic					GetTopic(index_t);
		PTopic					FindTopic(const StringRef&path);
		inline PTopic			FindTopic(const String&path)	{return FindTopic(path.ref());}

		static const count_t	MaxResearchQueueLength = 4;

		void					StaticCreate();
		#ifndef CLIENT
			void				ActivateProgram(const ScriptCache::PScript&script);
			void				LoadDefaultProgram();
		#endif

		/**
		Checks whether either a const value or lua function is defined the specified int attribute index
		*/
		bool					LuaIntAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified bool attribute index
		*/
		bool					LuaBoolAttributeIsSet(index_t);
		/**
		Checks whether either a const value or lua function is defined the specified float attribute index
		*/
		bool					LuaFloatAttributeIsSet(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified int attribute index
		*/
		bool					LuaIntAttributeIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified bool attribute index
		*/
		bool					LuaBoolIsFunctionOrConstNonZero(index_t);
		/**
		Checks whether a non-zero const value or lua function is defined the specified float attribute index
		*/
		bool					LuaFloatAttributeIsFunctionOrConstNonZero(index_t);

		template <typename T>
			inline bool			LuaAttributeIsSet(index_t) = delete;
		template <>
			inline bool			LuaAttributeIsSet<int>( index_t idx ) {return LuaIntAttributeIsSet(idx);}
		template <>
			inline bool			LuaAttributeIsSet<bool>( index_t idx ) {return LuaBoolAttributeIsSet(idx);}
		template <>
			inline bool			LuaAttributeIsSet<float>( index_t idx ) {return LuaFloatAttributeIsSet(idx);}

		template <typename T>
			inline bool			LuaAttributeIsFunctionOrConstNonZero(index_t) = delete;
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<int>( index_t idx ) {return LuaIntAttributeIsFunctionOrConstNonZero(idx);}
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<bool>( index_t idx ) {return LuaBoolIsFunctionOrConstNonZero(idx);}
		template <>
			inline bool			LuaAttributeIsFunctionOrConstNonZero<float>( index_t idx ) {return LuaFloatAttributeIsFunctionOrConstNonZero(idx);}
		


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
			Ctr::Buffer0<PTopic>	researchQueue;
			bool					factionIsNexus;
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
			void					operator=(const FactionState&)=delete;

			/**
			Handles changes necessary to advance all endless research topics by one. Should only be used on Nexus faction
			*/
			void					AdvanceNexusResearch();
			void					Contribute(int c)	{totalContribution+=c;}

			bool					CheckUnlocked(const Component::Base&base, String&errorOut) const	{return true;}
			bool					CheckUnlocked(ConstName, String&errorOut) const	{return true;}
			bool					IsIdle() const {return researchQueue.IsEmpty();}
			ArrayRef<const PTopic>	GetResearchQueue() const	{return researchQueue.ToRef();}
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
