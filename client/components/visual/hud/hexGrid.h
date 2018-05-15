#pragma once

//#include <global_root.h>
////#include <math/vclasses.h>
#include "../display.h"
#include "objectCommands.h"
#include "dialog.h"
#include "../objectSelection.h"
#include "../scene/objectRendering.h"
#include <objects/mobileEntity.h>
#include <objects/structure/tower.h>
#include "../../voting.h"
#include "guiVoteList.h"

namespace HUD
{

#ifndef HAS_WINDOW
	namespace HexGrid
	{
		inline void	RefreshContacts()	{}
		inline void	RefreshButtonState()	{}


	}

#else




/**
Hexagonal layouter and renderer for visual commands (see shared/objects/visualCommands.h)
*/
namespace HexGrid
{
	using namespace DeltaWorks;
	namespace DX11 = Engine::DX11;


	void		RefreshContacts();
	void		RefreshButtonState();
	
	
	typedef BYTE CellAnchor;

	namespace Anchor
	{
		enum anchor_t
		{
			Right = 1,
			Top = 2,

			TopLeft = Top,
			TopRight = Top | Right,
			BottomLeft = 0,
			BottomRight = Right,

			Count = 4,
		};
	};


	struct TGridConfig
	{
		float	cellSize,
				stepX,
				stepY;
	};

	TGridConfig	GetGridConfig();
	TGridConfig	GetGridConfig(float);

	
	//static const float CellSize = 0.075f;
	//static const float stepY = (0.5f + 1.f) * CellSize;
	//static const float stepX = 0.86602540378443864676372317075294f * CellSize;

	extern count_t					currentContactListWidth;
	//static const int2 ContactListOffset(5,2);
	static const int2 DynamicRowOffset(5,1);
	static const int2 GlobalCommandOffset(3,1);
	//static const int2 EntityCommandOffset(7,1);

	/**
	Everything related to the universal buttons located in the bottom right panel
	*/
	namespace BottomRightPanel
	{

		class Icon : public Objects::Command::VisualCommand
		{
			typedef Objects::Command::VisualCommand	Super;
		public:
			int2					gridCoords;

			/**/					Icon(const String&name);
			/**/					Icon(const String&name, const String&iconName);
		};

		#pragma region PanelButtonImplementations

		class TutorialCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					TutorialCommand() : Super("Tutorial") {}

			virtual void			Refresh(bool) override;
			virtual void			Execute() override
			{
				HUD::Dialog::ShowNextTutorial(true);
			}

			virtual void			CompileLabel(Label&outResult) override
			{
				outResult << (HUD::Dialog::HasMoreTutorials() ? "New Tutorial Messages" : "No Messages");
			}
		};

		class AICommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					AICommand() : Super("Reload and run AI", "Enable Nexus AI") {}

			virtual void			Refresh(bool) override;
			virtual void			Execute() override;
		};

		class VoteCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					VoteCommand() : Super("Question Mark") {}

			virtual void			Refresh(bool) override
			{
				state = Objects::CommandFlags::None;
				if (Vote::CountActiveVotes() > 0)
					state |= Objects::CommandFlags::IsEnabled;
				if (Vote::CountNewVotes() > 0)
					state |= Objects::CommandFlags::IsFlashing;
			}
			virtual void			Execute() override
			{
				HUD::VoteList::Show(true);
			}

