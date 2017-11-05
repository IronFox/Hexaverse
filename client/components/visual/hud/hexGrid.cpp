#include "hexGrid.h"

#include "../../cameraControl.h"
#include <objects/prototypes.h>
#include <objects/faction.h>
#include "../../network.h"
#include "../objectChoices.h"
#include "../tutorial.h"
#include "resourceGauges.h"
#include "../../configurationMode.h"

namespace HUD
{

namespace HexGrid
{
	CollectionTemplateRow<Objects::WWorldEntity>	friendlyGroups(0),hostileGroups(1);
	DynamicRow										animationTimeLine(2);

	count_t											currentContactListWidth = 10;
	std::shared_ptr<Custom::EntityFocusCommand>		selectedFocus;


	
	
	bool		Group::operator<(const Group&other) const
	{
		return significance > other.significance;
	}

	Objects::PWorldEntity	Group::GetPrototype() const
	{
		Objects::PWorldEntity rs;
		foreach (entries,e0)
		{
			rs = e0->lock();
			if (rs)
				break;
		}
		if (!rs || rs->IsVehicle())
			return rs;
		return Objects::Prototypes::Find(rs->GetType());
	}

	bool		Group::InitSelection()
	{
		if (currentSelection == InvalidIndex)
		{
			currentSelection = InvalidIndex;
			foreach (entries,e)
				if (e->lock() == Objects::selectedEntity)
				{
					currentSelection = e - entries.pointer();
					break;
				}
			if (currentSelection == InvalidIndex)
			{
				float distance = std::numeric_limits<float>::max();
					
				index_t at = 0;
				for (const auto&entry : entries)
				{
					Objects::PWorldEntity e = entry.lock();
					DBG_ASSERT__(e);
					float d = Vec::quadraticDistance(e->system.w.xyz,Camera::scene.location);
					if (d < distance)
					{
						distance = d;
						currentSelection = at;
					}
					at++;
				}
			}
			return true;
		}
		return false;

	}

	void		Group::UpdateSignificance()
	{
		Objects::PWorldEntity e = entries.First().lock();
		DBG_ASSERT__(e);
		significance = e->GetConstructionCost();



		Objects::PTurretBase b = e->GetTurretBase();
		if (b && b->turret->CanAttack())
		{
			significance *= 3.f;
		}
		if (e->IsVehicle())
			significance *= 10.f;
		if (e->IsDeadOrGhost())
		{
			if (e->isOwnedByMe)
				significance *= 15.f / (1.f + M::Sqr(e->health));
			else
				significance *= 0.8f;
		}
		UINT32 crc = CRC32::Sequence().Append(e->GetTypeName().c_str(),e->GetTypeName().length()).Finish();
		Random random(crc);
		significance *= random.NextFloat(0.95f,1.07f);
	}



	namespace Custom
	{

		/*virtual override*/ void		TextDisplayCommand::RasterizeImage()
		{
			Objects::GeometryImage::BeginRendering(backgroundColor,1.f);

			display.Pick(Engine::OrthographicAspect<>());	//all identity, -1 to +1
			textout.PushState();


			textout.SetScale(2. * textHeight / textout.GetUnscaledHeight());

			String text = this->GetText();
			textout.MoveTo(-textout.GetScaledWidth(text)*0.5f,textHeight*0.7f);
			textout.SetColor(textColor);
			textout << text;
			textout.PopState();

			Super::texture = Objects::GeometryImage::EndRendering();
		}


		ArrowCommand::ArrowCommand(bool left, DynamicRow*row) : Super(left ? "Left":"Right"), left(left),row(row)
		{
			LoadIcon();
		}


		EntityFocusCommand::EntityFocusCommand(const Objects::PWorldEntity&target) 
							: Super(target->GetNiceName(),target,[](){FATAL__("WTF");},
								[](bool) {return Objects::CommandFlags::IsEnabled;})
		{}

		/*virtual override*/ void	EntityFocusCommand::Execute()
		{
			Objects::PWorldEntity target = reference.lock();
			if (target)
				Camera::TranslateTo(target->system.w.xyz);
		}


		AnimationEventFocusCommand::AnimationEventFocusCommand(const String&name, const String&iconName, const M::TVec3<>&focusLocation, const TUniqueID&focusID, float atTime, bool isFriendly, bool isHostileActivity)
							: Super(name),focusLocation(focusLocation),atTime(atTime),isFriendly(isFriendly),isHostileActivity(isHostileActivity),focusTarget(focusID)
		{
			LoadIconByName(iconName);
		}



		Objects::PWorldEntity	AnimationEventFocusCommand::GetTarget()	const
		{
			if (focusTarget != TUniqueID::Empty())
			{
				Objects::PWorldEntity e = Objects::GetEntity(focusTarget);
				if (e)
					return e;
				auto marker = Objects::Marker::FindByReference(focusTarget);
				if (marker)
					return marker->visualization;
			}
			return Objects::PWorldEntity();
		}

		/*virtual override*/ void				AnimationEventFocusCommand::Execute()
		{
			Camera::TranslateTo(focusLocation);
			Objects::PWorldEntity e = GetTarget();
			if (e)
				Objects::Select(e,Network::ControlEnabled());
			Objects::Renderer::HighlightSmokeTrail(smokeTrailIndex);
			state = Objects::CommandFlags::IsEnabled;
		}

		/*virtual override*/ void				AnimationEventFocusCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();
			Objects::PWorldEntity e = GetTarget();
			String hostile = isFriendly? "" : "hostile ";
			String t = " at t="+String(atTime)+" sec";
			if (e)
				outResult << "Focus on "+hostile + name+" of "+e->GetNiceName()+t;
			else
				if (fallbackTargetName.IsNotEmpty())
					outResult << "Focus on "+hostile + name+" of "+fallbackTargetName+t;
				else
					outResult << "Focus on "+hostile + name+t;
		}


			

