#ifndef componentReflectionH
#define componentReflectionH

#include <string/cli.h>
#include <string/cli_defaults.h>


namespace Reflection
{

	extern CLI::Interpretor	state;

	namespace Variable
	{
		CONSTRUCT_ENUMERATION5(Persistence,
			Temporary,
			ConfigOnly,
			ConfigOverridesSavegame,
			RestoreFromSavegame,
			GameInvariant		//written to savegames, but unchangeable until a new game is created. Common type: planet type
		)


		class Base : public CLI::Variable
		{
			typedef CLI::Variable	Super;
		public:
			std::function<void(Base&)>	extraOnChange;
			const Persistence	persistence;
			String				lastValue;
			const CLI::WFolder	parent;

			
			/**/				Base(const String&name, const String&type, Persistence persistence, count_t numElements,UINT32 protection):
									Super(name,type,numElements,protection | (persistence == Persistence::GameInvariant ? CLI::ReadOnly : 0)),persistence(persistence),parent(state.GetFocus())	{}


			void				OnChanged();
			void				SetupBase();

			void				CheckChanges()
			{
				if (lastValue != ToString())
				{
					OnChanged();
				}
			}

			bool				WriteToSavegame() const
			{
				return persistence != Persistence::Temporary && persistence != Persistence::ConfigOnly;
			}
		};

		typedef std::shared_ptr<Base>	PBase;

		typedef std::function<void(const PBase&)>	FCallback;

	}




	/**
	Initializes all variables to their respective addresses.
	Must be called after all object prototypes have been created, but before any world is generated/populated
	*/
	void	Create();

	/**
	Overwrites config/declaration.xml with a version based on the current configuration. Any information previously imported from it will be exported again.
	This function is called automatically if problems are detected with the existing declaration.xml file.
	*/
	void	CreatePrototypeDeclaration();

	/**
	Loads configuration files to fill the appropriate variables.
	Also restores any missing prototype planet declarations in the user configuration folder (my documents...), before attempting to load them.
	If a planet type is targeted that is not a prototype planet, then a new planet type file will be generated and filled with default values.
	*/
	void	Load(bool inTestMode);

	/**
	Reloads configuration files and updates all variables that are tagged as ConfigOverridesSavegame
	*/
	void	LoadOverrides();


	/**
	Visits all variables in the CLI state tree
	*/
	void	VisitAllVariables(const Variable::FCallback&);

	/**
	Visits all variables and calls CheckChanges() on each. This checks for changes that happend outside the CLI state callback architecture
	*/
	void	CheckAllSaveGameVariables();
}





#endif