			virtual void			CompileLabel(Label&outResult) override
			{
				count_t active = Vote::CountActiveVotes();
				count_t nvotes = Vote::CountNewVotes();
				outResult << String(active)+" vote(s) active";
				if (nvotes)
					outResult.GetLastLine() += ", "+String(nvotes)+" demand your attention";
			}
		};

		class UndoCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					UndoCommand() : Super("Undo") {}
			/**/					UndoCommand(const String&name) : Super(name) {}



			virtual void			Refresh(bool controlsEnabled) override
			{
				MyRefresh(controlsEnabled,false);
			}

			virtual void			Execute() override
			{
				Objects::VehicleCmd::UndoOperation();
			}

			virtual void			CompileLabel(Label&outResult) override
			{
				String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
				outResult << "(Backspace) Abort current operation or undo last operation of "+name;
			}

		protected:
			void					MyRefresh(bool controlsEnabled, bool canClearMode)
			{
				state = Objects::CommandFlags::None;
				if (HUD::WindowStack::IsModal() || !controlsEnabled || !Objects::selectedEntity)
					return;
				if (Objects::selectedMobileEntity)
				{
					if (Objects::selectedMobileEntity->myTasks.IsEmpty() && (!canClearMode || Objects::activeCursorMode == Objects::CursorMode::Idle))
						return;
				}
				else
					if (Objects::selectedEntity->IsTower())
					{
						if (To<Objects::Tower>::Cast( Objects::selectedEntity )->attackQueue.IsEmpty() && (!canClearMode || Objects::activeCursorMode == Objects::CursorMode::Idle))
							return;
					}
					else
						return;

				state |= Objects::CommandFlags::IsEnabled;

			}
		};

		class AbortCommand : public UndoCommand
		{
			typedef UndoCommand		Super;
		public:
			/**/					AbortCommand() : Super("Abort") {}

			virtual void			Execute() override
			{
				Objects::VehicleCmd::AbortOperation();
			}

			virtual void			Refresh(bool controlsEnabled) override
			{
				Super::MyRefresh(controlsEnabled,true);
			}

			virtual void			CompileLabel(Label&outResult) override
			{
				String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
				outResult << "Abort any currently active operation and clear all scheduled operations of "+name;
			}
		};

		class DemolishCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					DemolishCommand() : Super("Demolish") {}
			/**/					DemolishCommand(const String&name) : Super(name) {}

			virtual void			Refresh(bool controlsEnabled) override;
			virtual void			Execute() override
			{
				Objects::CommonCmd::DemolishSelected();
			}
			virtual void			CompileLabel(Label&outResult) override;
		};

		class SleepCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					SleepCommand() : Super("Sleep") {}
			/**/					SleepCommand(const String&name) : Super(name) {}

			virtual void			Refresh(bool controlsEnabled) override
			{
				state = Objects::CommandFlags::None;
				if (HUD::WindowStack::IsModal() || !controlsEnabled || !Objects::selectedEntity)
					return;

				state |= Objects::CommandFlags::IsEnabled;

				if (Objects::selectedEntity->IsSleeping())
					state |= Objects::CommandFlags::IsPermaActive;
			}

			virtual void			Execute() override
			{
				Objects::VehicleCmd::Sleep();
			}

			virtual void			CompileLabel(Label&outResult) override
			{
				String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
				if (Objects::selectedEntity && Objects::selectedEntity->IsSleeping())
					outResult << "Wake "+name;
				else
					outResult << "Put "+name+" to sleep";
			}
		};

		class MenuCommand : public Icon
		{
			typedef Icon			Super;
		public:
			/**/					MenuCommand() : Super("Open Menu") {}
			/**/					MenuCommand(const String&name) : Super(name) {}
			virtual void			Execute() override;
			virtual void			CompileLabel(Label&outResult) override
			{
				outResult << "Open the main menu";
			}
			virtual void			Refresh(bool controlsEnabled) override	{state = Objects::CommandFlags::IsEnabled;};
		};


		#pragma endregion PanelButtonImplementations

		/**
		Updates state and presence of all buttons of this panel
		*/
		void		Refresh();
		void		RefreshMenuButtonOnly();

		/**
		Erases all commands and allocates them anew, using the current runtime configuration to determine which commands to allocate.
		The caller must subsequently call Refresh() or RefreshMenuButtonOnly().
		Better, yet Configure(*), which automatically calls the correct version
		*/
		void		RecreateCommands();
	}


	struct Group
	{
		String		name;
		Buffer0<Objects::WWorldEntity>	entries;
		index_t		currentSelection = InvalidIndex;
		float		significance = 0;
			

		void		swap(Group&other)
		{
			name.swap(other.name);
			entries.swap(other.entries);
			std::swap(currentSelection,other.currentSelection);
			std::swap(significance,other.significance);
		}
		void		UpdateSignificance();
		bool		InitSelection();
		bool		operator<(const Group&other) const;
		Objects::PWorldEntity	GetPrototype() const;
	};

	typedef std::shared_ptr<Group>				PGroup;

	/**
	Local custom visual commands
	*/
	namespace Custom
	{
		/**
		Command representing the currently selected entity.
		Clicking on the button selects the unit and moves the camera to the unit
		*/
		class EntityFocusCommand : public Objects::Command::EntityCommand
		{
		public:
			typedef	Objects::Command::EntityCommand	Super;

			/**/				EntityFocusCommand()	{}
			/**/				EntityFocusCommand(const Objects::PWorldEntity&target);
			virtual void		Execute()	override;
			virtual void		CompileLabel(Label&outResult) override;
		};

	
		class AnimationEventFocusCommand : public Objects::Command::VisualCommand
		{
		public:
			typedef	Objects::Command::VisualCommand	Super;
			M::TVec3<>				focusLocation;
			float				atTime;

			/**
			Signals that the origin of the event is friendly. Note that a lost owned unit will count as friendly as the origin is the entity itself (and hence friendly)
			*/
			bool				isFriendly;

			/**
			Declares that the activity originates in a hostile unit. The destruction of any owned entity counts as much as hostile, as an attack done by any non-owned entities
			*/
			bool				isHostileActivity;
			TUniqueID			focusTarget;
			Objects::Renderer::SmokeTrailIndex	smokeTrailIndex;
			String				fallbackTargetName;
		
			/**/				AnimationEventFocusCommand(const String&name, const String&iconName, const M::TVec3<>&focusLocation, const TUniqueID&focusID, 
															float atTime, bool isFriendly, bool isHostileActivity);

			Objects::PWorldEntity	GetTarget()	const;

			virtual void		Execute()	override;
			virtual void		Refresh(bool controlEnabled) override {}
			virtual void		CompileLabel(Label&outResult) override;
		};


		/**
		Dummy-command that merely rasterized and renders a specific value at its location
		*/
		class TextDisplayCommand : public Objects::Command::VisualCommand
		{
			typedef Objects::Command::VisualCommand	Super;
		protected:
			virtual String		GetText() const=0;
		public:
			float4				backgroundColor;	//defaults to transparent black
			float4				textColor = float4(1);
			float				textHeight = 0.4f;	//Relative text height. 1 fills the entire available height. Text width is scaled proportionally

			/**/				TextDisplayCommand(const String&name):Super(name)	{Super::state = Flags::IsEnabled;}
			virtual void		RasterizeImage() override;
			virtual	void		Refresh(bool) override	{Super::state = Flags::IsEnabled;}
			virtual void		Execute() override {};
		};

		/**
		Type-variable specialization of TextDisplayCommand.
		Assumes that T can be converted to String by any of its constructors.
		A reference to the provided address is maintained so that re-rastering will use the current value.
		*/
		template <typename T>
			class ValueReferenceCommand : public TextDisplayCommand
			{
				typedef TextDisplayCommand		Super;
			public:
				T&				value;

				/**/			ValueReferenceCommand(const String&name, T&value):Super(name),value(value)	{ RasterizeImage();}
				virtual String	GetText() const	{return String((float)value,2);}
			};


		class GroupFocusCommand : public Objects::Command::VisualCommand
		{
			typedef Objects::Command::VisualCommand	Super;
		public:
			PGroup				group;

			/**/				GroupFocusCommand() {}
			/**/				GroupFocusCommand(PGroup&group);
			virtual void		RasterizeImage() override;
			virtual void		Refresh(bool)	override {};
			virtual void		Execute()	override;
			virtual void		CompileLabel(Label&outResult) override;
			bool				operator<(const GroupFocusCommand&other) const {return group->operator<(*other.group);}
		};

		class ArrowCommand;
	}
	static const float4		friendlyBackgroundColor(0,0,1,0);
	static const float4		hostileBackgroundColor(1,0,0,0);


	/**
	Horizontal row of some commands. Its width is continuously adjusted via HexGrid::currentContactListWidth.
	All dynamic rows share the same width and are located at the same horizontal coordinates, reaching from the top down.
	Dynamic rows feature selective right and left arrows, depending on the current scroll position.
	*/
	class DynamicRow
	{
	private:
		Buffer0<Objects::Command::PVisualCommand>		lastVisible;
		std::shared_ptr<Custom::ArrowCommand>	leftCommand,rightCommand;
		index_t									offset=0;
		M::TIntRange<index_t>						visibleRange;

		void					RebuildCommands();
		void					RemoveCommandIcons();
	public:

		/**
		Visible commands of this row. Can be freely edited by the client application.
		Call Refresh() when done to make changes visible.
		Only elements addressed by @a visibleRange will be shown on screen.
		*/
		Buffer0<Objects::Command::PVisualCommand>		commands;

		/**
		Vertical position of this row with 0 being the top-most, and positive values iterating downwards
		*/
		const int				rowIndex;

		/**/					DynamicRow(int rowIndex) : rowIndex(rowIndex)	{}
		/**
		Flushes and reallocates gui cells as necessary. Redetects horizontal scrolling constraints and shows arrow buttons as necessary
		*/
		void					Refresh();
		/**
		Allocates arrow commands. Must be called once at program start or row creation.
		*/
		void					Create();
		/**
		Object destruction command. Should be called once when the program terminates.
		*/
		void					Destroy() {leftCommand.reset(); rightCommand.reset();}
		/**
		Attempts to execute a scrolling operation by at most one element. Scrolling may fail.
		Automatically refreshs the local icon composition.
		@param left Set true to scroll left, false to scroll right. Only scrolls by one element
		*/
		void					Scroll(bool left);
		/**
		Removes all visible icons of this row from the GUI, including left and right arrow icons.
		Call Refresh() to show them again.
		*/
		void					RemoveIcons();
	};


	template <typename T>
		class CollectionTemplateRow : public DynamicRow
		{
			typedef DynamicRow		Super;
		public:
			StringTable<Buffer0<T>,Swap>			collection;

			/**/					CollectionTemplateRow(int rowIndex) : Super(rowIndex)	{}
			void					Clear()	{commands.Clear(); collection.Clear();}
			void					Destroy() {Clear(); Super::Destroy();}
		};
	extern CollectionTemplateRow<Objects::WWorldEntity>	friendlyGroups,hostileGroups;
	extern DynamicRow							animationTimeLine;

	extern std::shared_ptr<Custom::EntityFocusCommand>	selectedFocus;


	namespace Custom
	{
		/**
		Horizontal arrow command pointing left or right.
		Used by DynamicRow
		*/
		class ArrowCommand : public Objects::Command::VisualCommand
		{
			typedef Objects::Command::VisualCommand	Super;
		public:
			const bool			left;
			DynamicRow *const	row;
			int2				gridCoords;

			/**/				ArrowCommand(bool left, DynamicRow*row);
			virtual void		Execute() override {row->Scroll(left);}
			virtual void		Refresh(bool controlEnabled) override {}
			virtual void		CompileLabel(Label&outResult) override
			{
				outResult << "Scroll "+name;
			}
		};
	}
		
}
}
		