		/*virtual override*/ void	EntityFocusCommand::CompileLabel(Label&outResult)
		{
			outResult.BeginBrief();
			outResult << name;
			Objects::PWorldEntity target = this->reference.lock();
			if (!target)
				return;
			if (target->isOwnedByMe)
			{
				outResult << "Owner: me";
				target->AppendDeploymentCount(outResult, target->owner);
			}
			elif (target->owner)
				outResult << "Owner: "+target->owner->GetName();
			if (target->IsAlive())
			{
				if (target->IsIndestructible())
					outResult << " Indestructible";
				else
					outResult << String(" Health: " +String( target->health) + "/" + target->DescribeMaxHealth());
				if (target->IsHibernating())
					outResult << " Hibernating";
			}
			else
			{
				outResult << " Ghost created " + String(-target->health) + " round(s) ago";
			}

			//outResult << "Health: "+String(target->health)+"/"+String(target->GetMaxHealth());
			Objects::PTurretBase base = target->GetTurretBase();
			if (base && base->turret->CanAttack())
				outResult << "Offensive ("+String(base->GetAttacksPerRound())+"x"+String(base->turret->GetDamageLevel(target->owner))+")";
			if (target->empDamageLevel)
				outResult << "EMP-Damage detected ("+String(target->empDamageLevel)+")";

			outResult.MarkBriefEnd();
			target->DescribeType(outResult,Objects::WorldEntity::DescIncludePrice/* | Objects::WorldEntity::DescIncludeUnitCount*/);

		}



		/**/	GroupFocusCommand::GroupFocusCommand(PGroup&group):
								Super(group->name+" ("+String(group->entries.Count())+")"),
								group(group)
		{
			RasterizeImage();
			
		}

		/*virtual override*/ void	GroupFocusCommand::RasterizeImage()
		{
			Objects::PWorldEntity e = group->entries.First().lock();
			if (!e)
				return;

			const float4&background = e->isOwnedByMe ? friendlyBackgroundColor : hostileBackgroundColor;
			Objects::Command::EntityCommand::UpdateTexture(*this,e,false,background);
		}


		/*virtual override*/ void				GroupFocusCommand::Execute()
		{
			if (!group->InitSelection() || group->entries[group->currentSelection].lock() == Objects::selectedEntity)
				group->currentSelection = (group->currentSelection+1) % group->entries.Count();
			Objects::PWorldEntity e = group->entries[group->currentSelection].lock();
			if (e)
			{
				Camera::TranslateTo(e->system.w.xyz);
				Objects::Select(e,Network::ControlEnabled());
			}
		}


		/*virtual override*/ void	GroupFocusCommand::CompileLabel(Label&outResult)
		{
			//group->InitSelection();
			outResult.BeginBrief();

			Objects::PWorldEntity proto = group->GetPrototype();
			Objects::PWorldEntity instance = group->entries.First().lock();

			if (!instance)
				return;
				
			outResult << name;
			if (instance->isOwnedByMe)
			{
//					outResult << "Owner: me";
				if (instance->IsAlive())
				{
					proto->AppendDeploymentCount(outResult, instance->owner);
				}
			}
			elif (instance->owner)
				outResult << "Owner: "+instance->owner->GetName();
			if (instance->IsAlive())
			{
				//if (e->IsIndestructible())
				//	outResult << " Indestructible";
				//else
				//	outResult << String(" Health: " +String( e->health) + "/" + String(e->GetMaxHealth()));
			}
			else
			{
				outResult << "Ghost";
			}

			#ifndef NO_HIBERNATION
				if (instance->ToHibernating()->IsHibernating())
					outResult << "Hibernating";
			#endif


			//outResult << "Health: "+String(target->health)+"/"+String(target->GetMaxHealth());
			Objects::PTurretBase b = proto->GetTurretBase();
			if (b && b->turret->CanAttack())
				outResult << "Offensive ("+String(b->GetAttacksPerRound())+"x"+String(b->turret->GetDamageLevel(instance->owner))+")";
//					if (target->empDamageLevel)
//					outResult << "EMP-Damage detected ("+String(target->empDamageLevel)+")";

			outResult.MarkBriefEnd(true);
			Objects::OwnerParameter old = proto->owner;
			proto->owner = instance->owner;	//crude, but either it's set already, or this is a global prototype in which case ownership don't matter
			proto->DescribeType(outResult,Objects::WorldEntity::DescIncludePrice /*| Objects::WorldEntity::DescIncludeUnitCount*/);
			proto->owner = old;

			//if (e->IsVehicle())
			//{
			//	VehicleFocusCommand vcommand(gridCoords,To<Objects::Vehicle>::Cast(e));
			//	vcommand.CompileLabel(outResult);
			//	return;
			//}
			//if (e->IsStructure())
			//{
			//	StructureFocusCommand scommand(gridCoords,To<Objects::Structure>::Cast(e));
			//	scommand.CompileLabel(outResult);
			//	return;
			//}
			//outResult << "Unsupported entity: "+e->GetName();
		}


		static int CollectionCommandCompare(const Objects::Command::PVisualCommand&a,const Objects::Command::PVisualCommand&b)
		{
			const GroupFocusCommand&g0 = *(GroupFocusCommand*)a.get();
			const GroupFocusCommand&g1 = *(GroupFocusCommand*)b.get();
			if (g0 < g1)
				return -1;
			return 1;
		}



	}

	void	DynamicRow::Create()
	{
		leftCommand.reset(new Custom::ArrowCommand(true,this));
		leftCommand->gridCoords = DynamicRowOffset + int2(0,rowIndex);
		rightCommand.reset(new Custom::ArrowCommand(false,this));
		rightCommand->gridCoords = DynamicRowOffset + int2(currentContactListWidth+1,rowIndex);
	}

	void	DynamicRow::Scroll(bool left)
	{
		if (left)
		{
			if (offset > currentContactListWidth)
				offset -= currentContactListWidth;
			else
				offset = 0;
		}
		else
		{
			offset += currentContactListWidth;
		}

		Refresh();
	}

