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

	enum class ResearchSateModulation
	{
		DefaultFloat,
		InvertedFloat,
		FloorInt,
		CeilInt,
		InvertedCeilInt
	};

	enum class ResearchTask
	{
		None,
		Unlock,
		Improve
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



		class Research;
		typedef std::shared_ptr<Research>	PResearch;
		typedef std::weak_ptr<Research>	WResearch;


		typedef Lua::Function<void(Lua::Reference)>	FOnActivate;	//must be activated on a valid state

		class Research
		{
		public:
			const String		name,iconName;
			const WResearch		parent;
			Buffer0<PResearch>	children;
			FOnActivate			onActivate;


			/**/				Research(const PResearch&parent, const String&name, const String&iconName, const FOnActivate&func);
			virtual				~Research();
		};


	private:
		TUniqueID				factionID;
		bool					factionIsNexus;
		static PResearch		CreateResearch(const PResearch&parent, const String&name, const String&icon, const FOnActivate&func);
	public:



		static void				StaticCreate();
		Lua::Reference			researchState;	//unknown to C++ code



		struct GlobalState
		{
			Buffer0<PResearch>	rootResearch;
			Lua::PProgram		researchProgram;
			Lua::Function<Lua::Reference()>	stateConstructor;
			Lua::Function<void(Lua::Reference)>	selectResearch;
		};
		static GlobalState		globalState;



		/**/					ResearchState(const TUniqueID&id, bool isNexus);
		void					SignalIDChange(const TUniqueID&){}

		void					Contribute()	{};
		void					FinalizeRound(const PFaction&)	{}

		bool					CheckUnlocked(const Component::Base&base, String&errorOut) const	{return true;}
		bool					CheckUnlocked(ConstName, String&errorOut) const	{return true;}
		bool					IsUnlocked(const Component::Base&base) const	{return true;}
		bool					IsUnlocked(ConstName) const	{return true;}

	};

	


}