DECLARE_DEFAULT_STRATEGY(HUD::HexGrid::Group,Swap);

namespace HUD {
namespace HexGrid
{
	/**
	Single visible hexagonal GUI button.
	One instance exists per currently visible button.
	All instances are stored in @a allGuiCells
	*/
	class GridCell
	{
	public:
		/**
		Command to draw texture and callback functions from.
		Used as a unique identifier for this cell.
		Only at most one cell can exist per command (address)
		*/
		Objects::Command::PVisualCommand	command;
		/**
		Location of this button on the current viewport (for faster click lookup).
		Updated by Update()
		*/
		M::Rect<>		projectedRect;
		///**
		//Configures button grid orientation.
		//Default orientation is left to right, and bottom to top.
		//Set flipX and flipY to alter this grid orientation, thus changing how command->coords are interpreted
		//*/
		//bool		flipY,flipX;
		int2		gridCoords;

		/**/		GridCell():command(nullptr)/*,flipY(false),flipX(false)*/	{}

		/**
		Updates projectedRect
		*/
		void		Update(const TGridConfig&cfg, const Engine::Aspect<>&aspect, CellAnchor);

		/**
		Determines the center floating point coordinates of this cell in the range [-1..-1+pixelAspect*2],[-1,+1]
		*/
		float2		ToFloatCoords(const TGridConfig&cfg, CellAnchor) const;
		void		IncludeX(const TGridConfig&cfg, M::TFloatRange<>&r, CellAnchor)const;