	void	DynamicRow::Refresh()
	{
		bool moreRight;
		{
			//Buffer0<Group>&groups = hostileGroups.groups;
			index_t newOffset = this->offset;

			const count_t scopeWidth = this->commands.Count();

			if (newOffset > 0 && newOffset + currentContactListWidth > scopeWidth)
			{
				if (scopeWidth >= currentContactListWidth)
					newOffset = scopeWidth - currentContactListWidth;
				else
					newOffset = 0;
			}
			visibleRange = M::IntRange(index_t(0),scopeWidth);
			if (newOffset > 0)
				visibleRange = M::IntRange(newOffset,scopeWidth);
			moreRight = visibleRange.GetExtent() > currentContactListWidth;
			if (moreRight)
				visibleRange.end = visibleRange.start + currentContactListWidth;
			this->offset = newOffset;
		}
		if (offset > 0)
		{
			AllocateCell(leftCommand,leftCommand->gridCoords,Anchor::TopLeft);
		}
		else
			FindAndDeleteCell(leftCommand);
		if (moreRight)
		{
			AllocateCell(rightCommand, DynamicRowOffset + int2(currentContactListWidth+1,rowIndex),Anchor::TopLeft);
		}
		else
			FindAndDeleteCell(rightCommand);

//				rightCommand->gridCoords.x = int(ContactListOffset.x + 1+currentContactListWidth);

		RebuildCommands();
	}

	void	DynamicRow::RemoveCommandIcons()
	{
		foreach (lastVisible,v)
			FindAndDeleteCell(*v);
		lastVisible.Clear();
	}

	void	DynamicRow::RemoveIcons()
	{
		RemoveCommandIcons();
		FindAndDeleteCell(rightCommand);
		FindAndDeleteCell(leftCommand);
	}

	void							DynamicRow::RebuildCommands()
	{
		RemoveCommandIcons();
		if (!showControlsAndContacts)
			return;
		for (index_t i = 0; i < visibleRange.GetExtent(); i++)
		{
			const Objects::Command::PVisualCommand cmd = commands[i+visibleRange.start];
			GridCell*cell = AllocateCell(cmd,DynamicRowOffset + int2(i+1,rowIndex),Anchor::TopLeft);
			if (cell)
				lastVisible << cmd;
		}
	}






	namespace BottomRightPanel
	{
		typedef std::shared_ptr<Icon>	PIcon;
		Buffer0<PIcon >				commands;

		/*static*/ index_t			Icon::indexCounter = 0;
			/**/					Icon::Icon(const String&name) : Super(name)
									{
										LoadIcon();
										indexCounter++;
										gridCoords = int2(1+indexCounter,1);
									}


		void Create()
		{
			commands << PIcon( new TutorialCommand() );

			commands << PIcon( new DemolishCommand() );
			commands << PIcon( new SleepCommand() );
			commands << PIcon( new AbortCommand() );
			commands << PIcon( new UndoCommand() );

		}

		void Destroy()
		{
			commands.Clear();
		}

		static void Add(const int2&coords, const Objects::Command::PVisualCommand&cmd)
		{
			GridCell*cell = AllocateCell(cmd,coords,Anchor::BottomRight);
			cmd->Refresh(Network::ControlEnabled());
		}

		void			Refresh()
		{
			if (!showControlsAndContacts)
			{
				foreach (commands,c)
					FindAndDeleteCell(*c);
				return;
			}
			foreach (commands,c)
				Add((*c)->gridCoords, *c);
		}

		/*virtual override*/ void			DemolishCommand::CompileLabel(Label&outResult)
		{
			String name = Objects::selectedEntity ? Objects::selectedEntity->GetNiceName() : "selected unit";
			outResult << "Permanently destroy "+name;
			if (Objects::selectedEntity)
			{
				if (Objects::selectedEntity->IsIndestructible())
					outResult << "(Unit is indestructible)";
				elif (RuntimeConfig::tutorialMode)
					outResult << "(Destruction not allowed right now)";
			}
			else
				outResult << "(none selected)";
		}

		/*virtual override*/ void			DemolishCommand::Refresh(bool controlsEnabled)
		{
			state = Objects::CommandFlags::None;
			if (HUD::WindowStack::IsModal() || !controlsEnabled || !Objects::selectedEntity || Objects::selectedEntity->IsIndestructible() || RuntimeConfig::tutorialMode)
				return;

			state |= Objects::CommandFlags::IsEnabled;
		}

		/*virtual override*/ void			TutorialCommand::Refresh(bool)
		{
			state = HUD::Dialog::HasMoreTutorials() && !HUD::WindowStack::IsModal() && !RuntimeConfig::tutorialMode ? Objects::CommandFlags::IsEnabled : Objects::CommandFlags::None;
		}
	}



	static float GetLeftCellOffset(const TGridConfig&cfg, int cellY);
	static float GetRightCellOffset(const TGridConfig&cfg, int cellY, bool snapX)
	{
		if (snapX)
		{
			float left = GetLeftCellOffset(cfg,cellY);
			float space = GetRightCellOffset(cfg,cellY,false) - left;
			int intSpace = space / (cfg.stepX*2);
			return left + intSpace * (cfg.stepX*2);
		}

		return (-1+2.f * display.GetPixelAspect()) - float(1 - (cellY % 2)) * cfg.stepX;
	}

	static float GetLeftCellOffset(const TGridConfig&cfg, int cellY)
	{
		return -1.f + float(1 - (cellY % 2)) * cfg.stepX;
	}

	void		GridCell::Update(const TGridConfig&cfg, const Engine::Aspect<>&aspect, CellAnchor anchor)
	{
		float2 center = ToFloatCoords(cfg, anchor);
		float2 topRight = center + float2(cfg.cellSize*0.9f),
				bottomLeft = center - float2(cfg.cellSize*0.9f);
		float3 topRightP,bottomLeftP;
		aspect.Project(float3(topRight,0),topRightP);
		aspect.Project(float3(bottomLeft,0),bottomLeftP);

		projectedRect.x.min = bottomLeftP.x * 0.5f + 0.5f;
		projectedRect.x.max = topRightP.x * 0.5f + 0.5f;
		projectedRect.y.min = bottomLeftP.y * 0.5f + 0.5f;
		projectedRect.y.max = topRightP.y * 0.5f + 0.5f;
	}
	float2		GridCell::ToFloatCoords(const TGridConfig&cfg, CellAnchor anchor) const
	{
		float x;
		const bool flipX = (anchor & Anchor::Right) != 0;
		const bool flipY = (anchor & Anchor::Top) != 0;
		if (flipX)
			x = GetRightCellOffset(cfg,gridCoords.y,!flipY) - cfg.stepX *2 * gridCoords.x;
		else
			x = GetLeftCellOffset(cfg,gridCoords.y) + cfg.stepX *2 * gridCoords.x;
		float2	rs(x, -1.f + cfg.stepY * gridCoords.y);
		if (flipY)
			rs.y = -rs.y;
		return rs;
	}

