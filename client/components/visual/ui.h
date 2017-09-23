#pragma once

#ifdef HAS_WINDOW


#include "objectSelection.h"

#include "../../ExtRenderer/DirectX11.h"
#include "dialog.h"
#include "../cameraControl.h"
#include "objectCommands.h"
#include "objectRendering.h"
#include <objects/mobileEntity.h>
#include <objects/structure.h>
#include <objects/structure/tower.h>

/**
Hexagonal button gui renderer.
Also responsible for rendering resource representations.
NOT responsible for dialogs, and component management in general (see GUI in guiElements.h for that)
*/
namespace UI
{
	namespace HexGrid
	{
		#ifdef HAS_WINDOW
			void		RefreshContacts();
			//void		RefreshTutorials();
		#else
			inline void	RefreshContacts()	{}
			//inline void	RefreshTutorials()	{}
		#endif
	}
	namespace DX11 = Engine::DX11;

	extern bool showControlsAndContacts /*=true*/;

	class IconItem
	{
	public:
		static DX11::BaseInstructionBlock	iconShader;
		static DX11::ObjectBuffer<float4>	iconColor;

		DX11::Texture	texture;
		float3			color;

		void	LoadTexture(const String&name);
		void	RenderIcon(float size=0.05f);

	};

	class Gauge : public IconItem
	{
	public:
		int	current,max,changePerRound,
			localCurrent,localChange,
			top,top0,top1;
		static const float height;
		float3	base,alternatingColor,projectedTop, projectedDelta,projectedBottom,projectedMax;
		bool	changed,enabled,usesAlternatingColors;
		float	ftop,hScale;
		char	myTag;
		#ifndef NO_HIBERNATION
			bool	isTraffic = false;	//!< Various visual modifications
		#endif
		//Objects::Resource	self;

		Objects::Geometry			geo[2];
		DX11::InstructionBlock32	instruction[2];

		static DX11::BaseInstructionBlock	shader;

		void	Destroy()
		{
			geo[0].Clear();
			geo[1].Clear();
			instruction[0].Clear();
			instruction[1].Clear();
			IconItem::texture.Clear();
		}


		void	LoadTexture(Objects::Resource res)
		{
			IconItem::LoadTexture(res.ToString());
			myTag = Objects::ResourceTag(res);
		}


		void	Set(int current, int max, int changePerRound, int localCurrent, int localChange, bool enabled);




		static void StaticCreate();
		static void StaticDestroy();

		/**/	Gauge():changed(true),usesAlternatingColors(false),current(0),max(0),changePerRound(0),localCurrent(0),localChange(0),top(0),ftop(1)
		{}

		void	UpdateTextProjection(const Engine::Aspect<>&camera);

		void	Render(BYTE target);
		void	RenderText();
		void	RenderListText(int y);
		void	ConstructPart(float radius, float lower, float upper, const float4&color, Objects::Geometry&geo)
		{
			ConstructPart(radius,lower,upper,color,color,geo);
		}

		void	ConstructPart(float radius, float lower, float upper, const float4&lowerColor, const float4&upperColor, Objects::Geometry&geo);
		void	Construct();

	};


	extern Array<Gauge>	gauges;
	#ifndef NO_HIBERNATION
		extern Gauge		trafficGauge;
	#endif
	extern IconItem		timeResource;

	extern Engine::VectorCamera<>	barProjectionCamera;
	extern Engine::OrthographicAspect<> textoutCamera;

	extern bool			resourcesAsList;

	/**
	Hexagonal layouter and renderer for visual commands (see shared/objects/visualCommands.h)
	*/
	namespace HexGrid
	{

		struct TGridConfig
		{
			float	cellSize,
					stepX,
					stepY;
		};

		TGridConfig	GetGridConfig();

		
		//static const float CellSize = 0.075f;
		//static const float stepY = (0.5f + 1.f) * CellSize;
		//static const float stepX = 0.86602540378443864676372317075294f * CellSize;

		extern count_t					currentContactListWidth;
		//static const int2 ContactListOffset(5,2);
		static const int2 DynamicRowOffset(5,1);


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
				static index_t			indexCounter;

				/**/					Icon(const String&name);
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
					Dialog::ShowNextTutorial(true);
				}

				virtual void			CompileLabel(Label&outResult) override
				{
					outResult << (Dialog::HasMoreTutorials() ? "New Tutorial Messages" : "No Messages");
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
					state = Objects::CommandFlags::None;
					if (GUI::IsModal() || !controlsEnabled || !Objects::selectedEntity)
						return;
					if (Objects::selectedMobileEntity)
					{
						if (Objects::selectedMobileEntity->myTasks.IsEmpty() && Objects::activeCursorMode == Objects::CursorMode::Idle)
							return;
					}
					else
						if (Objects::selectedEntity->IsTower())
						{
							if (To<Objects::Tower>::Cast( Objects::selectedEntity )->attackQueue.IsEmpty()  && Objects::activeCursorMode == Objects::CursorMode::Idle)
								return;
						}
						else
							return;

					state |= Objects::CommandFlags::IsEnabled;
				}

				virtual void			Execute() override
				{
					Objects::VehicleCmd::UndoOperation();
				}

