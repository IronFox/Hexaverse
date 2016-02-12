#ifndef uiComponentH
#define uiComponentH



namespace UI
{
	namespace HexGrid
	{
		#ifdef HAS_WINDOW
			void		RefreshContacts();
			void		RefreshTutorials();
		#else
			inline void	RefreshContacts()	{}
			inline void	RefreshTutorials()	{}
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

			extern index_t						hostileOffset,
												friendlyOffset;
			//static const count_t ContactListWidth = 10;
			extern count_t					currentContactListWidth;
			static const int2 ContactListOffset(5,2);
			static const int2 FriendlyListOffset(5,1);


			class TutorialCommand : public Objects::IconCommand
			{
				typedef Objects::IconCommand	Super;

			public:
				/**/				TutorialCommand() : Super(int2(1,1),"Tutorial",
										TutorialCommand::OnExecute,
										[](const Objects::PWorldEntity&e, bool b)->BaseCommand::Flags
										{
											return TutorialCommand::IsEnabled(e,b) ? BaseCommand::Flags::IsEnabled : BaseCommand::Flags::None;
										},
										Objects::NoCosts)
										{}


				static bool				IsEnabled(const Objects::PWorldEntity&, bool)
				{
					return Dialog::HasMoreTutorials() && !Dialog::IsModal();


				}

				static void				OnExecute(const Objects::PWorldEntity&)
				{
					Dialog::ShowNextTutorial();
				}

				virtual void			CompileLabel(const Objects::PWorldEntity&,Label&outResult) override
				{
					outResult << (Dialog::HasMoreTutorials() ? "New Tutorial Messages" : "No Messages");
				}


			};

			template <bool Friendly>
				inline void				ArrowOnExecute(bool left, const Objects::PWorldEntity&)
				{
					index_t&offset = Friendly?friendlyOffset:hostileOffset;
					if (left)
					{
						if (offset > currentContactListWidth)
							offset -= currentContactListWidth;
						else
							offset = 0;
					}
					else
						offset += currentContactListWidth;
					RefreshContacts();
				}

			class ArrowCommand : public Objects::IconCommand
			{
			public:
				typedef Objects::IconCommand	Super;

				//int2 coords, const String&name, OnExecute f, CheckEnabled a, IsPermaActive p, GetCosts c
				/**/				ArrowCommand(bool left, bool friendly=false) : Super(int2(), left ? "Left":"Right",
										std::bind(
											(friendly ? (&ArrowOnExecute<true>) : (&ArrowOnExecute<false>))
											,left,std::placeholders::_1),
										[](const Objects::PWorldEntity&,bool) {return Objects::BaseCommand::Flags::IsEnabled;},
										Objects::NoCosts)
					{}

				virtual void			CompileLabel(const Objects::PWorldEntity&,Label&outResult) override
				{
					outResult << "Scroll "+name;
				}
			};

			struct Group
			{
				String		name;
				Buffer0<Objects::PWorldEntity>	entries;
				index_t		currentSelection = InvalidIndex;
				float		significance = 0;
				

				bool		operator<(const Group&other) const
				{
					return significance > other.significance;
				}

				Objects::PWorldEntity	GetPrototype() const
				{
					if (entries.first()->IsVehicle())
						return entries.first();	//good enough
					return Objects::GetAnyNonVehiclePrototypeByName(entries.first()->GetCTypeName());

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


			class EntityFocusCommand : public Objects::VisualEntityCommand
			{
			public:
				typedef	Objects::VisualEntityCommand	Super;
				Objects::PWorldEntity		target;

				//int2 coords, const String&name, const VehicleBase*&base, const Turret*&turret,   OnExecute f, CheckEnabled a, IsPermaActive p
				/**/				EntityFocusCommand()	{}
				/**/				EntityFocusCommand(const Objects::PWorldEntity&target) 
									: Super(int2(), target->GetNiceName(),target,
										std::bind(&EntityFocusCommand::OnExecute,target,std::placeholders::_1),
										[](const Objects::PWorldEntity&,bool) {return Objects::BaseCommand::Flags::IsEnabled;},
										false), 
										target(target)
				{}

				static void				OnExecute(Objects::PWorldEntity const&target, Objects::PWorldEntity const&)	//must be static because it may move before being executed
				{
					//Objects::PVehicle target = To<Objects::Vehicle>::Cast(t);
					Camera::TranslateTo(target->system.w.xyz);
				}


				virtual void	CompileLabel(const Objects::PWorldEntity&,Label&outResult) override;
			};


			static const float4 friendlyBackgroundColor(0,0,1,0);
			static const float4 hostileBackgroundColor(1,0,0,0);


			class GroupFocusCommand : public Objects::EntityCommand
			{
			public:
				typedef Objects::EntityCommand	Super;
				
				//VehicleFocusCommand	vcommand;
				//StructureFocusCommand	scommand;
				PGroup					group;

				/**/				GroupFocusCommand()
				{}

				/**/				GroupFocusCommand(PGroup&group);
				virtual void		RasterizeImage() override;


				static void			OnExecute(PGroup const&group, Objects::PWorldEntity const&_e);

				virtual void		CompileLabel(const Objects::PWorldEntity&,Label&outResult) override;

			};


			//Buffer0<VehicleFocusCommand>		vehicleCommands;
			//Buffer0<StructureFocusCommand>		structureCommands;
			extern Buffer0<GroupFocusCommand>			groupFocusCommands;
			extern EntityFocusCommand					selectedFocus;

			extern std::shared_ptr<ArrowCommand>		leftCommand,rightCommand,
														friendlyLeftCommand,friendlyRightCommand;
			extern std::shared_ptr<TutorialCommand>		tutorialCommand;
			extern Buffer0<Objects::PWorldEntity>		hostileScope;


		}
	}

	DECLARE_DEFAULT_STRATEGY(UI::HexGrid::Group,Swap);

	namespace UI
	{
		namespace HexGrid
		{
			class GridCell
			{
			public:
				Objects::BaseCommand*command;
				//int2		cellCoords;
				Rect<>		projectedRect;
				bool		flipY,flipX;
				enum type_t
				{
							EntityCommand,
							ContactListLeft,
							ContactListRight,
							ContactListEntry,
							OwnedListLeft,
							OwnedListRight,
							OwnedListEntry,
							Tutorial,

				}			type = EntityCommand;

				/**/		GridCell():command(nullptr),flipY(false),flipX(false)	{}

				void		Update(const Engine::Aspect<>&aspect);
				float2		ToFloatCoords() const;

				static void Radial(float cellIndex, TVec2<>&rs);
				bool		Contains(const TVec2<>&p) const;
			};

			extern Buffer<GridCell>			guiCells;


			void	DeleteAllCellsOfType(GridCell::type_t t);



			extern DX11::BaseInstructionBlock	instruction;
			extern DX11::ObjectBuffer<float4>	center,color;
			extern Engine::OrthographicAspect<> camera;
			extern Buffer0<Group>	friendlyGroups,hostileGroups;



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

			extern int2 at;
			inline void	Next()
			{
				at.y ++;
				if (at.y == 3)
				{
					at.y = 1;
					at.x ++;
				}

			}

			void		VisitCommand(Objects::BaseCommand&cmd);

			void		RefreshButtonState();


			void		RefreshContacts();
			void		RefreshTutorials();
			void		Configure(const Objects::PWorldEntity&e);
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