	/*static*/ void GridCell::Radial(float cellIndex, M::TVec2<>&rs)
	{
		float a = cellIndex / 6.0f * M_PIF * 2.0f;
		rs.x = -sin(a);
		rs.y = cos(a);
	}

	void		GridCell::IncludeX(const TGridConfig&cfg, M::TFloatRange<>&r, CellAnchor a)const
	{
		float2 c = ToFloatCoords(cfg,a);
		r.Include(c.x-cfg.stepX);
		r.Include(c.x+cfg.stepX);
	}


	bool		GridCell::Contains(const M::TVec2<>&p) const
	{
		if (!projectedRect.Contains(p))
			return false;

		M::TVec2<> relative;
		projectedRect.Relativate(p,relative);
		relative = relative * 2.f - 1.f;
		M::TVec2<> prev;
		Radial(0,prev);
		for (int i = 0; i < 6; i++)
		{
			M::TVec2<> next;
			Radial((i+1)%6,next);
			M::TVec2<> change,sb;
			Vec::sub(next,prev,change);
			swp(change.x,change.y);
			change.x = -change.x;
			Vec::sub(relative,prev,sb);
			float dt = Vec::dot(change,sb);
			if (dt < 0)
				return false;

			prev = next;
		}
		return true;
	}


	/**
	True if any changes have been detected in hex-grid cell composition, but area min-max have not been updated yet
	*/
	bool						cellCompositionChanged = true;
	class Region;

	void		AddCommandGroup(const int2&root, const int2&childOffset, const Objects::Command::Container&container);
	void		AddCommand(const int2&root, const int2&location,const Objects::Command::PBase&cmd);


	class ScrollArea
	{
		void		AllocateButtons();
		void		ClampOffset()
		{
			if (lastDemand-lastOffset < GetStep())
			{
				lastOffset = lastDemand - GetStep();
			}
		}
	public:
		count_t		lastSpace=0,lastDemand=0;
		index_t		lastOffset=0;
		int2		origin;
		Region		*parentRegion;
		Objects::Command::PIconCommand	leftCommand,rightCommand;

		count_t		GetStep() const
		{
			return lastSpace-2;
		}


		bool		CanScrollLeft() const
		{
			return lastOffset > 0;
		}

		void		ScrollLeft()
		{
			if (CanScrollLeft())
			{
				if (lastOffset < GetStep())
					lastOffset = 0;
				else
					lastOffset -=GetStep();
				Configure(Objects::selectedEntity);
			}
		}

		bool		CanScrollRight() const
		{
			return (lastOffset + GetStep() < lastDemand);
		}

		void		ScrollRight()
		{
			if (CanScrollRight())
			{
				lastOffset+=GetStep();
				ClampOffset();
				Configure(Objects::selectedEntity);
			}
		}

		void		AddCommands(const int2&root, const Objects::Command::CommandGroup::Row&row)
		{
			count_t window = lastSpace-2;
			int2 offset = origin;
			offset.x++;
			AddCommand(root,origin,leftCommand);
			AddCommand(root ,origin+ int2(lastSpace-1,0),rightCommand);
			for (index_t i = 0; i < window; i++)
			{
				AddCommand(root,offset + int2(i ,0),row[i+ lastOffset]);
			}
		}

		void		Update(count_t space, count_t demand)
		{
			ASSERT_LESS__(space,demand);
			ASSERT_GREATER__(space,2);
			lastSpace = space;
			lastDemand = demand;
			ClampOffset();
		}

		ScrollArea&	Setup(Region*r, const int2&origin, count_t space, count_t demand)
		{
			ASSERT_LESS__(space,demand);
			ASSERT_GREATER__(space,2);
			this->origin = origin;
			lastSpace = space;
			lastDemand = demand;
			parentRegion = r;
			AllocateButtons();

			return *this;
		}



	};



	class Region
	{
	public:
		GenericHashTable<int2,ScrollArea>	scrollAreas;
		IndexTable<M::TFloatRange<> >	rowRanges;
		M::TIntRange<int>				yRanges;
		/**
		Linear list of all hex-grid cells
		*/
		Buffer<GridCell>			allGuiCells;
		M::Rect<>					unscaledCellRegion;


		ScrollArea&		GetScrollArea(const int2&origin, count_t space, count_t demand)
		{
			auto ptr = scrollAreas.QueryPointer(origin);
			if (ptr)
			{
				ptr->Update(space,demand);
				return *ptr;
			}
			return scrollAreas.Set(origin).Setup(this,origin,space,demand);
		}
		ScrollArea&		GetScrollArea(const int2&origin)
		{
			auto ptr = scrollAreas.QueryPointer(origin);
			ASSERT_NOT_NULL__(ptr);
			return *ptr;
		}

		bool			HasCell(const int2&coords)
		{
			foreach (allGuiCells,c)
				if (c->gridCoords == coords)
					return true;
			return false;
		}

		GridCell*		AllocateCell(const Objects::Command::PVisualCommand&cmd, const int2&coordinates, CellAnchor myAnchor, bool&isNew)
		{
			foreach (allGuiCells,c)
			{
				if (c->command == cmd)
				{
					isNew = false;
					return c;
				}
				if (c->gridCoords == coordinates)
					return nullptr;
			}
			GridCell&c = allGuiCells.Append();
			c.command = cmd;
			c.gridCoords = coordinates;
			isNew = true;
			cellCompositionChanged = true;
			c.Update(GetGridConfig(),camera,myAnchor);
			return &c;
		}
	};

	Region						region[Anchor::Count];
	float						hexGridScale = 1;
	
	void		ScrollArea::AllocateButtons()
	{
		int2 coords = this->origin;
		Region*r = this->parentRegion;
		

		leftCommand.reset(new Objects::Command::IconCommand("Left",[coords,r]()
		{
			r->GetScrollArea(coords).ScrollLeft();
		},
		[coords,r](bool)->Objects::Command::Base::Flags
		{
			auto rs = Objects::Command::Base::Flags::None;
			if (r->GetScrollArea(coords).CanScrollLeft())
				rs |= Objects::Command::Base::Flags::IsEnabled;
			return rs;
		}
		));
		rightCommand.reset(new Objects::Command::IconCommand("Right",[coords,r]()
		{
			r->GetScrollArea(coords).ScrollRight();
		},
		[coords,r](bool)->Objects::Command::Base::Flags
		{
			auto rs = Objects::Command::Base::Flags::None;
			if (r->GetScrollArea(coords).CanScrollRight())
				rs |= Objects::Command::Base::Flags::IsEnabled;
			return rs;
		}

		));

	}