		static void Radial(float cellIndex, M::TVec2<>&rs);
		/**
		Determines whether the specified viewport coordinates lie inside the local cell button.
		This calculation takes into account the hexagonal shape of the local button, accelerated by first querying @a projectedRect
		*/
		bool		Contains(const M::TVec2<>&p) const;
	};


	/**
	Allocates a new or existing cell based on the specified command.
	Only one cell may be allocated per command. A second call with the same parameter yields the cell returned by the first call.
	Attempting to allocate an occupied location will cause failure
	@return Reference to the newly allocated or existing cell, or null if the specified cell is occupied.
	Valid only until the next call to AllocateCell() or FindAndDeleteCell().
	Do not persistently store pointers or references to the returned cell
	*/
	GridCell*		AllocateCell(const Objects::Command::PVisualCommand&cmd, const int2&coordinates, CellAnchor);
	/**
	@copydoc AllocateCell()
	@param[out] isNew Contains to true if the specified command was not previously allocated. Undefined if the function returns null
	*/
	GridCell*		AllocateCell(const Objects::Command::PVisualCommand&, const int2&coordinates, CellAnchor, bool&isNew);
	/**
	Attempts to locate and delete the first cell using the specified command.
	Fails silently if no such cell exists.
	*/
	void			FindAndDeleteCell(const Objects::Command::PVisualCommand&);
	void			FindAndDeleteCell(const int2&, CellAnchor);



