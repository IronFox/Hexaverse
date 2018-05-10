#pragma once

#include <container/buffer.h>
#include <container/hashtable.h>
#include <global_string.h>
#include "../../lua/luaInterface.h"
#include "../types.h"
#include "attributes.h"
#include "../../lua/scriptCache.h"
#include "../../lua/luaGameAPI.h"

namespace Objects
{
	namespace Research
	{
		namespace D = DeltaWorks;

		class FactionState;

		/**
		Contains one value per research topic denoting the research progress
		*/
		typedef D::Array<int>	TopicProgressVector;


		/**
		Group of research topics or child research groups.
		Defined by Lua script or server package.
		With the exception of the root topic, this type is not directly used
		*/
		class TopicGroup
		{
		public:
			typedef D::Buffer0<PTopic>	List;
			typedef List::iterator			iterator;
			typedef List::const_iterator	const_iterator;


			List					children;
			D::StringTable<PTopic>	map;
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
			bool					ResearchCompleted(const TopicProgressVector&researchState) const;

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


			static PTopic			Create(const Research::PTopic&parent, const String&name, const String&iconName, 
												const Research::Topic::FSetProgress&setProgress, const Research::Topic::FGetProgress&getProgress, 
												int maxProgress,
												const Research::PTopic&dependency
											);

		};


		struct GlobalState
		{
			Attributes::Table	zeroTable;

			#ifndef CLIENT
				/**
				Minimal faction research state that owner-less prototypes may use
				*/
				Lua::Reference			zeroState;
				ScriptCache::PScript	script,	//currently loaded program
										factoryDefault;	//program defined in install directory. Must load or server execution fails
			#endif
			Buffer0<PTopic>		allTopics;
			TopicGroup			rootResearch;
			#ifndef CLIENT
				Lua::API			researchAPI;
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
	}
}