	void						VisitAllCells(const std::function<void(GridCell&, CellAnchor)>&f)
	{
		for (CellAnchor a = 0; a < Anchor::Count; a++)
		{
			foreach (region[a].allGuiCells,c)
				f(*c,a);
		}
	}

	GridCell*					ScanAllCells(const std::function<bool(GridCell&, CellAnchor)>&f)
	{
		for (CellAnchor a = 0; a < Anchor::Count; a++)
		{
			foreach (region[a].allGuiCells,c)
				if (f(*c,a))
					return c;
		}
		return nullptr;
	}

	template <typename T>
	static T&		Set(IndexTable<T>&table, index_t row, const T&def)
	{
		T*ptr = table.QueryPointer(row);
		if (ptr)
			return *ptr;
		return table.Set(row,def);
	}



	void			RecomputeCellBoundsIfNecessary()
	{
		if (!cellCompositionChanged)
			return;
		cellCompositionChanged = false;

		const auto cfg = GetGridConfig(1);
		const auto empty = M::MaxInvalidRange<float>();
		const auto emptyI = M::TIntRange<int>::Invalid;

		for (CellAnchor a = 0; a < Anchor::Count; a++)
		{
			auto&r = region[a];
			r.rowRanges.Clear();
			r.yRanges = emptyI;

			foreach (r.allGuiCells,c)
			{
				M::TFloatRange<>&range = Set(r.rowRanges, c->gridCoords.y, empty);
				c->IncludeX(cfg,range,a);
				r.yRanges.Include(c->gridCoords.y);
			}
		}

	
	}

	TGridConfig	GetGridConfig(float extraScale)
	{
		static const float CellSize = 0.075f;
		static const float stepY = (0.5f + 1.f) * CellSize;
		static const float stepX = 0.86602540378443864676372317075294f * CellSize;

		const float scale = UI::GetScale()*extraScale;
		TGridConfig rs;
		rs.cellSize = CellSize * scale;
		rs.stepX = stepX * scale;
		rs.stepY = stepY * scale;
		return rs;
	}

	TGridConfig	GetGridConfig()
	{
		return GetGridConfig(hexGridScale);
	}

	void			UpdateAllCells()
	{
		auto cfg2 = GetGridConfig();
		VisitAllCells([cfg2](GridCell&cell, CellAnchor a)
		{
			cell.Update(cfg2,camera,a);
		});
	}

	void			ReScaleComposition()
	{
		RecomputeCellBoundsIfNecessary();
		auto cfg = GetGridConfig(1);
		float newFc = 1;
		for (int y = 0; y < 2; y++)
		{
			const CellAnchor left = (y ? Anchor::Top : 0);
			const CellAnchor right = (y ? Anchor::Top : 0) | Anchor::Right;

			Region&reg0 = region[left], &reg1 = region[right];

			const M::TIntRange<> range = reg0.yRanges.Intersect(reg1.yRanges);
			for (int i = range.start; i < range.end; i++)
			{
				auto*r0 = reg0.rowRanges.QueryPointer(i);
				auto*r1 = reg1.rowRanges.QueryPointer(i);
			
				if (!r0 || !r1)
					continue;

				const float overlap = r0->max - r1->min;
				if (overlap <= 0)
					continue;


				const float scaleOrigin0 = GetLeftCellOffset(cfg,i);
				const float scaleOrigin1 = GetRightCellOffset(cfg,i,!y);
				const float ext0 = r0->max - scaleOrigin0;
				const float ext1 = scaleOrigin1 - r1->min;
				if (ext0 < 0 || ext1 < 0 || ext0 + ext1 == 0)
					continue;
				const float space = scaleOrigin1 - scaleOrigin0;

				//scaleOrigin0 + ext0*fc = scaleOrigin1 - ext1 * fc
				//fc * (ext0 + ext1) = scaleOrigin1 - scaleOrigin0 = space
				//fc = space / (ext0 + ext1)

				const float fc = space / (ext0 + ext1);
				newFc = M::Min(newFc,fc);
			}
		}
		if (newFc == hexGridScale)
			return;
		hexGridScale = newFc;
		UpdateAllCells();
	}


	GridCell*		AllocateCell(const Objects::Command::PVisualCommand&cmd, const int2&coordinates, CellAnchor a)
	{
		bool isNew;
		return AllocateCell(cmd,coordinates,a, isNew);
	}

	GridCell*		AllocateCell(const Objects::Command::PVisualCommand&cmd, const int2&coordinates, CellAnchor a, bool&isNew)
	{
		DBG_ASSERT__(showControlsAndContacts);
		return region[a].AllocateCell(cmd,coordinates,a,isNew);
	}

	void			FindAndDeleteCell(const Objects::Command::PVisualCommand&cmd)
	{
		if (!cmd)
			return;
		for (CellAnchor a = 0; a < Anchor::Count; a++)
			foreach (region[a].allGuiCells,c)
				if (c->command == cmd)
				{
					region[a].allGuiCells.erase(c);
					//allocatedEntityCommands.FindAndErase(cmd);
					cellCompositionChanged = true;
					return;
				}

	}

/*

	void	DeleteAllCellsOfType(GridCell::type_t t)
	{
		for (index_t i = 0; i < allGuiCells.Count(); i++)
			if (allGuiCells[i].type == t)
			{
				allGuiCells.Erase(i);
				i--;
			}
	}
*/


	DX11::BaseInstructionBlock	instruction;
	DX11::ObjectBuffer<float4>	center,visualCodeRGB_CellSize;
	Engine::OrthographicAspect<> camera;
	//Buffer0<Group>	friendlyGroups,hostileGroups;



	void						Configure(const Objects::PWorldEntity&e);