				virtual void			CompileLabel(Label&outResult) override
				{
					String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
					outResult << "Abort current operation or undo last operation of "+name;
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
					if (GUI::IsModal() || !controlsEnabled || !Objects::selectedEntity)
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

			#pragma endregion PanelButtonImplementations

			/**
			Updates state and presence of all buttons of this panel
			*/
			void		Refresh(const TGridConfig&);
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

			void					RebuildCommands(const TGridConfig&cfg);
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
			void					Refresh(const TGridConfig&cfg);
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

DECLARE_DEFAULT_STRATEGY(UI::HexGrid::Group,Swap);

namespace UI
{
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
			/**
			Configures button grid orientation.
			Default orientation is left to right, and bottom to top.
			Set flipX and flipY to alter this grid orientation, thus changing how command->coords are interpreted
			*/
			bool		flipY,flipX;
			int2		gridCoords;

			/**/		GridCell():command(nullptr),flipY(false),flipX(false)	{}

			/**
			Updates projectedRect
			*/
			void		Update(const TGridConfig&cfg, const Engine::Aspect<>&aspect);

			/**
			Determines the center floating point coordinates of this cell in the range [-1..-1+pixelAspect*2],[-1,+1]
			*/
			float2		ToFloatCoords(const TGridConfig&cfg) const;

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
		@return Reference to the newly allocated or existing cell. Valid only until the next call to AllocateCell() or FindAndDeleteCell(). Do not persistently store pointers or references to the returned cell
		*/
		GridCell&		AllocateCell(const Objects::Command::PVisualCommand&cmd);
		/**
		@copydoc AllocateCell()
		@param[out] isNew Set to true if the specified command was not previously allocated
		*/
		GridCell&		AllocateCell(const Objects::Command::PVisualCommand&, bool&isNew);
		/**
		Attempts to locate and delete the first cell using the specified command.
		Fails silently if no such cell exists.
		*/
		void			FindAndDeleteCell(const Objects::Command::PVisualCommand&);



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

		extern bool					hasFocusButton;


		void						OnScreenResize();

		void						Destroy();

		bool	HandleClick();

		extern GridCell*		hoveringOver;
		extern Objects::WrappedDescription	hoveringText;
		extern float			hoveringWidth;
		extern bool			hoveringBrief;

		/*
			Renders in viewport -1 to +1
		*/
		void						RenderCommands(const TGridConfig&);
		void						RenderHint();

		/**
		Command visitor function that adds a given command to the grid.
		The command shared_ptr is stored for easier removal (via ClearCommands()).
		Adding the same command multiple times will not produce multiple buttons, but move any existing one.
		@param location Grid location to place the button at. The coordinates are relative to the bottom left grid edge, and progress to the right/up
		@param cmd Command to place/memorize. Must not be empty/null
		*/
		void		VisitAddCommand(const int2&location,const Objects::Command::PBase&cmd);

		/**
		Removes all visual command buttons, and clears out all local command references
		*/
		void		ClearCommands();

		void		RefreshContacts();
		void		Configure(const Objects::PWorldEntity&e);
	}



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


	inline void OnScreenResize()
	{
		HexGrid::OnScreenResize();
	}



	inline bool	HandleClick()
	{
			



		return HexGrid::HandleClick();
	}


	void	Create();


	/**
	Updates the uplink traffic gauge using the traffic of the currently selected entity (Objects::selectedEntity)
	Shows extra gauges if an entity is currently selected
	*/
	void	UpdateTraffic();

	/**
	Updates the uplink traffic gauge using the specified traffic of the currently selected item
	@param traffic Uplink traffic generated by the selected entity (if any; 0 otherwise)
	*/
	void	UpdateTraffic(int traffic);

	/**
	Updates traffic, assuming no entity is selected, and hides all extra (resource) gauges
	*/
	void	HideExtraGauges();


	#ifndef NO_HIBERNATION
		#define HIB(item)	item
		#define TRAFFIC	int traffic,
	#else
		#define HIB(item)
		#define TRAFFIC
	#endif

	/**
	Updates all gauges using the various resource vectors
	@param traffic Uplink traffic generated by the selected entity (if any; 0 otherwise)
	@param current Total storage in the local entity/compound
	@param max Maximum possible storage in the local entity/compound
	@param change Resource change vector (per round), 
			visualized as transparent bars up (if possitive), 
			or orange portions of the storage (if negative)
	@param localCurrent Storage of the selected entity within its compound
	*/
	void	Update(TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change);
	void	Update(TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max);
	void	Update(TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change,
					const Objects::ResourceVector&localCurrent/*, const Objects::ResourceVector&localChange, bool isEnabled*/);
	void	RenderCommands();
	void	RenderHint();
	void	Destroy();



	void	RefreshButtonState();

	void	ShowSession();
	void	RefreshSessionView();


}


namespace StatePanel
{
	static const char tagOffset = Objects::TextoutResourceTagOffset;
	static const float tagIconSize = 50.f;
	//0.0135f;

	float		OnTag(const float3&at, char tagChar, float scaleX, float scaleY);

	inline float		TagWidth(char tagChar)
	{
		return tagIconSize;
	}

	void		Render( const Objects::WorldEntity&e);

	/**
	Signifies that the state of the selected unit has changed and the panel should be recompiled
	*/
	void		SignalChange();

}
#endif



