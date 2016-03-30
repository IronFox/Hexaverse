#include <global_root.h>
#include "ui.h"
#include "dialog.h"
#include "network.h"
#include "tutorial.h"


#ifdef HAS_WINDOW
	#include "../DX11/texture_font.h"
	#include "surfaceRenderer.h"

	extern Engine::Display<Engine::DirectX11>	display;
	extern Engine::Textout<Engine::DX11::TextureFont>	textout;

	namespace UI 
	{

		namespace DX11 = Engine::DX11; //!< Shortcut

		bool showControlsAndContacts = true;


		void	ShowSession()
		{
			if (Dialog::IsModal())
			{
				if (Dialog::ShowingFactions())
					Dialog::CloseAllModal();
				return;
			}

			Buffer0<Dialog::Faction>	factions;
			for (const auto&factionIn : Objects::Faction::allKnown)
			//Network::factionMap.VisitAllValues([&factions](const Objects::OwnerParameter&factionIn)
			{
				Dialog::Faction&factionOut = factions.Append();
				factionOut.name = factionIn->GetName();
				factionOut.online = factionIn->online;
				factionOut.requestedNewRound = factionIn->requestedNewRound;
				factionOut.factionReference = factionIn.get();
			};
			//);

			Dialog::ShowFactions(factions);
		}


		void	RefreshSessionView()
		{
			if (Dialog::ShowingFactions())
			{
				Dialog::CloseAllModal();
				ShowSession();
			}

		}



		void	IconItem::LoadTexture(const String&name)
		{
			Image image;
			Magic::LoadFromFile(image,PathString("resources/icons/"+name+".png"));
			texture.Load(image);
		}
		
		void IconItem::RenderIcon(float size/*=0.05f*/)
		{
			iconColor.Update(float4(color*0.75f + 0.25f,size));
			iconShader.quickSlots.first() = texture;
			display.RenderVertices(iconShader,0,1);
		}




		void	Gauge::Set(int current, int max, int current2, int max2, int changePerRound, bool enabled)
		{
			if (current == this->current && max == this->max 
				&&
				current2 == this->current2 && max2 == this->max2
				&& changePerRound == this->changePerRound
				&& enabled == this->enabled)
				return;
			top0 = std::max(current,current+changePerRound);
			top1 = std::max(max,top0);
			top = std::max(abs(current), top1);
			this->current = current;
			this->max = max;
			this->current2 = current2;
			this->max2 = max2;
			this->changePerRound = changePerRound;
			this->enabled = enabled;
			ftop = std::max(1,top);
			hScale = height / float(ftop);
			#ifndef NO_HIBERNATION
				if (isTraffic)
				{
					hScale *= 1.5f;
					float toRed = std::min(float(current+changePerRound)/float(max),1.f);
					Vec::HSL2RGB(float3(0.5f - 0.5f * toRed,toRed*0.5f,1.f - toRed*0.5f),color);
				}
			#endif
			changed = true;
		}



		/*static*/ void Gauge::StaticCreate()
		{
			shader.shader.LoadFromFile("shaders/gauge.hlsl","vsMain", "psMain");
			shader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
			shader.binding.Append("NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT);
			shader.binding.Append("COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			shader.UpdateLayout();
			shader.SetTriangles();

			//iconShader.shader.LoadFromFile("shaders/gaugeIcon.hlsl","vsMain", "psMain");
			//
			//iconShader.binding.Append("POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT);
			//shader.binding.Append("NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT);
			//shader.binding.Append("COLOR",0,DXGI_FORMAT_R32G32B32A32_FLOAT);
			//shader.UpdateLayout();
			//shader.SetTriangles();

			//iconCenter.Create(float4(0));
			iconColor.Create(float4(0));

			iconShader.quickSlots.SetSize(1);

			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(0,"Texture"),
				DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler"),
//				DX11::Shader::Binding(iconCenter,"Center"),
				DX11::Shader::Binding(iconColor,"Color"),
			};

			count_t numSet = DX11::Shader::macroDefinitions.Count();
			//DX11::Shader::RedefineMacro(DX11::Shader::Macro("ICON_SIZE",0.1f));
			iconShader.shader.LoadFromFile("shaders/gaugeIcon.hlsl","vsMain", "gsMain", "psMain",bindings, ARRAYSIZE(bindings));

			DX11::Shader::macroDefinitions.Truncate(numSet);
			iconShader.UpdateLayout();
			iconShader.SetPoints();

		}

		/*static*/ void Gauge::StaticDestroy()
		{
			shader.Clear();
			iconShader.Clear();
			iconColor.Clear();
			//foreach (gauges,g)
			//	g->texture.Clear();

		}


		void	Gauge::UpdateTextProjection(const Engine::Aspect<>&camera)
		{
			//camera.Project(base + float3(0,0,float(top) / ftop * height * 1.1f),projectedTop);
			camera.ProjectToRegion(base + float3(-0.0125f * log(std::max(1,current))/log(10.f),0.f,0.f) ,projectedBottom);
			//projectedBottom.x -= 0.0125f * log(std::max(1,current))/log(10.f);
			camera.ProjectToRegion(base + float3(0,0,float(current + changePerRound) * hScale),projectedDelta);
			camera.ProjectToRegion(base + float3(0,0,float(max) * hScale),projectedMax);
			camera.ProjectToRegion(base + float3(0,0,float(std::max(max, current+changePerRound)) * hScale),projectedTop);
		}

		void	Gauge::Render(BYTE target)
		{
			if (changed)
			{
				Construct();
			}
			TMatrix4<> system = Matrix<>::eye4;
			system.w.xyz = base;
			system.z.xyz *= hScale;
			#ifndef NO_HIBERNATION
				if (isTraffic)
				{
					system.x.xyz *= 0.75f;
					system.y.xyz *= 0.75f;
				}
			#endif

			system.x.xyz *= 1.5f;
			system.y.xyz *= 1.5f;

			display.SetObjectMatrix(system);

			display.Render(instruction[target]);


			//display.SetObjectMatrix()
			//if (!target)
			//{
			//	textout.pushState();
			//	textout.setScale(0.2,0.2);
			//	display.SetRasterizer(true,false);
			//	textout.locate(0,0,1);
			//	textout.color(1,1,1);
			//	textout << "test";
			//	textout.popState();

			//}

		}


		static void Write(int value)
		{
			if (value < 0)
			{
				textout << '-';
				Write(-value);
				return;
			}
			if (value < 2000)
			{
				textout << value;
				return;
			}
			if (value < 2000000)
			{
				int v = (int)round(float(value)/1000.f);
				textout << v << 'k';
				return;
			}
			int v = (int)round(float(value)/1000000.f);
			textout << v << 'm';
			return;
		}

		void	Gauge::RenderListText(int y)
		{
			textout.pushState();
			//textout.locate(projectedTop);
			textout.locate(-1.f + 0.01f,-1.f + textout.getScaledHeight()*1.2f*(y+5));

			//textout.SetLine(-1);
			textout << myTag;
			
			
			textout.color(color * 0.5f + float3(0.5f));
			if (changePerRound > 0)
			{
				textout << "+";
				Write(changePerRound);
			}
			else
				if (changePerRound < 0)
				{
					textout.color(1,0.6,0.5);
					Write(changePerRound);
					textout.color(color * 0.5f + float3(0.5f));
				}
			if (max > 0 || max2 > 0)
				textout.scale(1.2f,1.2f);
			if (max > 0)
			{
				textout << ' ';
				Write(current);
				textout.scale(0.6f,0.6f);
				textout <<'/';
				Write(max);
				textout.scale(1.f/0.6f,1.f/0.6f);
			}
			if (max2 > 0)
			{
				textout << " +";
				Write(current2);
				textout.scale(0.6f,0.6f);
				textout <<'/';
				Write(max2);
				textout.scale(1.f/0.6f,1.f/0.6f);
			}
			//textout << max;
			textout.popState();


		}

		void	Gauge::RenderText()
		{
			//textout.setScale(0.2,0.2);	//leave it
			//display.SetRasterizer(true,false);
			{
				textout.pushState();
				textout.color(color * 0.5f + float3(0.5f));
				textout.locate(projectedBottom);
				textout.SetLine(-1);
				textout << current << nl;
				textout.popState();
			}
			if (max)
			{
				textout.pushState();
				textout.locate(projectedMax);
				textout.scale(0.6f,0.6f);
				textout.SetLine(-1);
				textout << max;
				textout.popState();
			}

			if (changePerRound != 0)
			{
				textout.pushState();
				textout.locate(projectedDelta.xy);
				textout.scale(0.8f,0.8f);
				textout.SetLine(-1);
				if (changePerRound > 0)
				{
					textout.color(color);
					textout << '+';
				}
				else
					textout.color(1,0.6,0.5);
				textout << changePerRound;
				textout.popState();
			}
			
			{
				textout.pushState();
				//textout.locate(projectedTop);
				textout.scale(0.6f,0.6f);
				textout.locate(projectedTop.x,projectedTop.y + textout.getScaledHeight()*2.5f);

				//textout.SetLine(-1);
				textout << myTag;
				//textout << max;
				textout.popState();
			}
			
			//RenderIcon(projectedTop);
		}


		void	Gauge::ConstructPart(float radius, float lower, float upper, const float4&lowerColor, const float4&upperColor, Objects::Geometry&geo)
		{
			for (int i = 0; i < 6; i++)
			{
				float a = float(i)/6.f * 2.f * M_PIF;
				float b = float(i+1)/6.f * 2.f * M_PIF;
				float ab = (a + b)/2.f;

				geo.SetVertexOffsetToCurrent();

				float3 normal(cos(ab),-sin(ab),0);
				float2 av(cos(a),-sin(a));
				av *= radius;
				float2 bv(cos(b),-sin(b));
				bv *= radius;

				float3	na = float3(bv,0),//  normal,
						nb = float3(av,0);//	normal;

				geo.MakeVertex(float3(av,lower),na,lowerColor);
				geo.MakeVertex(float3(bv,lower),nb,lowerColor);
				geo.MakeVertex(float3(bv,upper),nb,upperColor);
				geo.MakeVertex(float3(av,upper),na,upperColor);

				geo.MakeQuadInv(0,1,2,3);
			}
			
			geo.SetVertexOffsetToCurrent();

			for (int i = 0; i < 6; i++)
			{
				float a = float(i)/6.f * 2.f * M_PIF;
				geo.MakeVertex(float3(cos(a) * radius,-sin(a) * radius,upper),float3(0,0,1),upperColor);
			}
			for (int i = 2; i < 6; i++)
				geo.MakeTriangleInv(0,i-1,i);
		}

		void	Gauge::Construct()
		{
			changed = false;

			geo[0].Clear();
			geo[1].Clear();
			for (int i = 0; i < 6; i++)
			{
				float a = float(i)/6.f * 2.f * M_PIF;
				geo[0].MakeVertex(float3(cos(a),-sin(a),0),float3(0,0,-1),color);
			}
			for (int i = 2; i < 6; i++)
				geo[0].MakeTriangle(0,i-1,i);

			//float cap = std::min(max,current + std::max(0.f,changePerRound));
			//float h = cap / max * height;

			if (changePerRound >= 0)
			{
				//if (localCurrent > 0)
				//{
				//	ConstructPart(1.f,0,localCurrent,float4(1,0,0,0.95f),geo[0]);
				//	ConstructPart(1.f,localCurrent,current,float4(color,0.95f),geo[0]);
				//}
				//else
					ConstructPart(1.f,0,current,float4(color,0.95f),geo[0]);
				if (changePerRound > 0)
				{
					int top = current + std::max(0,changePerRound /*- (enabled ? localChange : 0)*/);
					//int top1 = std::min(max,top);
					ConstructPart(0.8f,current,top,float4(color,0.f),float4(color,0.6f),geo[0]);

					//if (top != top1)
						//ConstructPart(0.8f,top1,top,float4(color,0.f),float4(color,0.6f),geo[0]);
				}
			}
			else
			{
				//if (localCurrent > 0)
				//{
				//	int step = std::min(localCurrent,std::max(0,(current+changePerRound)));
				//	ConstructPart(1.f,0,step,float4(1,0,0,0.95f),geo[0]);

				//	ConstructPart(1.f,step,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
				//}
				//else
				{
					ConstructPart(1.f,0,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
				}
				ConstructPart(1.f,std::max(0,(current+changePerRound)),current,float4((color + float3(1,0.5,0.5))/1.5f,0.95f),geo[0]);
			}

			//if (localChange != 0)
			//{
			//	float t = current+changePerRound;
			//	if (localChange > 0)
			//	{
			//		if (enabled)
			//			ConstructPart(0.6f,t- localChange,t,float4(1,0,0,0.f),float4(1,0,0,0.6f),geo[0]);
			//		else
			//			ConstructPart(0.6f,t,t + localChange,float4(1,0.5f,0,0.f),float4(1,0.5f,0,0.6f),geo[0]);
			//	}
			//	else
			//		if (enabled)
			//			ConstructPart(0.6f,t,t-localChange,float4(1,0,0,0.6f),float4(1,0,0,0.f),geo[0]);
			//		else
			//			ConstructPart(0.6f,t+localChange,t,float4(1,0.5f,0,0.6f),float4(1,0.5f,0,0.f),geo[0]);
			//		//ConstructPart(0.8f,current,top1,float4(color,0.f),float4(color,0.6f),geo[0]);
			//}

			ConstructPart(1.f,0,max,float4(float3(1),0.25),geo[1]);




			for (BYTE k = 0; k < 2; k++)
			{
				instruction[k].SetBase(shader);
				instruction[k].vbo.CreateFromStructs(geo[k].GetLOD(0).vertices);
				instruction[k].ibo.Create(geo[k].GetLOD(0).indices);
			}
		};



		/*static*/ const float Gauge::height = 10.f;

		/*static*/ DX11::BaseInstructionBlock	Gauge::shader,IconItem::iconShader;
		/*static*/ DX11::ObjectBuffer<float4>	/*IconItem::iconCenter,*/IconItem::iconColor;

		Array<Gauge>	gauges;
		#ifndef NO_HIBERNATION
			Gauge		trafficGauge;
		#endif
		IconItem		timeResource;

		Engine::VectorCamera<>	barProjectionCamera;
		Engine::OrthographicAspect<> textoutCamera;
		bool			resourcesAsList = false;


		namespace EventLog
		{

			void			Clear()
			{
				HexGrid::animationTimeLine.commands.Clear();
				HexGrid::animationTimeLine.Refresh();
			}

			void			RegisterAttack(float time, bool isFriendly,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, index_t smokeTrailIndex)
			{
				HexGrid::AnimationEventFocusCommand*cmd = new HexGrid::AnimationEventFocusCommand("Attack","Attack",focusLocation,focusEntityID,time,isFriendly);
				cmd->smokeTrailIndex = smokeTrailIndex;
				if (!isFriendly)
					cmd->state |= Objects::BaseCommand::Flags::IsFlashing;
				HexGrid::animationTimeLine.commands << Objects::PBaseCommand(cmd);
				HexGrid::animationTimeLine.Refresh();
			}

			void			RegisterEntityDestruction(float time, bool isOwnedByMe,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, const String&entityName)
			{
				HexGrid::AnimationEventFocusCommand*cmd = new HexGrid::AnimationEventFocusCommand("Entity Death","Entity Death",focusLocation,focusEntityID,time,isOwnedByMe);
				if (isOwnedByMe)	//flipped logic
					cmd->state |= Objects::BaseCommand::Flags::IsFlashing;
				cmd->fallbackTargetName = entityName;
				HexGrid::animationTimeLine.commands << Objects::PBaseCommand(cmd);
				HexGrid::animationTimeLine.Refresh();
			}


		}




		namespace HexGrid
		{

			CollectionTemplateRow<Objects::WWorldEntity>			friendlyGroups(0),hostileGroups(1);
			DynamicRow												animationTimeLine(2);

			count_t					currentContactListWidth = 10;



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
				state = Objects::BaseCommand::Flags::IsEnabled;
			}

			/*virtual override*/ void				AnimationEventFocusCommand::CompileLabel(Label&outResult)
			{
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



			void	DynamicRow::Create()
			{
				leftCommand.reset(new ArrowCommand(true,this));
				leftCommand->gridCoords = DynamicRowOffset + int2(0,rowIndex);
				rightCommand.reset(new ArrowCommand(false,this));
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
					visibleRange = IntRange(index_t(0),scopeWidth);
					if (newOffset > 0)
						visibleRange = IntRange(newOffset,scopeWidth);
					moreRight = visibleRange.GetExtend() > currentContactListWidth;
					if (moreRight)
						visibleRange.end = visibleRange.start + currentContactListWidth;
					this->offset = newOffset;
				}
				if (offset > 0)
				{
					GridCell&cell = AllocateCell(leftCommand);
					cell.gridCoords = leftCommand->gridCoords;
					cell.flipY = true;
					cell.Update(camera);
				}
				else
					FindAndDeleteCell(leftCommand);
				if (moreRight)
				{
					GridCell&cell = AllocateCell(rightCommand);
					cell.gridCoords = DynamicRowOffset + int2(currentContactListWidth+1,rowIndex);
					cell.flipY = true;
					cell.Update(camera);
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
				for (index_t i = 0; i < visibleRange.GetExtend(); i++)
				{
					const Objects::PBaseCommand cmd = commands[i+visibleRange.start];
					GridCell&cell = AllocateCell(cmd);
					cell.gridCoords = DynamicRowOffset + int2(i+1,rowIndex);
					cell.flipY = true;
					cell.Update(camera);
					lastVisible << cmd;
				}
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
				Objects::PWorldEntity e = entries.first().lock();
				DBG_ASSERT__(e);
				significance = e->GetConstructionCost();



				Objects::PTurretBase b = e->GetTurretBase();
				if (b && b->turret->CanAttack())
				{
					significance *= 3.f;
				}
				if (e->IsGroundBoundVehicle())
					significance *= 10.f;
				if (e->health <= 0)
				{
					if (e->isOwnedByMe)
						significance *= 15.f / (1.f + sqr(e->health));
					else
						significance *= 0.8f;
				}
				UINT32 crc = CRC32::getChecksum(e->GetTypeName().c_str());
				Random random(crc);
				significance *= random.getFloat(0.95f,1.07f);
			}







			/*virtual override*/ void	EntityFocusCommand::CompileLabel(Label&outResult)
			{
				outResult << name;
				Objects::PWorldEntity target = this->reference.lock();
				if (!target)
					return;
				if (target->isOwnedByMe)
				{
					outResult << "Owner: me";
					target->AppendDeploymentCount(outResult, target->owner);
				}
				else
					outResult << "Owner: "+target->owner->GetName();
				if (target->health > 0)
				{
					if (target->IsIndestructible())
						outResult << " Indestructible";
					else
						outResult << String(" Health: " +String( target->health) + "/" + String(target->GetMaxHealth()));
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
					outResult << "Offensive ("+String(base->GetAttacksPerRound())+"x"+String(base->turret->damageLevel)+")";
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
				Objects::PWorldEntity e = group->entries.first().lock();
				if (!e)
					return;

				const float4&background = e->isOwnedByMe ? friendlyBackgroundColor : hostileBackgroundColor;
				Objects::EntityCommand::UpdateTexture(*this,e,false,background);
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

				Objects::PWorldEntity proto = group->GetPrototype();
				Objects::PWorldEntity instance = group->entries.first().lock();

				if (!instance)
					return;
					
				outResult << name;
				if (instance->isOwnedByMe)
				{
//					outResult << "Owner: me";
					if (instance->health > 0)
					{
						proto->AppendDeploymentCount(outResult, instance->owner);
					}
				}
				else
					outResult << "Owner: "+instance->owner->GetName();
				if (instance->health > 0)
				{
					//if (e->IsIndestructible())
					//	outResult << " Indestructible";
					//else
					//	outResult << String(" Health: " +String( e->health) + "/" + String(e->GetMaxHealth()));
				}
				else
				{
					outResult << " Ghost";
				}

				#ifndef NO_HIBERNATION
					if (instance->ToHibernating()->IsHibernating())
						outResult << " Hibernating";
				#endif


				//outResult << "Health: "+String(target->health)+"/"+String(target->GetMaxHealth());
				Objects::PTurretBase b = proto->GetTurretBase();
				if (b && b->turret->CanAttack())
					outResult << "Offensive ("+String(b->GetAttacksPerRound())+"x"+String(b->turret->damageLevel)+")";
//					if (target->empDamageLevel)
//					outResult << "EMP-Damage detected ("+String(target->empDamageLevel)+")";

				outResult.MarkBriefEnd();
				Objects::OwnerParameter old = proto->owner;
				proto->owner = instance->owner;	//crude, but either it's set already, or this is a global prototype in which case ownership don't matter
				proto->DescribeType(outResult,Objects::WorldEntity::DescIncludePrice /*| Objects::WorldEntity::DescIncludeUnitCount*/);
				proto->owner = old;

				//if (e->IsGroundBoundVehicle())
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


			//Buffer0<VehicleFocusCommand>		vehicleCommands;
			//Buffer0<StructureFocusCommand>		structureCommands;
			//Buffer0<GroupFocusCommand>			groupFocusCommands;
			std::shared_ptr<EntityFocusCommand>					selectedFocus;

			namespace BottomRightPanel
			{
				typedef std::shared_ptr<Icon>	PIcon;
				Buffer0<PIcon >				commands;

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

				static void Add(const int2&coords, const Objects::PBaseCommand&cmd)
				{
					GridCell&cell = AllocateCell(cmd);
					cell.gridCoords = coords;
					cell.flipX = true;
					cell.Update(camera);
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
			}

		}



		namespace HexGrid
		{


			void		GridCell::Update(const Engine::Aspect<>&aspect)
			{
				float2 center = ToFloatCoords();
				float2 topRight = center + float2(CellSize*0.9f),
						bottomLeft = center - float2(CellSize*0.9f);
				float3 topRightP,bottomLeftP;
				aspect.Project(float3(topRight,0),topRightP);
				aspect.Project(float3(bottomLeft,0),bottomLeftP);

				projectedRect.x.min = bottomLeftP.x * 0.5f + 0.5f;
				projectedRect.x.max = topRightP.x * 0.5f + 0.5f;
				projectedRect.y.min = bottomLeftP.y * 0.5f + 0.5f;
				projectedRect.y.max = topRightP.y * 0.5f + 0.5f;
			}
			float2		GridCell::ToFloatCoords() const
			{
				int2 cellCoords = gridCoords;
				if (!flipX && !flipY)
					cellCoords += int2(3,1);
				float x;
				if (flipX)
					x = (-1+2.f * display.pixelAspect()) - stepX *2 * cellCoords.x - float(1 - (cellCoords.y % 2)) * stepX;
				else
					x = -1.f + stepX *2 * cellCoords.x + float(1 - (cellCoords.y % 2)) * stepX;
				float2	rs(x, -1.f + stepY * cellCoords.y);
				if (flipY)
					rs.y = -rs.y;
				return rs;
			}

			/*static*/ void GridCell::Radial(float cellIndex, TVec2<>&rs)
			{
				float a = cellIndex / 6.0f * M_PIF * 2.0f;
				rs.x = -sin(a);
				rs.y = cos(a);
			}

			bool		GridCell::Contains(const TVec2<>&p) const
			{
				if (!projectedRect.Contains(p))
					return false;

				TVec2<> relative;
				projectedRect.Relativate(p,relative);
				relative = relative * 2.f - 1.f;
				TVec2<> prev;
				Radial(0,prev);
				for (int i = 0; i < 6; i++)
				{
					TVec2<> next;
					Radial((i+1)%6,next);
					TVec2<> change,sb;
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

			Buffer<GridCell>			allGuiCells;

			GridCell&		AllocateCell(const Objects::PBaseCommand&cmd)
			{
				foreach (allGuiCells,c)
					if (c->command == cmd)
						return *c;
				GridCell&c = allGuiCells.Append();
				c.command = cmd;
				return c;
			}

			void			FindAndDeleteCell(const Objects::PBaseCommand&cmd)
			{
				if (!cmd)
					return;
				foreach (allGuiCells,c)
					if (c->command == cmd)
					{
						allGuiCells.erase(c);
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
			DX11::ObjectBuffer<float4>	center,color;
			Engine::OrthographicAspect<> camera;
			//Buffer0<Group>	friendlyGroups,hostileGroups;



			void						Configure(const Objects::PWorldEntity&e);

			void						Create()
			{
				center.Create(float4());
				color.Create(float4(1));

				instruction.quickSlots.SetSize(1);

				DX11::Shader::Binding bindings[] =
				{
					DX11::Shader::Binding(0,"Texture"),
					DX11::Shader::SamplerBinding(true,Engine::TextureFilter::Trilinear,1,"Sampler"),
					DX11::Shader::Binding(center,"Center"),
					DX11::Shader::Binding(color,"VisualCode"),
				};

				count_t numSet = DX11::Shader::macroDefinitions.Count();
				DX11::Shader::RedefineMacro(DX11::Shader::Macro("CELL_SIZE",CellSize*0.9f));
				instruction.shader.LoadFromFile("shaders/hexUI.hlsl","vsMain", "gsMain", "psMain",bindings, ARRAYSIZE(bindings));

				DX11::Shader::macroDefinitions.Truncate(numSet);
				instruction.UpdateLayout();
				instruction.SetPoints();

				camera.UpdateProjection(-1,-1,-1+2.f * display.pixelAspect(),1,-1,1);
				camera.depthTest = Engine::NoDepthTest;

				friendlyGroups.Create();
				hostileGroups.Create();
				animationTimeLine.Create();

				BottomRightPanel::Create();

				Configure(Objects::selectedEntity);	//very likely not set

			}


			bool		hasFocusButton = false;


			void						OnWindowResize()
			{
				barProjectionCamera.region.x.max = 0.9f;
				//1.f / display.pixelAspect();

				camera.UpdateProjection(-1,-1,-1+2.f * display.pixelAspect(),1,-1,1);
				foreach (allGuiCells,cell)
					cell->Update(camera);
				RefreshContacts();
				//if (hasFocusButton)
				//{
				//	selectedFocus.gridCoords = GetFocusLocation();
				//}
			}

			void						Destroy()
			{
				center.Clear();
				color.Clear();
				instruction.Clear();
				allGuiCells.Clear();
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
				foreach (allGuiCells,c)
				{
					if (c->command->texture.IsNotEmpty())
					{
						if (c->Contains(Engine::mouse.location.windowRelative))
						{
							if (c->command->state & Objects::BaseCommand::Flags::IsEnabled)
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
				}
				return false;
			}

			GridCell*		hoveringOver = nullptr;
			Objects::BaseCommand::Label	hoveringText;
			float			hoveringWidth = 0;
			bool			hoveringBrief = true;

			/*
				Renders in viewport -1 to +1
			*/
			void						Render()
			{
				//static int2 lastMouse;
				float flashOpacity = //float(sin(Engine::timing.now*2.0*M_PI)*0.5+0.5);
										((UINT64)floor(Engine::timing.now*2.0))%2;


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

				camera.UpdateProjection2(-1,-1,-1+2.f * display.pixelAspect(),1,-1,1);
				display.pick(camera);

				display.SetRasterizer(true,false);
				display.SetDepthTest(false,false);
				display.SetBlendMode(Engine::BlendMode::AlphaBlend);

				GridCell*lastHovering = hoveringOver;
				hoveringOver = nullptr;
				//Array<String>	hoveringText;

				foreach (allGuiCells,c)
				{
					if (c->command->texture.IsNotEmpty())
					{
						float2 world = c->ToFloatCoords();
						float3 at;
						camera.Project(float3(world,0),at);
						at.xy = at.xy*0.5f + 0.5f;
						auto&cmd = *c->command;

						float4 code;
					
						{
							if (!Dialog::IsModal() && c->Contains(Engine::mouse.location.windowRelative))
							{
								code.r = 1.f;
								if (stationary)
								{
									hoveringOver = c;
							
								}
							}
							else
								if (cmd.state & Objects::BaseCommand::Flags::IsFlashing)
								{
									code.r = flashOpacity;
								}
							
							if (cmd.state & Objects::BaseCommand::Flags::IsPermaActive)
								code.y = 1.f;
						}

						if (!(cmd.state & Objects::BaseCommand::Flags::IsEnabled))
						{
						//	code.xy = float2(0);
							code.z = 0.5f;
						}
							//color.Update(float4(1,1,1,0.5));
						color.Update(code);

						center.Update(float4(world,0,1));
						instruction.quickSlots.first() = c->command->texture;
						display.RenderVertices(instruction,0,1);
					}
					else
						FATAL__("command texture is empty: "+c->command->name);

				}

				if (lastHovering != hoveringOver)
				{
					lastHovering = hoveringOver;
					hoveringText.Clear();
					if (hoveringOver)
						hoveringOver->command->CompileLabel(hoveringText);
					hoveringText.briefEndAt = std::min(hoveringText.briefEndAt,hoveringText.lines.Count());
					hoveringWidth = 0;
					hoveringBrief = true;
				}
				else
				{	bool wasBefore = hoveringBrief;
					hoveringBrief = !extStationary;
					if (wasBefore != hoveringBrief)
						hoveringWidth = 0;
				}

				if (hoveringOver)
				{
					//tooltips
					display.pick(UI::textoutCamera);

					textout.pushState();
					float2 base = Engine::mouse.location.windowRelative * 2.f - 1.f;
					textout.scale(2.5f,2.5f);

					SurfaceRenderer::SetColor(float4(0,0,0,0.9f));
					if (hoveringWidth == 0)
					{
						if (hoveringBrief)
						{
							for (index_t i = 0; i < hoveringText.briefEndAt; i++)
								hoveringWidth = std::max(hoveringWidth, textout.getScaledWidth(hoveringText.lines[i]));
						}
						else
							foreach (hoveringText.lines,t)
								hoveringWidth = std::max(hoveringWidth, textout.getScaledWidth(*t));
					}
				
					count_t lineCount = hoveringBrief ? hoveringText.briefEndAt : hoveringText.lines.Count();
					float h = textout.getScaledHeight()*(lineCount-1);
					Rect<> r;

					if (hoveringOver->flipX)
					{
						if (hoveringOver->flipY)
						{
							textout.locate(base - float2(hoveringWidth,textout.getScaledHeight()));
							r = Rect<>(base -float2(hoveringWidth,textout.getScaledHeight() + h+textout.getScaledHeight()),base + float2(0,-textout.getScaledHeight()*0.8f));
						}
						else
						{
							textout.locate(base + float2(-hoveringWidth,h + textout.getScaledHeight()));
							r = Rect<>(base-float2(hoveringWidth,0) ,base + float2(0,h + textout.getScaledHeight()*1.2f));
						}
					}
					else
					{
						base.x = std::min(base.x,1.f - hoveringWidth);

						if (hoveringOver->flipY)
						{
							textout.locate(base - float2(0,textout.getScaledHeight()));
							r = Rect<>(base -float2(0,textout.getScaledHeight() + h+textout.getScaledHeight()),base + float2(hoveringWidth,-textout.getScaledHeight()*0.8f));
						}
						else
						{
							textout.locate(base + float2(0,h + textout.getScaledHeight()));
							r = Rect<>(base ,base + float2(hoveringWidth,h + textout.getScaledHeight()*1.2f));
						}
					}

					r.x.min = r.x.min * 0.5f + 0.5f;
					r.x.max = r.x.max * 0.5f + 0.5f;
					r.y.min = r.y.min * 0.5f + 0.5f;
					r.y.max = r.y.max * 0.5f + 0.5f;
					//Rect<>r(0,0,1,1);

					SurfaceRenderer::Rasterize(r);
					SurfaceRenderer::Flush();

					textout.color(1,1,1);
					for (index_t i = 0; i < lineCount; i++)
						textout << hoveringText.lines[i] << nl;
					textout.popState();
				}





				//static Engine::Aspect<> aspect;
				//aspect.depthTest = Engine::VisualEnum::NoDepthTest;
				//display.pick(aspect);
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

			Buffer0<Objects::PBaseCommand>	allocatedEntityCommands;
			int2 at;
			void	VisitCommand(const int2&coords,const Objects::PBaseCommand&cmd)
			{
				GridCell&cell = AllocateCell(cmd);
				cmd->Refresh(Network::ControlEnabled());
				cell.gridCoords = coords;
				cell.Update(camera);
				allocatedEntityCommands<<cmd;
			}

			void RefreshButtonState()
			{
				foreach (allGuiCells,cell)
				{
					cell->command->Refresh(Network::ControlEnabled());
				}

			}

			static int CollectionCommandCompare(const Objects::PBaseCommand&a,const Objects::PBaseCommand&b)
			{
				const GroupFocusCommand&g0 = *(GroupFocusCommand*)a.get();
				const GroupFocusCommand&g1 = *(GroupFocusCommand*)b.get();
				if (g0 < g1)
					return -1;
				return 1;
			}

			static void CollectionToCommands(CollectionTemplateRow<Objects::WWorldEntity>&context)
			{
				context.commands.Clear();
				context.collection.VisitAllValues([&context](Buffer0<Objects::WWorldEntity>&entries)
				{
					Objects::PWorldEntity e = entries.first().lock();
					if (e)
					{
						PGroup group (new Group());
						group->name = e->GetTypeName();
						group->entries.adoptData(entries);
						group->UpdateSignificance();

						context.commands << Objects::PBaseCommand(new GroupFocusCommand(group));
					}
				});
				context.collection.Clear();
				ByComparator::QuickSort(context.commands,CollectionCommandCompare);
				context.Refresh();
			}

			void					RefreshContacts()
			{
				//this is off:
				//currentContactListWidth = std::max(1, int((display.pixelAspect()-0.67f)*15));

				const float viewPortRange = 2.f * display.pixelAspect();
				const float screenRelativeRightPanel = 300.f / display.clientWidth();
				const float inViewPortRange = screenRelativeRightPanel * viewPortRange;
				const int hexOffset = 5;
				const float leftOffset = stepX *2 * hexOffset + float(1 - (hexOffset % 2)) * stepX;
				const float remainder = viewPortRange - inViewPortRange - leftOffset;
				const int iconRange = std::max(3,(int)(remainder / stepX / 2));
				currentContactListWidth = iconRange-2;


				//camera.UpdateProjection2(-1,-1,-1+2.f * display.pixelAspect(),1,-1,1);
				float len = 300.f / display.clientWidth();
				textout.locate(1.f - len,0.95f);



				//ClearContacts();

				{
					hostileGroups.collection.Clear();
					friendlyGroups.collection.Clear();


					Objects::ForeachWorldEntity([](const Objects::PWorldEntity&e)
					{
						String key = e->GetTypeName() + "|";
						if (e->owner)
							key += e->owner->id.ToFullString();
						if (e->health <= 0)
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
					});
					CollectionToCommands(friendlyGroups);
					CollectionToCommands(hostileGroups);
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



			void					Configure(const Objects::PWorldEntity&e)
			{
				foreach (allocatedEntityCommands,cmd)
					FindAndDeleteCell(*cmd);
				allocatedEntityCommands.Clear();

				bool showControls = showControlsAndContacts && e
						#ifndef DEBUG_ACCESS_RESTRICTIONS
							&& e->isOwnedByMe && e->health > 0
						#endif
					 ;

				if (showControlsAndContacts)
				{
					at = int2(5,1);
					Objects::globalCommands.VisitMembers(&VisitCommand,false);
					Next();
					Next();
				}

				if (e)
				{
					hasFocusButton = true;
					if (showControls)
						e->VisitAllCommands(VisitCommand,false);

					selectedFocus.reset(new EntityFocusCommand(e) );
					selectedFocus->Refresh(true);//Network::ControlEnabled());
					selectedFocus->UpdateTexture(e,false,e->isOwnedByMe ? friendlyBackgroundColor : hostileBackgroundColor);

					GridCell&cell = AllocateCell(selectedFocus);
					cell.gridCoords = GetFocusLocation();
					cell.flipY = true;
					cell.flipX = true;
					cell.Update(camera);
				}
				else
				{
					hasFocusButton = false;
					FindAndDeleteCell(selectedFocus);
					selectedFocus.reset();
				}



				if (showControlsAndContacts)
					RefreshContacts();
				else
				{
					friendlyGroups.RemoveIcons();
					hostileGroups.RemoveIcons();
					animationTimeLine.RemoveIcons();
				}

			}
		}






		void Create()
		{
			HexGrid::Create();
			Gauge::StaticCreate();

			//camera.location = float3(0,-100,0);
			barProjectionCamera.retraction = float3(0,0,-120);
			barProjectionCamera.direction = float3(0,-1,0.3).Normalized();
			barProjectionCamera.upAxis = float3(0,0,1);
			barProjectionCamera.UpdateView();
			barProjectionCamera.UpdateProjection2(1.f,0.1f,1000,35.f);
			barProjectionCamera.depthTest = Engine::NoDepthTest;
			barProjectionCamera.region.x.max = 1.f / display.pixelAspect();

			textoutCamera.UpdateProjection(-1,-1,1,1,-1,1);
			textoutCamera.depthTest = Engine::NoDepthTest;



			static const float stepX = (0.5f + 1.f) * 2.2f * 1.2f HIB(/1.2f);
			static const float stepY = 0.86602540378443864676372317075294f * 2.2f;
//			const float3 base(-38,0,-36);
			const float3 base(-33,0,-36);
			Random random(1024);

			gauges.SetSize(Objects::ResourceVector::NumEntries);
			#ifndef NO_HIBERNATION
				trafficGauge.isTraffic = true;
			#endif

			for (index_t i = 0; i < gauges.Count(); i++)
			{
				Gauge&g = gauges[i];
				g.base.x = float(i HIB(+2)) * (stepX );
				g.base.y = (1.f - float(i%2)) * stepY;
				g.base += base;
				g.LoadTexture(Objects::Resource::Reinterpret(i));
				//test.Construct();
			}
			#ifndef NO_HIBERNATION
			{
				Gauge&g = trafficGauge;
				g.base.x = 0;
				g.base.y = stepY;
				g.base += base;

				g.IconItem::LoadTexture("Traffic");
				g.myTag = Objects::TrafficResourceTag();
				g.color = float3(0.1,1,1);
			}
			#endif


			timeResource.color = float3(1);
			timeResource.LoadTexture("time");



			gauges[Objects::Resource::Energy].color = float3(0.2,0.4,1);
			gauges[Objects::Resource::Fuel].color = float3(0.4,1,0.2);
			gauges[Objects::Resource::Minerals].color = float3(0.7,0.7,0.7);
			#ifndef NO_MUNITION
				gauges[Objects::Resource::Munitions].color = float3(1,0.4,0.2);
			#endif
			gauges[Objects::Resource::Uranium].color = float3(1,1,0.1);
			gauges[Objects::Resource::Uranium].alternatingColor = float3(0.1,0.1,0.1);
			gauges[Objects::Resource::Uranium].usesAlternatingColors = true;
		}


		static bool showExtraGauges = false;

		#ifndef NO_HIBERNATION

	
			void	UpdateTraffic(int traffic)
			{
				showExtraGauges = true;
				int gain = Objects::uplinkTraffic - Objects::onlineUplinkTraffic;


				if (Objects::onlineUplinkTraffic*2 > Network::uplinkBandwidth)
					Tutorial::ShowMessage("Uplink Traffic");


				trafficGauge.Set(Objects::onlineUplinkTraffic,Network::uplinkBandwidth,gain,traffic,0,true);
			}
		#endif

		void	UpdateTraffic()
		{
			if (Objects::selectedEntity)
			{
				#ifndef NO_HIBERNATION
				UpdateTraffic(
					Objects::selectedEntity->GetUplinkTraffic()
					);
				#else
					showExtraGauges = true;

				#endif
			}
			else
				HideExtraGauges();
		}

		void	HideExtraGauges()
		{
			showExtraGauges = false;
			#ifndef NO_HIBERNATION
				int gain = Objects::uplinkTraffic - Objects::onlineUplinkTraffic;


				if (Objects::onlineUplinkTraffic*2 > Network::uplinkBandwidth)
					Tutorial::ShowMessage("Uplink Traffic");

				trafficGauge.Set(Objects::onlineUplinkTraffic,Network::uplinkBandwidth,gain,0,0,true);
			#endif
		}

		void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change)
		{
			#ifndef NO_HIBERNATION
				UpdateTraffic(traffic);
			#else
				showExtraGauges = true;
			#endif
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
				gauges[i].Set(current.level[i],max.level[i],0,0,change.level[i],true);

		}

		void	Update(TRAFFIC	const Objects::ResourceVector&current, const Objects::ResourceVector&max, 
								const Objects::ResourceVector&current2, const Objects::ResourceVector&max2, 
								const Objects::ResourceVector&change)
		{
			#ifndef NO_HIBERNATION
				UpdateTraffic(traffic);
			#else
				showExtraGauges = true;
			#endif
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
				gauges[i].Set(current.level[i],max.level[i],current2.level[i],max2.level[i],change.level[i],true);
		}




		void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max)
		{
			#ifndef NO_HIBERNATION
				UpdateTraffic(traffic);
			#else
				showExtraGauges = true;
			#endif
			for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
				gauges[i].Set(current.level[i],max.level[i],0,0,0,true);

		}

	//	void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change,
	//					const Objects::ResourceVector&localCurrent/*, const Objects::ResourceVector&localChange, bool isEnabled*/)
	//	{
	//		#ifndef NO_HIBERNATION
	//			UpdateTraffic(traffic);
	//		#else
	//			showExtraGauges = true;
	//		#endif
	//		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
	////			resources[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],localChange.level[i],isEnabled);
	//			gauges[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],0,true);
	//	}

		void	Render()
		{
			HexGrid::Render();
			//if (!Objects::selectedEntity)
			//{
			//	return;
			//}
			if (!showControlsAndContacts && !Objects::selectedEntity)
				return;
			display.StoreRendererState();
			display.SetBlendMode(Engine::BlendMode::AlphaBlend);
			display.SetRasterizer(true,true);
			display.SetDepthTest(false,false);


			if (resourcesAsList)
			{
				display.pick(textoutCamera);
					textout.pushState();
					textout.scale(2.5f,2.5f);
					float lineHeight = textout.getScaledHeight()*1.2f;
					SurfaceRenderer::SetColor(float4(0,0,0,0.6f));
					float w = 0.2f / display.pixelAspect();
					//textout.locate(-1.f + 0.01f,-1.f + textout.getScaledHeight()*1.2f*(y+6));
					Rect<> t(0,lineHeight*2.f,w,lineHeight*0.5f*11.5f);
					SurfaceRenderer::Rasterize(
						float2(t.x.min,t.y.min),float4(0,0,0,0.8f),
						float2(t.x.max,t.y.min),float4(0,0,0,0.2f),
						float2(t.x.max,t.y.max),float4(0,0,0,0.2f),
						float2(t.x.min,t.y.max),float4(0,0,0,0.8f)
					);
					SurfaceRenderer::Flush();
					//textout.scale(4.f,4.f);
					//textout.color(1,1,1);

					#ifndef NO_HIBERNATION
						trafficGauge.RenderListText(0);	//bottom
					#endif
					if (showExtraGauges)
					{
						index_t at = 2;
						foreach (gauges,res)
						{
							res->RenderListText((int)(at++));
						}
					}

					textout.popState();
			}
			else
			{

				barProjectionCamera.region.x.max = 1.f / display.pixelAspect();
				//(sin(Engine::timing.now)*0.5f+0.5f) + 0.5f;

				display.pick(barProjectionCamera);



				display.SetBlendMode(Engine::BlendMode::AlphaBlend);

				#ifndef NO_HIBERNATION
					trafficGauge.Render(0);
				#endif
				if (showExtraGauges)
				{
					for (index_t i = 0; i < gauges.Count(); i+=2)
						gauges[i].Render(0);
					for (index_t i = 1; i < gauges.Count(); i+=2)
						gauges[i].Render(0);
				}

				display.SetBlendMode(Engine::BlendMode::FlareBlend);
				#ifndef NO_HIBERNATION
					trafficGauge.Render(1);
				#endif

				if (showExtraGauges)
				{
					for (index_t i = 0; i < gauges.Count(); i+=2)
						gauges[i].Render(1);
					for (index_t i = 1; i < gauges.Count(); i+=2)
						gauges[i].Render(1);
				}


				display.SetBlendMode(Engine::BlendMode::AlphaBlend);

				#ifndef NO_HIBERNATION
					trafficGauge.UpdateTextProjection(barProjectionCamera);
				#endif
				if (showExtraGauges)
					foreach (gauges,res)
					{
						res->UpdateTextProjection(barProjectionCamera);
					}

				display.pick(textoutCamera);
					textout.pushState();
					textout.scale(4.f,4.f);
					textout.color(1,1,1);

					#ifndef NO_HIBERNATION
						trafficGauge.RenderText();
					#endif
					if (showExtraGauges)
						foreach (gauges,res)
						{
							res->RenderText();
						}

					textout.popState();
			


			}

			display.RestoreRendererState();

		}



		void Destroy()
		{
			HexGrid::Destroy();
			Gauge::StaticDestroy();
			gauges.Clear();
			#ifndef NO_HIBERNATION
				trafficGauge.Destroy();
			#endif
			timeResource.texture.Clear();


		}




		
		void	RefreshButtonState()
		{
			HexGrid::RefreshButtonState();
		}




	}


	namespace StatePanel
	{
		//0.0135f;

		float		OnTag(const float3&at, char tagChar, float scaleX, float scaleY)
		{
	//		float2 at = float2(textout.state.left+0.015,textout.state.top - textout.getScaledHeight() * (float(textout.state.line)-0.5f));

			index_t i = tagChar - tagOffset;
	//		float2 p = at.xy;
			if (i < Objects::Resource::NumberOfPossibilities/* UI::gauges.Count()*/)
			{
				UI::gauges[i].RenderIcon(tagIconSize /** scaleX*/);
			}
			else
			{
				if (i - Objects::Resource::NumberOfPossibilities == 0)
					UI::timeResource.RenderIcon(tagIconSize);
				#ifndef NO_HIBERNATION
					else
						UI::trafficGauge.RenderIcon(tagIconSize);
				#endif
			}
			return tagIconSize * scaleX;
			//;*2.f/display.pixelAspect();
		}


		void		Render(const Objects::WorldEntity&e)
		{
			textout.pushState();
			float3 color (1);
			#ifndef NO_HIBERNATION
				const Objects::Hibernating*h = e.ToHibernating();
			#endif
			textout.color(color);
			


			float len = 300.f / display.clientWidth();
			//if (len == 0)
				//len = textout.scaledWidth("012345678901234567890123456789012345");
			textout.locate(1.f - len,0.95f);
			const String&name= e.GetNiceName();
			if (name.length() < 20)
				textout << name <<nl;
			else
			{
				index_t split = name.GetIndexOf('\'');
				if (split == 0)
					split = name.GetIndexOf(' ');
				textout << name.subStringRef(0,split-1) << nl;
				textout << ' '<<name.subStringRef((int)split-1) << nl;
				
				#ifdef _DEBUG
					textout << ' '<<e.GetName()<<nl;

				#endif

			}

			#ifdef _DEBUG
				textout << " Location: "<<e.location.ToString()<<nl;
				if (e.IsMobile())
					textout << " Last point of origin: "<<((const Objects::MobileEntity&)e).GetLastPointOfOrigin().ToString()<<nl;
				textout << " x: "<<e.system.x.xyz<<nl;
				textout << " y: "<<e.system.y.xyz<<nl;
				textout << " z: "<<e.system.z.xyz<<nl;
				textout << " w: "<<e.system.w.xyz<<nl;
				TVec3<> wz = e.system.w.xyz;
				Vec::normalize(wz);
				textout << " w1: "<<wz<<nl;
			#else
				textout << " Height: "<<e.location.height<<nl;
			#endif

			textout << " Owner: "<<e.owner->GetName();
			if (e.isOwnedByMe)
			{
				textout<<" (me)";
				if (e.health > 0)
				{
					static const Objects::WorldEntity *last=nullptr;
					static String lastDeployed;
					if (last != &e)
					{
						last = &e;
						lastDeployed = e.CompileDeploymentCount(e.owner);
					}
					if (lastDeployed.IsNotEmpty())
					{
						textout.PushColorState();
						textout.color(0.6,0.7,1.f);
						textout << nl << lastDeployed;

						textout.PopColorState();
					}
				}

			}
			textout << nl;
			textout << " Scanning-Range: "<<e.GetScanningRange() << nl;
			#ifndef NO_HIBERNATION
				textout << " "<<Objects::TrafficResourceTag()  <<e.GetUplinkTraffic() << nl;
				if (h)
					textout << " "<<Objects::TrafficResourceTag()  << "Priority: "<<h->GetUplinkPriority()<<nl;
			#endif
			textout.PushColorState();
			if (e.health > 0)
			{
				if (e.IsIndestructible())
					textout << " Indestructible" << nl;
				else
				{
					int maxHealth = e.GetMaxHealth();
					if (e.health < maxHealth/4)
						textout.Tint(1,0.2,0.2);
					elif (e.health < maxHealth/2)
						textout.Tint(1,1,0.2);
					elif (e.health < maxHealth)
						textout.Tint(0.2,1,0.2);

					textout << " Health: " << e.health << "/" << e.GetMaxHealth() << nl;
				}
			}
			else
			{
				textout.Tint(1, 0.8, 0.75);
				textout << " Ghost created " << -e.health << " round(s) ago" << nl;
				textout.color(color);
			}
			textout.PopColorState();
			#ifndef NO_HIBERNATION
				if (h->IsHibernating())
				{
					color = float3(0.5f);
					textout.color(0.6,1,1);
					textout << " Hibernating" << nl;
					textout.color(color);
				}
			#endif
			if (e.empDamageLevel)
			{
				textout.Tint(1,0.8,0.75);
				textout << " EMP-Damage detected: "<< e.empDamageLevel<<nl;
				textout.color(color);
			}


			if (e.IsMobile())
			{
				const Objects::MobileEntity*v = (const Objects::MobileEntity*)(&e);

				//if (!v->storedIn.expired())
					//textout << " Stored in: "<<v->storedIn.lock()->GetName()<<nl;	//probably never happens
				if (e.IsGroundBoundVehicle())
				{
					const Objects::Vehicle*veh = (const Objects::Vehicle*)(&e);
					textout << " Turret-Overweight: "<<std::max(0,veh->turret->weight - veh->base->carryingCapacity) << nl;
					textout << " Movement Initiative: "<<(int)v->GetMovementInitiative()<<"/"<<(int)veh->base->initiative<<nl;
				}
				elif (e.IsSatellite())
				{
					Objects::Satellite*sat = (Objects::Satellite*)(&e);
					int demand = sat->GetFuelConsumptionPerRound();
					if (!sat->RequiresFuel())
						demand = 0;
					textout << " Fuel-Consumption: "<<(char)(tagOffset+Objects::Resource::Fuel)<<demand;
					if (demand > 0)
						textout <<'+'<<sat->GetExtraFuelDemand();
						
					textout<<nl;
				}
				textout << " Attack Initiative: "<<(int)v->GetAttackInitiative()<<"/"<<(int)v->turret->initiative<<nl;
				textout << " Speed: "<<v->GetMovementStepsPerRound() << nl;
				if (v->turret->CanAttack())
				{
					//textout << " Range: "<<v->GetAttackRange() << nl;
					//textout << " Damage: "<<v->turret->damageLevel << nl;
					//textout << " Ammo-Usage: "<<v->turret->ammoNeedPerShot<<nl;
					textout << " Hrz. Range: "<<v->GetFlatAttackRange();
					textout << '+'<<v->turret->explosionRadius<< nl;
					textout << " Damage: "<<v->turret->damageLevel << nl;
					if (v->turret->ammoNeedPerShot > 0)
						textout << " "<<(char)(tagOffset+v->turret->ammoType)<<"Ammo-Usage: "<<v->turret->ammoNeedPerShot<<nl;

				}
				if (v->turret->CanAttack())
				{
					textout << " Attacks left: "<<int(v->CountPossibleAttacks())<<"/"<<v->GetAttacksPerRound();
					textout << nl;
					if (v->turret->ignoresVehicleMotion)
						textout << "  (ignores movement)"<<nl;
				}
				int opSpeed = v->GetOperationSpeed(Objects::Component::Construct);
				if (opSpeed)
					textout << " Converts "<<Objects::ResourceTag(Objects::Resource::Minerals)<<opSpeed<<" per round"<<nl;
				opSpeed = v->GetOperationSpeed(Objects::Component::Repair);
				if (opSpeed)
					textout << " Repairs "<<Objects::ResourceTag(Objects::Resource::Minerals)<<opSpeed<<" per round"<<nl;

				textout << " Tasks:" << nl;
				if (v->myTasks.IsEmpty())
				{
					if (v->unstable)
						textout << "  Operation done. Relocate to complete" << nl;
					else
						textout << "  idle"<<nl;
				}
				else
				{
					textout.Tint(0.75,0.8,1);
					foreach (v->myTasks,t)
					{
						textout << "  * "<<t->name;
						if (t->IsMode(Objects::CursorMode::AttackMode) && t->foci.Count() > 1)
							textout << " (x"<<t->foci.Count()<<')';
						if (t->job && t->job->roundsToJobCompletion)
						{
							textout << " ("<<t->job->roundsToJobCompletion<<" round"<<(t->job->roundsToJobCompletion != 1 ? "s)" : ")") << nl;
						}
						else
							textout << nl;
					}
					textout.color(color);
				}
				#ifndef ONE_COMPOUND_PER_FACTION
					const Objects::ResourceVector max = v->GetMaxStorageCapacity();
					bool any = false;
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
						if (max.level[i] > 0)
							any = true;
					if (any)
					{

						textout << "Resources in storage:" << nl;
						for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
							if (max.level[i] > 0)
							{

		//						textout << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<v->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
								textout << "  "<<(char)(tagOffset+i)<<" "<<v->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
								//textout << "        : "<<v->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
							}
					}
				#endif
			}

			if (e.IsStructure())
			{
				#ifdef _DEBUG
				{
					textout << " Connected to: "<<nl;
					const Objects::Structure&s = (const Objects::Structure&)e;
					foreach (s.connectedTo,c)
					{
						Objects::PStructure s = c->structure.lock();
						if (!s)
							continue;
						textout << "  "<<s->GetName()<<", wireless="<<c->wireless <<nl;
					}
				}
				#endif
				const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
				if (t)
				{
					textout << " Attack Initiative: "<<(int)t->turret->initiative<<nl;
					if (t->turret->CanAttack())
					{
						const Objects::Tower*v = t;	//rename
						textout << " Hrz. Range: "<<v->GetFlatAttackRange();
						textout << '+'<<v->turret->explosionRadius<< nl;
						textout << " Damage: "<<v->turret->damageLevel << nl;
						textout << " "<<(char)(tagOffset+v->turret->ammoType)<<"Ammo-Usage: "<<v->turret->ammoNeedPerShot<<nl;
					}
					if (t->GetAttacksPerRound() > 0)
						textout << " Attacks left: "<<int(t->CountPossibleAttacks())<<"/"<<t->GetAttacksPerRound() << nl;
					//textout << " Tasks:" << nl;

					//textout << "Resources in storage:" << nl;
					//const Objects::ResourceVector max = t->GetMaxStorageCapacity();
					//for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					//	if (max.level[i] > 0)
					//		textout << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<t->storage.level[i]<<"/"<<max.level[i]<<nl;
		
				}


				const Objects::VehicleStore*st = dynamic_cast<const Objects::VehicleStore*>(&e);
				if (st)
				{
					textout << " Max Vehicles per Store: "<<Objects::VehicleStore::MaxVehiclesPerStore<<nl;


					if (st->park)
					{
						textout << " Vehicles stored in park ("<<st->park->vehiclesStored<<"/"<<st->GetParkVehicleCapacity()<<"):"<<nl;


						textout.PushColorState();

						textout.Tint(0.8f,0.9f,1.1f);
						count_t cnt = 0;

						{
							const Objects::VehicleStore *other = st;
							for (const auto&v : other->stored)
							{
								Objects::PMobileEntity v1 = v.entity.lock();
								ASSERT1__(v1,st->ToEntity().id.ToString());
								if (++cnt > 10)
								{
									textout << "..."<<nl;
									break;
								}
								textout << "  "<<v1->GetNiceName() <<" (local)"<< nl;
							}
						}

						textout.Tint(0.9f,0.9f,0.9f);

						for (const auto&_s : st->park->members)
						{
							Objects::PVehicleStore other = _s.lock();
							if (other.get() == st)
								continue;
							for (const auto&v : other->stored)
							{
								Objects::PMobileEntity v1 = v.entity.lock();
								ASSERT1__(v1,st->ToEntity().id.ToString());
								if (++cnt > 10)
								{
									textout << "..."<<nl;
									break;
								}
								textout << "  "<<v1->GetNiceName() << nl;
							}
							if (cnt > 10)
								break;
						}
						textout.PopColorState();
					}
					else
					{
						textout << " Vehicles stored ("<<st->stored.Count()<<"/"<<Objects::VehicleStore::MaxVehiclesPerStore<<"):"<<nl;
						for (const auto&v : st->stored)
						{
							Objects::PMobileEntity v1 = v.entity.lock();
							ASSERT1__(v1,st->ToEntity().id.ToString());
							textout << "  "<<v1->GetNiceName() << nl;
						}
					}
				}


				const Objects::Structure*s = (const Objects::Structure*)(&e);
				
				textout.color(1,1,1);

				#if defined _DEBUG && !defined ONE_COMPOUND_PER_FACTION
					textout << " Compound ID: "<<s->compoundID << nl;
				#endif
				const Objects::Compound*net = nullptr;
				if (s->health > 0)
				{
					const Objects::Compound&network = s->GetCompound();
					net = &network;
					textout << " Compound status:" << nl;
					if (!network.stable)
					{
						textout.PushColorState();
						if (e.isOwnedByMe)
							textout.Tint(1, 0.2, 0.2);

						textout << "  UNSTABLE";
						if (!e.isOwnedByMe)
							textout << " (?)";
						textout << nl;
						textout.PopColorState();
					}
					textout << "  Resource consumption:" << nl;
					textout << "    (demand/supply)" << nl;
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					{
						textout.PushColorState();
						if (e.isOwnedByMe && network.total.demand.level[i] > network.total.supply.level[i])
							textout.Tint(1, 0.2, 0.2);
						textout << "   " << (char)(tagOffset+i) << " " << network.total.demand.level[i] << "/" << network.total.supply.level[i] << nl;
						textout.PopColorState();
					}
					textout << "  Resource storage:" << nl;
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					{
						if (network.total.storageMax.level[i] > 0)
							textout << "   " << (char)(tagOffset+i) << " " << network.total.storage.level[i] << "/" << network.total.storageMax.level[i] << nl;
					}
				}
				else
				{
					//textout << " Ghost created in round "<<
				}

				{
					const Objects::MobileEntityProducer*ps = e.CastToProducer();
					if (ps)
					{
						//const Objects::VehiclePlant*ps = (const Objects::VehiclePlant*)(&e);
						bool active = true;
						if (e.IsWorkingStructure())
						{
							active = ((const Objects::WorkingStructure*)(&e))->IsEnabled();
						}
						
						if (ps->queue.IsNotEmpty())
						{
							textout << "  producing "<<ps->queue.first().name<<nl;
							if (!active)
								textout << "   completion (when enabled) in "<<Objects::TimeResourceTag() <<ps->productionRoundsLeft<<nl;
							else
								textout << "   completion in "<<Objects::TimeResourceTag() <<ps->productionRoundsLeft<<nl;
						}

					}
					else
					{


					}
				}

				if (e.IsWorkingStructure())
				{
					const Objects::WorkingStructure*ws = (const Objects::WorkingStructure*)(&e);
					textout << nl << " Structure status:"<<nl;
					if (ws->IsHibernating())
						textout << "  Hibernating" << nl;
					textout.color(color);

					if (ws->AwaitsUserInteraction())
						textout << "  Awaiting user input" << nl;
					else
						textout << (ws->IsEnabled() ? "  Enabled" : "  Disabled") << nl;
				


					{
						
						if (ws->IsMine())
						{
							const Objects::Mine*ms = (const Objects::Mine*)(ws);
							textout << "  Efficiency: "<<  int(ms->GetEfficiency()*100) << "%"  << nl;
							if (ms->IsLinkedToCore())
								textout << "  Linked to Core" << nl;
							else
							{
								if (e.isOwnedByMe)
									textout.Tint(1,0.5,0.4);
								textout << "  Disconnected from Core";
								if (!e.isOwnedByMe)
									textout << " (?)";
								textout << nl;

								textout.color(color);
							}
						}
						else
						{
							const Objects::NexusPort*np = dynamic_cast<const Objects::NexusPort*>(&e);
							if (np)
							{
								if (np->isOwnedByMe)
								{
									if (np->resistence)
										textout << "  Remaining resistence: "<<np->resistence << nl;
									else
										textout << "  Captured"<< nl;
								}
							}
						}
					}
					Objects::ResourceVector	delta,storage,storageMax;
					ws->GetResourceVector(delta,storage,storageMax);

					bool any = false;
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] < 0)
							any = true;
					const bool isEnabled = ws->IsEnabled();
					if (any)
					{
						textout << " Consumes";
						if (!isEnabled)
							textout << " (if enabled)";
						textout <<':'<<nl;
						if (!isEnabled)
							textout << "    (demand/compound supply)" << nl;
					

						for (index_t i = 0; i < delta.NumEntries; i++)
							if (delta.level[i] < 0)
							{
								//textout << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<-delta.level[i];
								if (isEnabled)
									textout << "  "<<(char)(tagOffset+i)<<" "<<-delta.level[i];
								else
								{
									if (net && delta.level[i] + net->total.balance.level[i] < 0)
										textout.Tint(1,0.6,0.5);
									textout << "  "<<(char)(tagOffset+i)<<" "<<-delta.level[i];
								
									if (net)
									{
										textout << " / " << net->total.balance.level[i];
										if (net->total.storage.level[i])
											textout << " (" << net->total.storage.level[i] << " stored)";
									}
									//textout << nl;
									textout.color(color);

								}
								textout << nl;
							}
					}
					any = false;
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] > 0)
							any = true;
					if (any)
					{
						//textout << " Produces:"<<nl;
						textout << " Produces";
						bool isEnabled = ws->IsEnabled();
						if (!isEnabled)
							textout << " (if enabled)";
						textout <<':'<<nl;

						for (index_t i = 0; i < delta.NumEntries; i++)
							if (delta.level[i] > 0)
								textout << "  "<<(char)(tagOffset+i)<<" "<<delta.level[i]<<nl;
					}
					any = false;
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (storageMax.level[i] > 0)
							any = true;
					if (any)
					{
						textout << " Storage:"<<nl;
						for (index_t i = 0; i < delta.NumEntries; i++)
							if (storageMax.level[i] > 0)
								textout << "  "<<(char)(tagOffset+i)<<" "<<storage.level[i]<<"/"<<storageMax.level[i]<<nl;
					}
				}


	
			}


			//textout.getActiveFont()->EndTagMode();



			textout.popState();
	
		}


	}
#endif