	void						Create()
	{
		GlobalEvents::uiScaleChanged += OnScreenResize;	//mostly the same effect

		center.Create(float4());
		visualCodeRGB_CellSize.Create(float4(1));

		instruction.quickSlots.SetSize(1);

		DX11::Shader::Binding bindings[] =
		{
			DX11::Shader::Binding(0,"Texture"),
			DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler"),
			DX11::Shader::Binding(center,"Center"),
			DX11::Shader::Binding(visualCodeRGB_CellSize,"VisualCodeRGB_CellSize"),
		};

		instruction.shader.LoadFromFile("shaders/hexUI.hlsl","vsMain", "gsMain", "psMain",bindings, ARRAYSIZE(bindings));
		instruction.UpdateLayout();
		instruction.SetPoints();

		camera.UpdateProjection(-1,-1,-1+2.f * display.GetPixelAspect(),1,-1,1);
		camera.depthTest = Engine::NoDepthTest;

		friendlyGroups.Create();
		hostileGroups.Create();
		animationTimeLine.Create();

		BottomRightPanel::Create();

		Configure(Objects::selectedEntity);	//very likely not set

	}


	bool		hasFocusButton = false;


	void						OnScreenResize()
	{
		//1.f / display.GetPixelAspect();
		camera.UpdateProjection(-1,-1,-1+2.f * display.GetPixelAspect(),1,-1,1);
		UpdateAllCells();
		RefreshContacts();
		cellCompositionChanged = true; 
		ReScaleComposition();
	}


	void						Destroy()
	{
		center.Clear();
		visualCodeRGB_CellSize.Clear();
		instruction.Clear();
		ClearCommands();
		//groupFocusCommands.Clear();
		//vehicleCommands.Clear();
		//structureCommands.Clear();

		BottomRightPanel::Destroy();

		friendlyGroups.Destroy();
		hostileGroups.Destroy();
		animationTimeLine.Destroy();

		selectedFocus.reset();
	}

	bool	HandleClick()
	{
		auto found = ScanAllCells([](const GridCell&cell, CellAnchor)->bool
		{
			auto c = &cell;
			if (c->command->texture.IsNotEmpty())
			{
				if (c->Contains(Engine::mouse.location.windowRelative))
				{
					if (c->command->state & Objects::CommandFlags::IsInvisible)
						return false;

					if (c->command->state & Objects::CommandFlags::IsEnabled)
					{
						String name = c->command->name;
						c->command->Execute();
						//c is not guaranteed to be valid past this point
						Objects::UpdateChoices(Network::ControlEnabled());
						Tutorial::ShowMessage(name);
						Configure(Objects::selectedEntity);
					}
					return true;
				}
			}
			return false;
		});
		return found != nullptr;
	}

	GridCell*		hoveringOver = nullptr;
	CellAnchor		hoveringAnchor = 0;
	Objects::WrappedDescription	hoveringText;
	float			hoveringWidth = 0;
	bool			hoveringBrief = true;

	/*
		Renders in viewport -1 to +1
	*/
	void						RenderCommands(const TGridConfig&cfg)
	{
		//static int2 lastMouse;
		float flashOpacity = 	((UINT64)floor(Engine::timing.now*2.0))%2;
		float softFlashOpacity = float(sin(Engine::timing.now*2.0*M_PI)*0.5+0.5);


		static float stationaryCounter = 0;
		static int2 refPos;

		if (!Vec::similar(refPos, Engine::mouse.location.absolute,10))
		{
			stationaryCounter = 0;
			refPos = Engine::mouse.location.absolute;
		}
		else
			stationaryCounter += Engine::timing.delta;

		bool stationary = stationaryCounter > 0.25f;
		bool extStationary = stationaryCounter > 1.5f;

		camera.UpdateProjection2(-1,-1,-1+2.f * display.GetPixelAspect(),1,-1,1);
		display.Pick(camera);

		display.SetRasterizer(true,false);
		display.SetDepthTest(false,false);
		display.SetBlendMode(Engine::BlendMode::AlphaBlend);

		GridCell*lastHovering = hoveringOver;
		hoveringOver = nullptr;
		//Array<String>	hoveringText;

		VisitAllCells([cfg,stationary,softFlashOpacity,flashOpacity]( GridCell&cell,CellAnchor a)
		{
			auto c = &cell;
			if (c->command->texture.IsNotEmpty())
			{
				float2 world = c->ToFloatCoords(cfg,a);
				float3 at;
				camera.Project(float3(world,0),at);
				at.xy = at.xy*0.5f + 0.5f;
				auto&cmd = *c->command;

				float4 code;
				if (cmd.state & Objects::CommandFlags::IsInvisible)
					return;
				{
					if (!WindowStack::IsModal() && c->Contains(Engine::mouse.location.windowRelative))
					{
						code.r = 1.f;
						if (stationary)
						{
							hoveringOver = c;
							hoveringAnchor = a;
						}
					}
					else
						if (cmd.state & (Objects::CommandFlags::IsFlashing | Objects::CommandFlags::SoftFlashing))
						{
							code.r = (cmd.state & Objects::CommandFlags::SoftFlashing) ? softFlashOpacity : flashOpacity;
						}
						
					if (cmd.state & Objects::CommandFlags::IsPermaActive)
						code.y = 1.f;
				}

				if (!(cmd.state & Objects::CommandFlags::IsEnabled))
				{
				//	code.xy = float2(0);
					code.z = 0.5f;
				}
				code.w = cfg.cellSize;
					//color.Update(float4(1,1,1,0.5));
				visualCodeRGB_CellSize.Update(code);

				center.Update(float4(world,0,1));
				instruction.quickSlots.First() = c->command->texture;
				display.RenderVertices(instruction,0,1);
			}
			else
				FATAL__("command texture is empty: "+c->command->name);

		});

		if (lastHovering != hoveringOver)
		{
			lastHovering = hoveringOver;
			Objects::Description desc;
			if (hoveringOver)
				hoveringOver->command->CompileLabel(desc);
			hoveringText = desc;
			hoveringWidth = 0;
			hoveringBrief = true;
		}
		else
		{	bool wasBefore = hoveringBrief;
			hoveringBrief = !extStationary;
			if (wasBefore != hoveringBrief)
				hoveringWidth = 0;
		}
	}

