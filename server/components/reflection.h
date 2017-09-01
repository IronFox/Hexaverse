#ifndef componentReflectionH
#define componentReflectionH

#include <global_string.h>
#include <string/cli.h>
#include <string/cli_defaults.h>

using namespace DeltaWorks;

/**
The reflection module handles all configurable variables, including loadable planets.
All variables are mapped to CLI (Command Line Interpretor) entries, that may execute arbitrary code on change.
*/
namespace Reflection
{

	extern CLI::Interpreter	state;

	namespace Variable
	{
		CONSTRUCT_ENUMERATION5(Persistence,
			Temporary,
			ConfigOnly,			//only read from configuration. not written to or extracted from savegames
		//	ConfigOverridesSavegame,	//written to savegames but overridden by configuration after load (useful for debugging or statistical purposes, mostly)
			RestoreFromSavegame,		//written to and restored from savegames. Configuration does not override these values, but they may be changed via the console
			GameInvariant,		//written to savegames, but unchangeable until a new game is created. Common type: planet type
			PlanetConfig		//written to savegames, but changeable via console in order to reconfigure a new planet
		)

		/**
		Basic variable abstruction, providing callbacks for change detection
		*/
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
				if (lastValue != ConvertToString())
				{
					OnChanged();
				}
			}

			bool				WriteToSavegame() const
			{
				return persistence != Persistence::Temporary && persistence != Persistence::ConfigOnly;
			}

			String				GetPath() const
			{
				CLI::PFolder dir = parent.lock();
				if (dir)
				{
					return dir->GetPath()+'/'+this->name;
				}
				return this->name;
			}

			virtual bool		IsWriteProtected() const override;
		};

		typedef std::shared_ptr<Base>	PBase;

		typedef std::function<void(const PBase&)>	FCallback;

		extern PBase	planetType;	
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
	Reloads all config settings from the respective ini file(s)
	*/
	void	ReloadPlanet();


	/**
	Visits all variables in the CLI state tree
	*/
	void	VisitAllVariables(const Variable::FCallback&);
	
	/**
	Signals that something has changed in the faction object composition.
	Entity state changes are not signalled.
	*/
	void	SignalFactionsChanged();
//	void	SignalUserNameChange(const Objects::PFaction&);

	/**
	Visits all variables and calls CheckChanges() on each. This checks for changes that happend outside the CLI state callback architecture
	*/
	void	CheckAllSaveGameVariables();
}





#endif
