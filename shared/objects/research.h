#pragma once

#include "types.h"
#include <functional>
#include <interface/serializable.h>
#include <global_string.h>
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
	class ResearchState : public SerializableObject
	{
	public:
		typedef std::function<float(float)>	ExtraModulationFunction;
	private:
		static float			GetFactor(int level);
		static String			_Describe(float fc, float baseValue, ResearchSateModulation type/*=ModulationType::DefaultFloat*/, ExtraModulationFunction mod/*=ExtraModulationFunction()*/);
	public:



		static void				StaticCreate();
		Lua::Reference			researchState;	//unknown to C++ code



		struct GlobalState
		{
			Lua::PProgram	researchProgram;
			Lua::Function<Lua::Reference()>	stateConstructor;
		};
		static GlobalState		globalState;

		#ifndef CLIENT
			UINT32				contribution=0;
		#endif

		typedef ResearchTask Task;

		ConstName				currentResearch = ConstName::Empty;
		Component::Attribute	currentResearchAttribute;	//ignored if targeted research is not unlocked

		Array<UINT32>			unlockProgress,improveProgress;
		bool					changed = false;
		TUniqueID				ownerID;

		/**/					ResearchState(const TUniqueID&id, bool isNexus);
		/**/					ResearchState();

		void					operator=(const ResearchState&) = delete;

		#ifdef CLIENT
			void				Update(const ResearchState&, const PFaction&parent);
		#endif

		virtual	serial_size_t	GetSerialSize(bool export_size) const override;
		virtual	bool			Serialize(IWriteStream&out_stream, bool export_size) const override;
		virtual	bool			Deserialize(IReadStream&in_stream, serial_size_t fixed_size) override;
		virtual	bool			HasFixedSize() const override {return true;}


		static void		RegisterResearch(Structure*, Component::Attribute);
		static void		RegisterResearch(Component::Base&component, Component::Attribute);
		#ifndef NO_SATELLITES
			static void	RegisterResearch(Satellite*, Component::Attribute);
		#endif
		static void		RegisterUnlockCost(Structure*, int researchCost);
		static void		RegisterUnlockCost(Component::Base&component, int researchCost);
		static void		RegisterDependency(Component::Base&component, Component::Base&dependentOn);
		static void		RegisterDependency(Component::Base&component, const PStructure&dependentOn);

		bool			CheckUnlocked(const Component::Base&,String&errorOut)	const;
		bool			CheckUnlocked(ConstName,String&errorOut)	const;
		bool			IsUnlocked(ConstName) const;
		bool			IsActive() const {return currentResearch != ConstName::Empty;}

		float			GetFactorFor(ConstName, Component::Attribute)	const;
		float			GetFactorFor(const Component::Base&, Component::Attribute)	const;

		UINT32			GetResearchOf(ConstName, Component::Attribute)	const;
		void			GetResearchOf(ConstName, UINT32&current, UINT32&max)	const;

		Task			GetCurrentTask() const;

		static void		StaticCreate();
		static bool		IsGenerallyResearchable(ConstName);
		static bool		IsGenerallyResearchable(ConstName, Component::Attribute);
		bool			IsCurrentlyResearchable(ConstName name, Component::Attribute attrib) const;
		bool			IsCurrentlyResearchable(ConstName name, String*dependentOnOut=nullptr) const;

		#ifndef CIENT
			void		Contribute();
			void		FinalizeRound(const PFaction&parent);
		#endif

		typedef ResearchSateModulation ModulationType;



		String			Describe(ConstName, float baseValue, Component::Attribute attrib, ModulationType type=ModulationType::DefaultFloat, ExtraModulationFunction mod=ExtraModulationFunction())	const;
		String			Describe(const Component::Base&, float baseValue, Component::Attribute attrib, ModulationType type=ModulationType::DefaultFloat, ExtraModulationFunction mod=ExtraModulationFunction())	const;
	};

	


}