	void	RenderHint()
	{
		if (hoveringOver)
		{
			//tooltips
			display.Pick(ResourceGauges::textoutCamera);

			textout.PushState();
			float2 base = Engine::mouse.location.windowRelative * 2.f - 1.f;
			textout.Scale(2.5f,2.5f);

			SurfaceRenderer::SetColor(float4(0,0,0,0.9f));
			if (hoveringWidth == 0)
			{
				if (hoveringBrief)
				{
					foreach (hoveringText.wrappedLines,t)
						if (t->second)
							hoveringWidth = std::max(hoveringWidth, textout.QueryScaledWidth(t->first));
				}
				else
					foreach (hoveringText.wrappedLines,t)
						hoveringWidth = std::max(hoveringWidth, textout.QueryScaledWidth(t->first));
			}
			
			count_t lineCount = hoveringBrief ? hoveringText.CountBriefLines() : hoveringText.wrappedLines.Count();
			float h = textout.GetScaledHeight()*(lineCount-1);
			M::Rect<> r;

			const bool flipX = (hoveringAnchor & Anchor::Right) != 0;
			const bool flipY = (hoveringAnchor & Anchor::Top) != 0;

			if (flipX)
			{
				if (flipY)
				{
					textout.MoveTo(base - float2(hoveringWidth,textout.GetScaledHeight()));
					r = M::Rect<>(base -float2(hoveringWidth,textout.GetScaledHeight() + h+textout.GetScaledHeight()),base + float2(0,-textout.GetScaledHeight()*0.8f));
				}
				else
				{
					textout.MoveTo(base + float2(-hoveringWidth,h + textout.GetScaledHeight()));
					r = M::Rect<>(base-float2(hoveringWidth,0) ,base + float2(0,h + textout.GetScaledHeight()*1.2f));
				}
			}
			else
			{
				base.x = std::min(base.x,1.f - hoveringWidth);

				if (flipY)
				{
					textout.MoveTo(base - float2(0,textout.GetScaledHeight()));
					r = M::Rect<>(base -float2(0,textout.GetScaledHeight() + h+textout.GetScaledHeight()),base + float2(hoveringWidth,-textout.GetScaledHeight()*0.8f));
				}
				else
				{
					textout.MoveTo(base + float2(0,h + textout.GetScaledHeight()));
					r = M::Rect<>(base ,base + float2(hoveringWidth,h + textout.GetScaledHeight()*1.2f));
				}
			}

			//r.x.min = r.x.min * 0.5f + 0.5f;
			//r.x.max = r.x.max * 0.5f + 0.5f;
			//r.y.min = r.y.min * 0.5f + 0.5f;
			//r.y.max = r.y.max * 0.5f + 0.5f;
			//M::Rect<>r(0,0,1,1);

			SurfaceRenderer::Rasterize(r);
			SurfaceRenderer::Flush();

			textout.SetColor(1,1,1);
			foreach (hoveringText.wrappedLines,l)
				if (!hoveringBrief || l->second)
					textout << l->first << nl;
			textout.PopState();
		}





		//static Engine::Aspect<> aspect;
		//aspect.depthTest = Engine::VisualEnum::NoDepthTest;
		//display.Pick(aspect);
		//SurfaceRenderer::SetColor(float4(1,0,0,0.4));
		//foreach (activeCells,c)
		//{
		//	if (c->command->texture.IsNotEmpty())
		//	{
		//		SurfaceRenderer::Rasterize(c->projectedRect);
		//	}
		//}
		//SurfaceRenderer::Flush();

	}

	//Buffer0<Objects::Command::PVisualCommand>	allocatedEntityCommands;
		


	void		AddCommand(const int2&root, const int2&location,const Objects::Command::PBase&cmd)
	{
		auto vcmd = To<Objects::Command::VisualCommand>::DynamicCast( cmd);
		if (!vcmd)
			return;
		bool isNew;
		const CellAnchor a = Anchor::BottomLeft;
		
		const auto loc = location;
		{
			GridCell*cell = AllocateCell(vcmd,loc,a,isNew);
			if (cell == nullptr)
				return;
		}
		if (cmd->ToContainer())
		{
			int2 childOffset = loc;
			childOffset.y ++;
			#if 0
				childOffset.x+=childOffset.y %2;
			#else
				childOffset.x = root.x + (childOffset.y-root.y)/2;// + childOffset.y %2;
			#endif

			auto group = To<Objects::Command::CommandGroup>::Cast(cmd);
			if (group->IsExpanded())
				AddCommandGroup(root, childOffset,*group);
		}
		cmd->Refresh(Network::ControlEnabled());

	}

	void		AddCommandGroup(const int2&root, const int2&childOffset, const Objects::Command::Container&container)
	{
		for (index_t r = 0; r < container.CountRows(); r++)
		{
			const auto&row = container.GetRow(r);
			const int2 myOffset = childOffset+int2(r/2,r);

			const count_t rightLimit = myOffset.y <= 1 ? 16 : 20;
			count_t space = 4;
			for (; space < row.Count() && !region[Anchor::BottomLeft].HasCell(myOffset+int2(space+1,0)) && space + childOffset.x < rightLimit; space++)
			{}

			if (space >= row.Count())
			{
				for (index_t i = 0; i < row.Count(); i++)
				{
					auto cmd = row[i];
					if (!cmd)
						continue;
					AddCommand(root,myOffset+int2(i,0),cmd);
				}
			}
			else
			{
				ScrollArea&s = region[Anchor::BottomLeft].GetScrollArea(myOffset,space, row.Count());
				s.AddCommands(root,row);
			}
		}
	}

	void		AddCommandGroup(const int2&childOffset, const Objects::Command::Container&container)
	{
		AddCommandGroup(childOffset, childOffset,container);
	}

	void		AddCommand(const int2&location,const Objects::Command::PBase&cmd)
	{
		AddCommand(location,location,cmd);
	}

	void RefreshButtonState()
	{
		VisitAllCells([](GridCell&cell, CellAnchor)
		{
			cell.command->Refresh(Network::ControlEnabled());
		});
	}

	static void CollectionToCommands(const TGridConfig&cfg, CollectionTemplateRow<Objects::WWorldEntity>&context)
	{
		context.commands.Clear();
		context.collection.VisitAllValues([&context](Buffer0<Objects::WWorldEntity>&entries)
		{
			Objects::PWorldEntity e = entries.First().lock();
			if (e)
			{
				PGroup group (new Group());
				group->name = e->GetTypeName();
				group->entries.adoptData(entries);
				group->UpdateSignificance();

				context.commands << Objects::Command::PVisualCommand(new Custom::GroupFocusCommand(group));
			}
		});
		context.collection.Clear();
		Sorting::ByComparator::QuickSort(context.commands,Custom::CollectionCommandCompare);
		context.Refresh();
	}