	extern DX11::BaseInstructionBlock	instruction;
	extern DX11::ObjectBuffer<float4>	center,visualCodeRGB_CellSize;
	extern Engine::OrthographicAspect<> camera;



	void						Configure(const Objects::PWorldEntity&e);

	void						Create();

	inline int2			GetFocusLocation()
	{
		return int2(1,1);
		//float textWidth = 300.f / display.GetClientWidth();
		//return int2(ceil(textWidth * display.GetPixelAspect() / stepX + 0.37f),1);

	}

	extern bool				hasFocusButton;
	void					OnScreenResize();
	void					Destroy();
	bool					HandleClick();

	extern GridCell*		hoveringOver;
	extern CellAnchor		hoveringAnchor;
	extern Objects::WrappedDescription	hoveringText;
	extern float			hoveringWidth;
	extern bool				hoveringBrief;

	/*
	Renders in viewport -1 to +1
	*/
	void					RenderCommands(const TGridConfig&);
	void					RenderHint();

	/**
	Adds a visual command at the given location (base bottom left).
	If the command is a group, then its members will be layed out recursively, as space is available.
	If space is unavailable, or commands attempt to explicitly be located on occupied slots, then some commands may be rejected.
	The command shared_ptr is stored for easier removal (via ClearCommands()).
	Adding the same command multiple times will not produce multiple buttons, but move any existing one.
	@param location Grid location to place the button at. The coordinates are relative to the bottom left grid edge, and progress to the right/up
	@param cmd Command to place/memorize. Must not be empty/null
	*/
	void		AddCommand(const int2&location,const Objects::Command::PBase&cmd);

	/**
	Removes all visual command buttons, and clears out all local command references
	*/
	void		ClearCommands();

	void		RefreshContacts();
	void		Configure(const Objects::PWorldEntity&e);
	
	
	
	namespace EventLog
	{
		void			Clear();
		void			RegisterAttack(float time, bool isFriendly,  const M::TVec3<>&focusLocation, const TUniqueID&focusEntityID, const Objects::Renderer::SmokeTrailIndex&smokeTrailIndex);
		void			RegisterEntityDestruction(float time, bool isOwnedByMe,  const M::TVec3<>&focusLocation, const TUniqueID&focusEntityID, const String&entityName);
		/**
		Checks if any events have been registered in the log that signals hostile activity (attacks, units dying etc)
		*/
		bool			HasRegisteredHostileActivity();

	}

}


#endif
}
