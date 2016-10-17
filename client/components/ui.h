#ifndef uiComponentH
#define uiComponentH


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
}




#ifdef HAS_WINDOW
	#include "../ExtRenderer/DirectX11.h"
	#include "../../shared/objects.h"
	#include "dialog.h"
	#include "cameraControl.h"

	namespace UI 
	{
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
			int	current,max,
				current2=0, max2=0,
				changePerRound,
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


			void	Set(int current, int max, int current2, int max2, int changePerRound, bool enabled);




			static void StaticCreate();
			static void StaticDestroy();

			/**/	Gauge():changed(true),usesAlternatingColors(false),current(0),max(0),changePerRound(0),top(0),ftop(1)
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


		namespace HexGrid
		{
			static const float CellSize = 0.075f;
			static const float stepY = (0.5f + 1.f) * CellSize;
			static const float stepX = 0.86602540378443864676372317075294f * CellSize;

			extern count_t					currentContactListWidth;
			//static const int2 ContactListOffset(5,2);
			static const int2 DynamicRowOffset(5,1);


			namespace BottomRightPanel
			{

				class Icon : public Objects::BaseCommand
				{
					typedef Objects::BaseCommand	Super;
				public:
					int2					gridCoords;

					/**/					Icon(const int2&coords, const String&name) : Super(name)
											{
												LoadIcon();
												gridCoords = coords;
											}
				};

				class TutorialCommand : public Icon
				{
					typedef Icon			Super;
				public:
					/**/					TutorialCommand() : Super(int2(1,1),"Tutorial") {}

					virtual void			Refresh(bool)
					{
						state = Dialog::HasMoreTutorials() && !GUI::IsModal() ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
					}

					virtual void			Execute() override
					{
						Dialog::ShowNextTutorial();
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
					/**/					UndoCommand() : Super(int2(5,1),"Undo") {}
					/**/					UndoCommand(const int2&coords, const String&name) : Super(coords,name) {}

					virtual void			Refresh(bool controlsEnabled) override
					{
						state = BaseCommand::Flags::None;
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

						state |= BaseCommand::Flags::IsEnabled;
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
					/**/					AbortCommand() : Super(int2(4,1),"Abort") {}

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
					/**/					DemolishCommand() : Super(int2(2,1),"Demolish") {}
					/**/					DemolishCommand(const int2&coords, const String&name) : Super(coords,name) {}

					virtual void			Refresh(bool controlsEnabled) override
					{
						state = BaseCommand::Flags::None;
						if (GUI::IsModal() || !controlsEnabled || !Objects::selectedEntity || Objects::selectedEntity->IsIndestructible())
							return;

						state |= BaseCommand::Flags::IsEnabled;
					}

					virtual void			Execute() override
					{
						Objects::CommonCmd::DemolishSelected();
					}

					virtual void			CompileLabel(Label&outResult) override
					{
						String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
						outResult << "Permanently destroy "+name;
					}
				};

				class SleepCommand : public Icon
				{
					typedef Icon			Super;
				public:
					/**/					SleepCommand() : Super(int2(3,1),"Sleep") {}
					/**/					SleepCommand(const int2&coords, const String&name) : Super(coords,name) {}

					virtual void			Refresh(bool controlsEnabled) override
					{
						state = BaseCommand::Flags::None;
						if (GUI::IsModal() || !controlsEnabled || !Objects::selectedEntity)
							return;

						state |= BaseCommand::Flags::IsEnabled;

						if (Objects::selectedEntity->IsSleeping())
							state |= BaseCommand::Flags::IsPermaActive;
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



				/**
				Updates state and presence of all buttons of this panel
				*/
				void		Refresh();
			}


			struct Group
			{
				String		name;
				Buffer0<Objects::WWorldEntity>	entries;
				index_t		currentSelection = InvalidIndex;
				float		significance = 0;
				

				bool		operator<(const Group&other) const
				{
					return significance > other.significance;
				}

				Objects::PWorldEntity	GetPrototype() const
				{
					Objects::PWorldEntity e = entries.First().lock();
					ASSERT__(e);
					if (e->IsStructure())
						return Objects::GetPrototypeByName(e->GetCTypeName());
					return e;	//good enough

				}

				bool		InitSelection();

				void		UpdateSignificance();

				void		swap(Group&other)
				{
					name.swap(other.name);
					entries.swap(other.entries);
					std::swap(currentSelection,other.currentSelection);
					std::swap(significance,other.significance);
				}
			};

			typedef std::shared_ptr<Group>				PGroup;


			class EntityFocusCommand : public Objects::EntityCommand
			{
			public:
				typedef	Objects::EntityCommand	Super;

				//int2 coords, const String&name, const VehicleBase*&base, const Turret*&turret,   OnExecute f, CheckEnabled a, IsPermaActive p
				/**/				EntityFocusCommand()	{}
				/**/				EntityFocusCommand(const Objects::PWorldEntity&target) 
									: Super(target->GetNiceName(),target,[](){FATAL__("WTF");},
										[](bool) {return Objects::BaseCommand::Flags::IsEnabled;})
				{}

				void				Execute()	override
				{
					Objects::PWorldEntity target = reference.lock();
					if (target)
						Camera::TranslateTo(target->system.w.xyz);
				}


				virtual void	CompileLabel(Label&outResult) override;
			};

		
			class AnimationEventFocusCommand : public Objects::BaseCommand
			{
			public:
				typedef	Objects::BaseCommand	Super;
				TVec3<>				focusLocation;
				float				atTime;
				bool				isFriendly;
				TUniqueID			focusTarget;
				Objects::Renderer::SmokeTrailIndex	smokeTrailIndex;
				String				fallbackTargetName;
			
				/**/				AnimationEventFocusCommand(const String&name, const String&iconName, const TVec3<>&focusLocation, const TUniqueID&focusID, float atTime, bool isFriendly)
									: Super(name),focusLocation(focusLocation),atTime(atTime),isFriendly(isFriendly),focusTarget(focusID)
				{
					LoadIcon(iconName);
				}

				Objects::PWorldEntity	GetTarget()	const;

				virtual void	Execute()	override;
				virtual void	Refresh(bool controlEnabled) override {}
				virtual void	CompileLabel(Label&outResult) override;
			};


			static const float4 friendlyBackgroundColor(0,0,1,0);
			static const float4 hostileBackgroundColor(1,0,0,0);


			class GroupFocusCommand : public Objects::BaseCommand
			{
			public:
				typedef Objects::BaseCommand	Super;
				
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

			/**
			Horizontal row of some commands. Its width is continuously adjusted via HexGrid::currentContactListWidth.
			All dynamic rows share the same width and are located at the same horizontal coordinates, reaching from the top down.
			Dynamic rows feature selective right and left arrows, depending on the current scroll position.
			*/
			class DynamicRow
			{
			private:
				Buffer0<Objects::PBaseCommand>			lastVisible;
				std::shared_ptr<ArrowCommand>			leftCommand,rightCommand;
				index_t									offset=0;
				TIntRange<index_t>						visibleRange;

				void									RebuildCommands();
				void									RemoveCommandIcons();
			public:

				/**
				Visible commands of this row. Can be freely edited by the client application.
				Call Refresh() when done to make changes visible.
				Only elements addressed by @a visibleRange will be shown on screen.
				*/
				Buffer0<Objects::PBaseCommand>			commands;

				/**
				Vertical position of this row with 0 being the top-most, and positive values iterating downwards
				*/
				const int								rowIndex;

				/**/									DynamicRow(int rowIndex) : rowIndex(rowIndex)	{}
				/**
				Flushes and reallocates gui cells as necessary. Redetects horizontal scrolling constraints and shows arrow buttons as necessary
				*/
				void									Refresh();
				/**
				Allocates arrow commands. Must be called once at program start or row creation.
				*/
				void									Create();
				/**
				Object destruction command. Should be called once when the program terminates.
				*/
				void									Destroy() {leftCommand.reset(); rightCommand.reset();}
				/**
				Attempts to execute a scrolling operation by at most one element. Scrolling may fail.
				Automatically refreshs the local icon composition.
				@param left Set true to scroll left, false to scroll right. Only scrolls by one element
				*/
				void									Scroll(bool left);
				/**
				Removes all visible icons of this row from the GUI, including left and right arrow icons.
				Call Refresh() to show them again.
				*/
				void									RemoveIcons();
			};


			/**
			Horizontal arrow command pointing left or right.
			Used by DynamicRow
			*/
			class ArrowCommand : public Objects::BaseCommand
			{
			public:
				typedef Objects::BaseCommand	Super;
				const bool						left;
				DynamicRow *const				row;
				int2							gridCoords;

				//int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p, GetCosts c
				/**/				ArrowCommand(bool left, DynamicRow*row) : Super(left ? "Left":"Right"), left(left),row(row)
									{
										LoadIcon();
									}
				virtual void Execute() override
				{
					row->Scroll(left);
				}

				virtual void Refresh(bool controlEnabled) override {}

				virtual void			CompileLabel(Label&outResult) override
				{
					outResult << "Scroll "+name;
				}
			};


			template <typename T>
			class CollectionTemplateRow : public DynamicRow
			{
				typedef DynamicRow		Super;
			public:
				StringTable<Buffer0<T>,Swap>			collection;

				/**/									CollectionTemplateRow(int rowIndex) : Super(rowIndex)	{}
				void									Clear()	{commands.Clear(); collection.Clear();}
				void									Destroy() {Clear(); Super::Destroy();}
			};
			extern CollectionTemplateRow<Objects::WWorldEntity>	friendlyGroups,hostileGroups;
			extern DynamicRow							animationTimeLine;

			extern std::shared_ptr<EntityFocusCommand>	selectedFocus;



		}
	}

	DECLARE_DEFAULT_STRATEGY(UI::HexGrid::Group,Swap);

	namespace UI
	{
		/**
		All methods and classes associated with the HUD hexagonal button grid
		*/
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
				Objects::PBaseCommand	command;
				/**
				Location of this button on the current viewport (for faster click lookup).
				Updated by Update()
				*/
				Rect<>		projectedRect;
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
				void		Update(const Engine::Aspect<>&aspect);

				/**
				Determines the center floating point coordinates of this cell in the range [-1..-1+pixelAspect*2],[-1,+1]
				*/
				float2		ToFloatCoords() const;

				static void Radial(float cellIndex, TVec2<>&rs);
				/**
				Determines whether the specified viewport coordinates lie inside the local cell button.
				This calculation takes into account the hexagonal shape of the local button, accelerated by first querying @a projectedRect
				*/
				bool		Contains(const TVec2<>&p) const;
			};


			/**
			Allocates a new or existing cell based on the specified command.
			Only one cell may be allocated per command. A second call with the same parameter yields the cell returned by the first call.
			@return Reference to the newly allocated or existing cell. Valid only until the next call to AllocateCell() or FindAndDeleteCell(). Do not persistently store pointers or references to the returned cell
			*/
			GridCell&		AllocateCell(const Objects::PBaseCommand&cmd);
			/**
			@copydoc AllocateCell()
			@param[out] isNew Set to true if the specified command was not previously allocated
			*/
			GridCell&		AllocateCell(const Objects::PBaseCommand&, bool&isNew);
			/**
			Attempts to locate and delete the first cell using the specified command.
			Fails silently if no such cell exists.
			*/
			void			FindAndDeleteCell(const Objects::PBaseCommand&);



			extern DX11::BaseInstructionBlock	instruction;
			extern DX11::ObjectBuffer<float4>	center,color;
			extern Engine::OrthographicAspect<> camera;



			void						Configure(const Objects::PWorldEntity&e);

			void						Create();

			inline int2			GetFocusLocation()
			{
				return int2(1,1);
				//float textWidth = 300.f / display.clientWidth();
				//return int2(ceil(textWidth * display.pixelAspect() / stepX + 0.37f),1);

			}

			extern bool					hasFocusButton;


			void						OnWindowResize();

			void						Destroy();

			bool	HandleClick();

			extern GridCell*		hoveringOver;
			extern Objects::BaseCommand::Label	hoveringText;
			extern float			hoveringWidth;
			extern bool			hoveringBrief;

			/*
				Renders in viewport -1 to +1
			*/
			void						Render();

			/**
			Command visitor function that adds a given command to the grid.
			The command shared_ptr is stored for easier removal (via ClearCommands()).
			Adding the same command multiple times will not produce multiple buttons, but move any existing one.
			@param location Grid location to place the button at. The coordinates are relative to the bottom left grid edge, and progress to the right/up
			@param cmd Command to place/memorize. Must not be empty/null
			*/
			void		VisitAddCommand(const int2&location,const Objects::PBaseCommand&cmd);

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
			void			RegisterAttack(float time, bool isFriendly,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, const Objects::Renderer::SmokeTrailIndex&smokeTrailIndex);
			void			RegisterEntityDestruction(float time, bool isOwnedByMe,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, const String&entityName);


		}


		inline void OnWindowResize()
		{
			HexGrid::OnWindowResize();
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
		void	Update(TRAFFIC	const Objects::ResourceVector&current, const Objects::ResourceVector&max, 
								const Objects::ResourceVector&current2, const Objects::ResourceVector&max2, 
								const Objects::ResourceVector&change);
		void	Update(TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max);
		//void	Update(TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change,
		//				const Objects::ResourceVector&localCurrent/*, const Objects::ResourceVector&localChange, bool isEnabled*/);
		void	Render();
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

	}
#endif



#endif