	void					RefreshContacts()
	{
		if (!showControlsAndContacts)
			return;
		auto cfg = GetGridConfig();
		//this is off:
		//currentContactListWidth = std::max(1, int((display.GetPixelAspect()-0.67f)*15));
		const float viewPortRange = 2.f * display.GetPixelAspect();
		const float screenRelativeRightPanel = 300.f / display.GetClientWidth();
		const float inViewPortRange = screenRelativeRightPanel * viewPortRange;
		const int hexOffset = 5;
		const float leftOffset = cfg.stepX *2 * hexOffset + float(1 - (hexOffset % 2)) * cfg.stepX;
		const float remainder = viewPortRange - inViewPortRange - leftOffset;
		const int iconRange = std::max(3,(int)(remainder / cfg.stepX / 2));
		currentContactListWidth = iconRange-2;


		//camera.UpdateProjection2(-1,-1,-1+2.f * display.GetPixelAspect(),1,-1,1);
		float len = 300.f / display.GetClientWidth();
		textout.MoveTo(1.f - len,0.95f);



		//ClearContacts();

		{
			hostileGroups.collection.Clear();
			friendlyGroups.collection.Clear();


			Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
			{
				String key = e->GetTypeName() + "|";
				if (e->owner)
					key += e->owner->id.ToFullString();
				if (e->IsDeadOrGhost())
					key += "|ghost";
				#ifndef NO_HIBERNATION
					if (e->ToHibernating()->IsHibernating())
						key += "|hibernating";
				#endif

				if (e->isOwnedByMe)
				{
					friendlyGroups.collection.Set(key).Append(e);
				}
				else
					hostileGroups.collection.Set(key).Append(e);
			},true);
			CollectionToCommands(cfg,friendlyGroups);
			CollectionToCommands(cfg,hostileGroups);
		}
			


		#ifdef HAS_WINDOW
			if (hostileGroups.commands.IsNotEmpty())
				Tutorial::ShowMessage("Hostile Contacts");
		#endif

		animationTimeLine.Refresh();

		BottomRightPanel::Refresh();
		//display.SetDepthTest(false,false);
		//display.SetRasterizer(true,false);
	}

	void					ClearCommands()
	{
		for (CellAnchor a = 0; a < Anchor::Count; a++)
		{
			region[a].allGuiCells.Clear();
		}
		//allocatedEntityCommands.Clear();
		cellCompositionChanged = true;
	}

	void					Configure(const Objects::PWorldEntity&e)
	{
		if (ConfigurationMode::ReconfigureVisualCommands())	//configuration mode has its own buttons; should not be removed while active
			return;
		const auto cfg = GetGridConfig();
		ClearCommands();

		bool showControls = showControlsAndContacts && e
				#ifndef DEBUG_ACCESS_RESTRICTIONS
					&& e->isOwnedByMe && e->IsAlive()
				#endif
				;

//			visitCfg = cfg;

		if (e)
		{
			hasFocusButton = true;
			if (showControls)
			{
				Objects::Command::Container temp;
				auto cmds = e->GetCommands(temp);
				if (cmds)
					AddCommandGroup(EntityCommandOffset,*cmds);
			}

			FindAndDeleteCell(selectedFocus);
			selectedFocus.reset(new Custom::EntityFocusCommand(e) );
			selectedFocus->Refresh(true);//Network::ControlEnabled());
			selectedFocus->UpdateTexture(e,false,e->isOwnedByMe ? friendlyBackgroundColor : hostileBackgroundColor);

			AllocateCell(selectedFocus,GetFocusLocation(),Anchor::TopRight);
		}
		else
		{
			hasFocusButton = false;
			FindAndDeleteCell(selectedFocus);
			selectedFocus.reset();
		}

		//put this after any entity commands, in case they overlap
		if (showControlsAndContacts)
			AddCommandGroup(GlobalCommandOffset,Objects::globalCommands);


		if (showControlsAndContacts)
			RefreshContacts();
		else
		{
			friendlyGroups.RemoveIcons();
			hostileGroups.RemoveIcons();
			animationTimeLine.RemoveIcons();
		}
		ReScaleComposition();
	}





	

	namespace EventLog
	{

		void			Clear()
		{
			animationTimeLine.commands.Clear();
			animationTimeLine.Refresh();
		}

		void			RegisterAttack(float time, bool isFriendly,  const M::TVec3<>&focusLocation, const TUniqueID&focusEntityID, const Objects::Renderer::SmokeTrailIndex&smokeTrailIndex)
		{
			Custom::AnimationEventFocusCommand*cmd = new Custom::AnimationEventFocusCommand("Attack","Attack",focusLocation,focusEntityID,time,isFriendly,!isFriendly);
			cmd->smokeTrailIndex = smokeTrailIndex;
			if (!isFriendly)
				cmd->state |= Objects::CommandFlags::IsFlashing;
			animationTimeLine.commands << Objects::Command::PVisualCommand(cmd);
			animationTimeLine.Refresh();
		}

		void			RegisterEntityDestruction(float time, bool isOwnedByMe,  const M::TVec3<>&focusLocation, const TUniqueID&focusEntityID, const String&entityName)
		{
			Custom::AnimationEventFocusCommand*cmd = new Custom::AnimationEventFocusCommand("Entity Death","Entity Death",focusLocation,focusEntityID,time,isOwnedByMe,isOwnedByMe);
			if (isOwnedByMe)	//flipped logic
				cmd->state |= Objects::CommandFlags::IsFlashing;
			cmd->fallbackTargetName = entityName;
			animationTimeLine.commands << Objects::Command::PVisualCommand(cmd);
			animationTimeLine.Refresh();
		}

		bool			HasRegisteredHostileActivity()
		{
			foreach (animationTimeLine.commands,cmd)
			{
				std::shared_ptr<Custom::AnimationEventFocusCommand> aec = To<Custom::AnimationEventFocusCommand>::Cast(*cmd);
				if (aec->isHostileActivity)
					return true;
			}
			return false;
		}


	}	
	
}
}
