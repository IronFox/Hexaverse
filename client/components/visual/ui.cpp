#include <global_root.h>
#include "ui.h"
#include "dialog.h"
#include "../network.h"
#include "tutorial.h"


#include "../../DX11/texture_font.h"
#include "surfaceRenderer.h"
#include "../configurationMode.h"
#include "objectRendering.h"
#include "objectChoices.h"
#include "display.h"

#include <objects/structure/storageStructures.h>
#include <objects/structure/vehicleStore.h>
#include <objects/structure/laboratory.h>
#include <objects/structure/mine.h>



namespace UI 
{

	namespace DX11 = Engine::DX11; //!< Shortcut

	bool showControlsAndContacts = true;


	void	ShowSession()
	{
		if (GUI::IsModal())
		{
			if (Dialog::ShowingFactions())
				GUI::CloseAllModal();
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
			GUI::CloseAllModal();
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
		iconShader.quickSlots.First() = texture;
		display.RenderVertices(iconShader,0,1);
	}





	void	Gauge::Set(int current, int max, int changePerRound, int localCurrent, int localChange, bool enabled)
	{

		//max = std::max(10.f,max);
		if (current == this->current && max == this->max && changePerRound == this->changePerRound
			&& localCurrent == this->localCurrent && localChange == this->localChange && enabled == this->enabled)
			return;
		top0 = std::max(current,current+changePerRound);
		top1 = std::max(max,top0);
		top = std::max(abs(current+localChange), top1);
		this->current = current;
		this->max = max;
		this->changePerRound = changePerRound;
		this->localCurrent = localCurrent;
		this->localChange = localChange;
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
		//	textout.PushState();
		//	textout.SetScale(0.2,0.2);
		//	display.SetRasterizer(true,false);
		//	textout.locate(0,0,1);
		//	textout.SetColor(1,1,1);
		//	textout << "test";
		//	textout.PopState();

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
		textout.PushState();
		//textout.locate(projectedTop);
		textout.MoveTo(-1.f + 0.01f,-1.f + textout.GetScaledHeight()*1.2f*(y+5));

		//textout.SetLine(-1);
		textout << myTag;
			
			
		textout.SetColor(color * 0.5f + float3(0.5f));
		if (changePerRound > 0)
		{
			textout << "+";
			Write(changePerRound);
		}
		else
			if (changePerRound < 0)
			{
				textout.SetColor(1,0.6,0.5);
				Write(changePerRound);
				textout.SetColor(color * 0.5f + float3(0.5f));
			}
		if (max > 0)
			textout.Scale(1.2f,1.2f);
		if (max > 0)
		{
			textout << ' ';
			Write(current);
			textout.Scale(0.6f,0.6f);
			textout <<'/';
			Write(max);
			textout.Scale(1.f/0.6f,1.f/0.6f);
		}
		//textout << max;
		textout.PopState();


	}

	void	Gauge::RenderText()
	{
		//textout.SetScale(0.2,0.2);	//leave it
		//display.SetRasterizer(true,false);
		{
			textout.PushState();
			textout.SetColor(color * 0.5f + float3(0.5f));
			textout.MoveTo(projectedBottom);
			textout.SetLine(-1);
			textout << current << nl;
			textout.PopState();
		}
		if (max)
		{
			textout.PushState();
			textout.MoveTo(projectedMax);
			textout.Scale(0.6f,0.6f);
			textout.SetLine(-1);
			textout << max;
			textout.PopState();
		}

		if (changePerRound != 0)
		{
			textout.PushState();
			textout.MoveTo(projectedDelta.xy);
			textout.Scale(0.8f,0.8f);
			textout.SetLine(-1);
			if (changePerRound > 0)
			{
				textout.SetColor(color);
				textout << '+';
			}
			else
				textout.SetColor(1,0.6,0.5);
			textout << changePerRound;
			textout.PopState();
		}
			
		{
			textout.PushState();
			//textout.locate(projectedTop);
			textout.Scale(0.6f,0.6f);
			textout.MoveTo(projectedTop.x,projectedTop.y + textout.GetScaledHeight()*2.5f);

			//textout.SetLine(-1);
			textout << myTag;
			//textout << max;
			textout.PopState();
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
			if (localCurrent > 0)
			{
				ConstructPart(1.f,0,localCurrent,float4(1,0,0,0.95f),geo[0]);
				ConstructPart(1.f,localCurrent,current,float4(color,0.95f),geo[0]);
			}
			else
				ConstructPart(1.f,0,current,float4(color,0.95f),geo[0]);
			if (changePerRound > 0)
			{
				int top = current + std::max(0,changePerRound - (enabled ? localChange : 0));
				//int top1 = std::min(max,top);
				ConstructPart(0.8f,current,top,float4(color,0.f),float4(color,0.6f),geo[0]);

				//if (top != top1)
					//ConstructPart(0.8f,top1,top,float4(color,0.f),float4(color,0.6f),geo[0]);
			}
		}
		else
		{
			if (localCurrent > 0)
			{
				int step = std::min(localCurrent,std::max(0,(current+changePerRound)));
				ConstructPart(1.f,0,step,float4(1,0,0,0.95f),geo[0]);

				ConstructPart(1.f,step,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
			}
			else
			{
				ConstructPart(1.f,0,std::max(0,(current+changePerRound)) ,float4(color,0.95f),geo[0]);
			}
			ConstructPart(1.f,std::max(0,(current+changePerRound)),current,float4((color + float3(1,0.5,0.5))/1.5f,0.95f),geo[0]);
		}

		if (localChange != 0)
		{
			float t = current+changePerRound;
			if (localChange > 0)
			{
				if (enabled)
					ConstructPart(0.6f,t- localChange,t,float4(1,0,0,0.f),float4(1,0,0,0.6f),geo[0]);
				else
					ConstructPart(0.6f,t,t + localChange,float4(1,0.5f,0,0.f),float4(1,0.5f,0,0.6f),geo[0]);
			}
			else
				if (enabled)
					ConstructPart(0.6f,t,t-localChange,float4(1,0,0,0.6f),float4(1,0,0,0.f),geo[0]);
				else
					ConstructPart(0.6f,t+localChange,t,float4(1,0.5f,0,0.6f),float4(1,0.5f,0,0.f),geo[0]);
				//ConstructPart(0.8f,current,top1,float4(color,0.f),float4(color,0.6f),geo[0]);
		}

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
			HexGrid::animationTimeLine.Refresh(HexGrid::GetGridConfig());
		}

		void			RegisterAttack(float time, bool isFriendly,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, const Objects::Renderer::SmokeTrailIndex&smokeTrailIndex)
		{
			HexGrid::Custom::AnimationEventFocusCommand*cmd = new HexGrid::Custom::AnimationEventFocusCommand("Attack","Attack",focusLocation,focusEntityID,time,isFriendly,!isFriendly);
			cmd->smokeTrailIndex = smokeTrailIndex;
			if (!isFriendly)
				cmd->state |= Objects::CommandFlags::IsFlashing;
			HexGrid::animationTimeLine.commands << Objects::Command::PVisualCommand(cmd);
			HexGrid::animationTimeLine.Refresh(HexGrid::GetGridConfig());
		}

		void			RegisterEntityDestruction(float time, bool isOwnedByMe,  const TVec3<>&focusLocation, const TUniqueID&focusEntityID, const String&entityName)
		{
			HexGrid::Custom::AnimationEventFocusCommand*cmd = new HexGrid::Custom::AnimationEventFocusCommand("Entity Death","Entity Death",focusLocation,focusEntityID,time,isOwnedByMe,isOwnedByMe);
			if (isOwnedByMe)	//flipped logic
				cmd->state |= Objects::CommandFlags::IsFlashing;
			cmd->fallbackTargetName = entityName;
			HexGrid::animationTimeLine.commands << Objects::Command::PVisualCommand(cmd);
			HexGrid::animationTimeLine.Refresh(HexGrid::GetGridConfig());
		}

		bool			HasRegisteredHostileActivity()
		{
			foreach (HexGrid::animationTimeLine.commands,cmd)
			{
				std::shared_ptr<HexGrid::Custom::AnimationEventFocusCommand> aec = To<HexGrid::Custom::AnimationEventFocusCommand>::Cast(*cmd);
				if (aec->isHostileActivity)
					return true;
			}
			return false;
		}


	}




	namespace HexGrid
	{

		CollectionTemplateRow<Objects::WWorldEntity>			friendlyGroups(0),hostileGroups(1);
		DynamicRow												animationTimeLine(2);

		count_t					currentContactListWidth = 10;


		bool		Group::operator<(const Group&other) const
		{
			return significance > other.significance;
		}

		Objects::PWorldEntity	Group::GetPrototype() const
		{
			Objects::PWorldEntity e = entries.First().lock();
			ASSERT__(e);
			if (e->IsStructure())
				return Objects::GetPrototypeByName(e->GetCTypeName());
			return e;	//good enough
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
			if (e->health <= 0)
			{
				if (e->isOwnedByMe)
					significance *= 15.f / (1.f + sqr(e->health));
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


			AnimationEventFocusCommand::AnimationEventFocusCommand(const String&name, const String&iconName, const TVec3<>&focusLocation, const TUniqueID&focusID, float atTime, bool isFriendly, bool isHostileActivity)
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
				else
					outResult << "Owner: "+target->owner->GetName();
				if (target->health > 0)
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
					outResult << "Offensive ("+String(b->GetAttacksPerRound())+"x"+String(b->turret->GetDamageLevel(instance->owner))+")";
//					if (target->empDamageLevel)
//					outResult << "EMP-Damage detected ("+String(target->empDamageLevel)+")";

				outResult.MarkBriefEnd();
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

			Refresh(HexGrid::GetGridConfig());
		}

		void	DynamicRow::Refresh(const TGridConfig&cfg)
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
				moreRight = visibleRange.GetExtent() > currentContactListWidth;
				if (moreRight)
					visibleRange.end = visibleRange.start + currentContactListWidth;
				this->offset = newOffset;
			}
			if (offset > 0)
			{
				GridCell&cell = AllocateCell(leftCommand);
				cell.gridCoords = leftCommand->gridCoords;
				cell.flipY = true;
				cell.Update(cfg,camera);
			}
			else
				FindAndDeleteCell(leftCommand);
			if (moreRight)
			{
				GridCell&cell = AllocateCell(rightCommand);
				cell.gridCoords = DynamicRowOffset + int2(currentContactListWidth+1,rowIndex);
				cell.flipY = true;
				cell.Update(cfg,camera);
			}
			else
				FindAndDeleteCell(rightCommand);

//				rightCommand->gridCoords.x = int(ContactListOffset.x + 1+currentContactListWidth);

			RebuildCommands(cfg);
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

		void							DynamicRow::RebuildCommands(const TGridConfig&cfg)
		{
			RemoveCommandIcons();
			if (!showControlsAndContacts)
				return;
			for (index_t i = 0; i < visibleRange.GetExtent(); i++)
			{
				const Objects::Command::PVisualCommand cmd = commands[i+visibleRange.start];
				GridCell&cell = AllocateCell(cmd);
				cell.gridCoords = DynamicRowOffset + int2(i+1,rowIndex);
				cell.flipY = true;
				cell.Update(cfg,camera);
				lastVisible << cmd;
			}
		}








		std::shared_ptr<Custom::EntityFocusCommand>			selectedFocus;

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

			static void Add(const TGridConfig&cfg,const int2&coords, const Objects::Command::PVisualCommand&cmd)
			{
				GridCell&cell = AllocateCell(cmd);
				cell.gridCoords = coords;
				cell.flipX = true;
				cell.Update(cfg,camera);
				cmd->Refresh(Network::ControlEnabled());
			}

			void			Refresh(const TGridConfig&cfg)
			{
				if (!showControlsAndContacts)
				{
					foreach (commands,c)
						FindAndDeleteCell(*c);
					return;
				}
				foreach (commands,c)
					Add(cfg,(*c)->gridCoords, *c);
			}
		}

	}



	namespace HexGrid
	{


		void		GridCell::Update(const TGridConfig&cfg, const Engine::Aspect<>&aspect)
		{
			float2 center = ToFloatCoords(cfg);
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
		float2		GridCell::ToFloatCoords(const TGridConfig&cfg) const
		{
			int2 cellCoords = gridCoords;
			if (!flipX && !flipY)
				cellCoords += int2(3,1);
			float x;
			if (flipX)
				x = (-1+2.f * display.GetPixelAspect()) - cfg.stepX *2 * cellCoords.x - float(1 - (cellCoords.y % 2)) * cfg.stepX;
			else
				x = -1.f + cfg.stepX *2 * cellCoords.x + float(1 - (cellCoords.y % 2)) * cfg.stepX;
			float2	rs(x, -1.f + cfg.stepY * cellCoords.y);
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

		GridCell&		AllocateCell(const Objects::Command::PVisualCommand&cmd)
		{
			bool isNew;
			return AllocateCell(cmd,isNew);
		}

		GridCell&		AllocateCell(const Objects::Command::PVisualCommand&cmd, bool&isNew)
		{
			DBG_ASSERT__(showControlsAndContacts);

			foreach (allGuiCells,c)
				if (c->command == cmd)
				{
					isNew = false;
					return *c;
				}
			GridCell&c = allGuiCells.Append();
			c.command = cmd;
			isNew = true;
			return c;
		}

		void			FindAndDeleteCell(const Objects::Command::PVisualCommand&cmd)
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
		DX11::ObjectBuffer<float4>	center,visualCodeRGB_CellSize;
		Engine::OrthographicAspect<> camera;
		//Buffer0<Group>	friendlyGroups,hostileGroups;



		void						Configure(const Objects::PWorldEntity&e);

		void						Create()
		{
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
			barProjectionCamera.region.x.max = 0.9f;
			//1.f / display.GetPixelAspect();
			const auto cfg = GetGridConfig();
			camera.UpdateProjection(-1,-1,-1+2.f * display.GetPixelAspect(),1,-1,1);
			foreach (allGuiCells,cell)
				cell->Update(cfg,camera);
			RefreshContacts();
		}


		void						Destroy()
		{
			center.Clear();
			visualCodeRGB_CellSize.Clear();
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
						if (c->command->state & Objects::CommandFlags::IsInvisible)
							continue;

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
			}
			return false;
		}

		GridCell*		hoveringOver = nullptr;
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

			foreach (allGuiCells,c)
			{
				if (c->command->texture.IsNotEmpty())
				{
					float2 world = c->ToFloatCoords(cfg);
					float3 at;
					camera.Project(float3(world,0),at);
					at.xy = at.xy*0.5f + 0.5f;
					auto&cmd = *c->command;

					float4 code;
					if (cmd.state & Objects::CommandFlags::IsInvisible)
						continue;
					{
						if (!GUI::IsModal() && c->Contains(Engine::mouse.location.windowRelative))
						{
							code.r = 1.f;
							if (stationary)
							{
								hoveringOver = c;
							
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

			}

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
				display.Pick(UI::textoutCamera);

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
				Rect<> r;

				if (hoveringOver->flipX)
				{
					if (hoveringOver->flipY)
					{
						textout.MoveTo(base - float2(hoveringWidth,textout.GetScaledHeight()));
						r = Rect<>(base -float2(hoveringWidth,textout.GetScaledHeight() + h+textout.GetScaledHeight()),base + float2(0,-textout.GetScaledHeight()*0.8f));
					}
					else
					{
						textout.MoveTo(base + float2(-hoveringWidth,h + textout.GetScaledHeight()));
						r = Rect<>(base-float2(hoveringWidth,0) ,base + float2(0,h + textout.GetScaledHeight()*1.2f));
					}
				}
				else
				{
					base.x = std::min(base.x,1.f - hoveringWidth);

					if (hoveringOver->flipY)
					{
						textout.MoveTo(base - float2(0,textout.GetScaledHeight()));
						r = Rect<>(base -float2(0,textout.GetScaledHeight() + h+textout.GetScaledHeight()),base + float2(hoveringWidth,-textout.GetScaledHeight()*0.8f));
					}
					else
					{
						textout.MoveTo(base + float2(0,h + textout.GetScaledHeight()));
						r = Rect<>(base ,base + float2(hoveringWidth,h + textout.GetScaledHeight()*1.2f));
					}
				}

				//r.x.min = r.x.min * 0.5f + 0.5f;
				//r.x.max = r.x.max * 0.5f + 0.5f;
				//r.y.min = r.y.min * 0.5f + 0.5f;
				//r.y.max = r.y.max * 0.5f + 0.5f;
				//Rect<>r(0,0,1,1);

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

		Buffer0<Objects::Command::PVisualCommand>	allocatedEntityCommands;
			
		TGridConfig	visitCfg;

		void	VisitAddCommand(const int2&coords,const Objects::Command::PBase&cmd)
		{
			auto vcmd = To<Objects::Command::VisualCommand>::DynamicCast( cmd);
			if (!vcmd)
				return;
			bool isNew;
			GridCell&cell = AllocateCell(vcmd,isNew);
			cmd->Refresh(Network::ControlEnabled());
			cell.gridCoords = coords;
			cell.Update(visitCfg,camera);
			if (isNew)
				allocatedEntityCommands<<vcmd;
		}

		void RefreshButtonState()
		{
			foreach (allGuiCells,cell)
			{
				cell->command->Refresh(Network::ControlEnabled());
			}

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
			context.Refresh(cfg);
		}

		TGridConfig	GetGridConfig()
		{
			static const float CellSize = 0.075f;
			static const float stepY = (0.5f + 1.f) * CellSize;
			static const float stepX = 0.86602540378443864676372317075294f * CellSize;

			const float scale = UI::GetScale();
			TGridConfig rs;
			rs.cellSize = CellSize * scale;
			rs.stepX = stepX * scale;
			rs.stepY = stepY * scale;
			return rs;
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
				},true);
				CollectionToCommands(cfg,friendlyGroups);
				CollectionToCommands(cfg,hostileGroups);
			}
				


			#ifdef HAS_WINDOW
				if (hostileGroups.commands.IsNotEmpty())
					Tutorial::ShowMessage("Hostile Contacts");
			#endif

			animationTimeLine.Refresh(cfg);

			BottomRightPanel::Refresh(cfg);
			//display.SetDepthTest(false,false);
			//display.SetRasterizer(true,false);
		}

		void					ClearCommands()
		{
			foreach (allocatedEntityCommands,cmd)
				FindAndDeleteCell(*cmd);
			allocatedEntityCommands.Clear();
		}

		void					Configure(const Objects::PWorldEntity&e)
		{
			if (ConfigurationMode::ReconfigureVisualCommands())	//configuration mode has its own buttons; should not be removed while active
				return;
			const auto cfg = GetGridConfig();
			ClearCommands();

			bool showControls = showControlsAndContacts && e
					#ifndef DEBUG_ACCESS_RESTRICTIONS
						&& e->isOwnedByMe && e->health > 0
					#endif
					;

			visitCfg = cfg;
			if (showControlsAndContacts)
				Objects::globalCommands.VisitMembers(&VisitAddCommand,false);

			if (e)
			{
				hasFocusButton = true;
				if (showControls)
					e->VisitAllCommands(VisitAddCommand,false);

				FindAndDeleteCell(selectedFocus);
				selectedFocus.reset(new Custom::EntityFocusCommand(e) );
				selectedFocus->Refresh(true);//Network::ControlEnabled());
				selectedFocus->UpdateTexture(e,false,e->isOwnedByMe ? friendlyBackgroundColor : hostileBackgroundColor);

				GridCell&cell = AllocateCell(selectedFocus);
				cell.gridCoords = GetFocusLocation();
				cell.flipY = true;
				cell.flipX = true;
				cell.Update(cfg,camera);
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

		GlobalEvents::uiScaleChanged += OnScreenResize;	//mostly the same effect


		//camera.location = float3(0,-100,0);
		barProjectionCamera.retraction = float3(0,0,-120);
		barProjectionCamera.direction = float3(0,-1,0.3).Normalized();
		barProjectionCamera.upAxis = float3(0,0,1);
		barProjectionCamera.UpdateView();
		barProjectionCamera.UpdateProjection2(1.f,0.1f,1000,35.f);
		barProjectionCamera.depthTest = Engine::NoDepthTest;
		barProjectionCamera.region.x.max = 1.f / display.GetPixelAspect();

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
			gauges[i].Set(current.level[i],max.level[i],change.level[i],0,0,true);

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

	void	Update( TRAFFIC const Objects::ResourceVector&current, const Objects::ResourceVector&max, const Objects::ResourceVector&change,
					const Objects::ResourceVector&localCurrent/*, const Objects::ResourceVector&localChange, bool isEnabled*/)
	{
		#ifndef NO_HIBERNATION
			UpdateTraffic(traffic);
		#else
			showExtraGauges = true;
		#endif
		for (index_t i = 0; i < Objects::ResourceVector::NumEntries; i++)
//			resources[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],localChange.level[i],isEnabled);
			gauges[i].Set(current.level[i],max.level[i],change.level[i],localCurrent.level[i],0,true);
	}

	void	RenderHint()
	{
		HexGrid::RenderHint();
	}

	void	RenderCommands()
	{
		HexGrid::RenderCommands(HexGrid::GetGridConfig());
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
			display.Pick(textoutCamera);
				textout.PushState();
				textout.Scale(2.5f,2.5f);
				float lineHeight = textout.GetScaledHeight()*1.2f;
				SurfaceRenderer::SetColor(float4(0,0,0,0.6f));
				float w = 0.2f / display.GetPixelAspect();
				//textout.locate(-1.f + 0.01f,-1.f + textout.GetScaledHeight()*1.2f*(y+6));
				Rect<> t(-1,lineHeight*4.f-1.f,w*2.f - 1.f,lineHeight*11.5f - 1.f);
				SurfaceRenderer::Rasterize(
					float2(t.x.min,t.y.min),float4(0,0,0,0.8f),
					float2(t.x.max,t.y.min),float4(0,0,0,0.2f),
					float2(t.x.max,t.y.max),float4(0,0,0,0.2f),
					float2(t.x.min,t.y.max),float4(0,0,0,0.8f)
				);
				SurfaceRenderer::Flush();
				//textout.Scale(4.f,4.f);
				//textout.SetColor(1,1,1);

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

				textout.PopState();
		}
		else
		{

			barProjectionCamera.region.x.max = 1.f / display.GetPixelAspect();
			//(sin(Engine::timing.now)*0.5f+0.5f) + 0.5f;

			display.Pick(barProjectionCamera);



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

			display.Pick(textoutCamera);
				textout.PushState();
				textout.Scale(4.f,4.f);
				textout.SetColor(1,1,1);

				#ifndef NO_HIBERNATION
					trafficGauge.RenderText();
				#endif
				if (showExtraGauges)
					foreach (gauges,res)
					{
						res->RenderText();
					}

				textout.PopState();
			


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
//		float2 at = float2(textout.state.left+0.015,textout.state.top - textout.GetScaledHeight() * (float(textout.state.line)-0.5f));

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
		//;*2.f/display.GetPixelAspect();
	}


	static RoundNumber cachedNumber = RoundNumber(-1);
	static const Objects::WorldEntity *cachedEntity = nullptr;

	static float4	currentColor;

	static Buffer0<float4>	pushedColors;
	static void PushCurrentColor()
	{
		pushedColors << currentColor;
		ASSERT_LESS__(pushedColors.Count(),3);
	}
	static void PopCurrentColor()
	{
		ASSERT__(pushedColors.Count() != 0);
		currentColor = pushedColors.Pop();
	}



	struct TCachedLine
	{
		float4		color;
		String		text;

		/**/		TCachedLine()	{}
		/**/		TCachedLine(const String&str):text(str),color(currentColor){}
		/**/		TCachedLine(const StringRef&str):text(str),color(currentColor){}
		/**/		TCachedLine(String&&str):text(std::move(str)),color(currentColor){}
		/**/		TCachedLine(const char*str):text(str),color(currentColor){}

		void		swap(TCachedLine&other)
		{
			swp(color,other.color);
			text.swap(other.text);
		}
	};
	static Buffer0<TCachedLine,Swap>	cachedLines;

	void		SignalChange()
	{
		cachedEntity = nullptr;
	}


	void		UpdateCacheIfNeeded(const Objects::WorldEntity&e)
	{
		if (cachedNumber == RuntimeConfig::roundNumber && cachedEntity == &e)
			return;
		cachedNumber = RuntimeConfig::roundNumber;
		cachedEntity = &e;
		cachedLines.Clear();
		Lua::Reference researchState = e.GetResearchState();

		currentColor = float4(1);
		#ifndef NO_HIBERNATION
			const Objects::Hibernating*h = e.ToHibernating();
		#endif
			


		const String&name= e.GetNiceName();
		if (name.length() < 20)
			cachedLines << name;
		else
		{
			index_t split = name.Find('\'');
			if (split == InvalidIndex)
				split = name.Find(' ');
			cachedLines << name.subStringRef(0,split);
			cachedLines << String(' ')+name.SubStringRef(split);
				
			#ifdef _DEBUG
				cachedLines << ' '+e.GetName();

			#endif

		}

		#ifdef _DEBUG
			cachedLines << " Location: "+e.location.ToString();
			if (e.IsMobile())
				cachedLines << " Last point of origin: "+((const Objects::MobileEntity&)e).GetLastPointOfOrigin().ToString();
			cachedLines << " x: "+Vec::toString(e.system.x.xyz);
			cachedLines << " y: "+Vec::toString(e.system.y.xyz);
			cachedLines << " z: "+Vec::toString(e.system.z.xyz);
			cachedLines << " w: "+Vec::toString(e.system.w.xyz);
			TVec3<> wz = e.system.w.xyz;
			Vec::normalize(wz);
			cachedLines << " w1: "+Vec::toString(wz);
		#else
			cachedLines << " Height: "+String(e.location.height);
		#endif

		cachedLines << " Owner: "+e.owner->GetName()+(e.isOwnedByMe ? " (me)":"");
		if (e.isOwnedByMe)
		{
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
					PushCurrentColor();
					currentColor.rgb = float3(0.6,0.7,1.f);
					cachedLines << lastDeployed;

					PopCurrentColor();
				}
			}

		}
		cachedLines << "";
		cachedLines << " Scanning-Range: "+e.DescribeScanningRange();
		#ifndef NO_HIBERNATION
			cachedLines << " "<<Objects::TrafficResourceTag()  <<e.GetUplinkTraffic() << nl;
			if (h)
				cachedLines << " "<<Objects::TrafficResourceTag()  << "Priority: "<<h->GetUplinkPriority()<<nl;
		#endif
		PushCurrentColor();
		if (e.health > 0)
		{
			if (e.IsIndestructible())
				cachedLines << " Indestructible";
			else
			{
				int maxHealth = e.GetMaxHealth();
				if (e.health < maxHealth/4)
					currentColor.rgb &= float3(1,0.2,0.2);
				elif (e.health < maxHealth/2)
					currentColor.rgb &= float3(1,1,0.2);
				elif (e.health < maxHealth)
					currentColor.rgb &= float3(0.2,1,0.2);
	
				cachedLines << " Health: " + String(e.health) + "/" + e.DescribeMaxHealth();
			}
		}
		else
		{
			currentColor.rgb &= float3(1, 0.8, 0.75);
			cachedLines << " Ghost created " + String( -e.health )+ " round(s) ago";
		}
		PopCurrentColor();
		#ifndef NO_HIBERNATION
			if (h->IsHibernating())
			{
				color = float3(0.5f);
				cachedLines.SetColor(0.6,1,1);
				cachedLines << " Hibernating" << nl;
				cachedLines.SetColor(color);
			}
		#endif
		if (e.empDamageLevel)
		{
			PushCurrentColor();
			currentColor.rgb &= float3(1,0.8,0.75);
			cachedLines << " EMP-Damage detected: "+String(e.empDamageLevel);
			PopCurrentColor();
		}
	
	
		if (e.IsMobile())
		{
			const Objects::MobileEntity*v = (const Objects::MobileEntity*)(&e);
	
			//if (!v->storedIn.expired())
				//cachedLines << " Stored in: "<<v->storedIn.lock()->GetName()<<nl;	//probably never happens
			if (e.IsVehicle())
			{
				const Objects::Vehicle*veh = (const Objects::Vehicle*)(&e);
				cachedLines << " Turret-Overweight: "+String((int)vmax(0,veh->turret->weight(e.owner) - floor(veh->base->GetCarryingCapacity(e.owner))));
				cachedLines << " Movement Initiative: "+String((int)v->GetMovementInitiative())+"/"+String((int)veh->base->initiative);
			}
			#ifndef NO_SATELLITES
				elif (e.IsSatellite())
				{
					Objects::Satellite*sat = (Objects::Satellite*)(&e);
					int demand = sat->GetFuelConsumptionPerRound();
					if (!sat->RequiresFuel())
						demand = 0;
					cachedLines << " Fuel-Consumption: "<<(char)(tagOffset+Objects::Resource::Fuel)<<demand;
					if (demand > 0)
						cachedLines <<'+'<<sat->GetExtraFuelDemand();
							
					cachedLines<<nl;
				}
			#endif
			cachedLines << " Attack Initiative: "+String((int)v->GetAttackInitiative())+"/"+String((int)v->turret->initiative);
			cachedLines << " Speed: " + v->DescribeMovementStepsPerRound();
			if (v->turret->CanAttack())
			{
				//cachedLines << " Range: "<<v->GetAttackRange() << nl;
				//cachedLines << " Damage: "<<v->turret->damageLevel << nl;
				//cachedLines << " Ammo-Usage: "<<v->turret->ammoNeedPerShot<<nl;
				cachedLines << " Hrz. Range: "+v->turret->horizontalAttackRange.Describe(researchState)
								+'+'+v->turret->explosionRadius.Describe(researchState);
				cachedLines << " Damage: "+String(v->turret->GetDamageLevel(e.owner));
				int ammoNeed = v->turret->GetAmmoNeedPerShot(e.owner);
				if (ammoNeed > 0)
					cachedLines << " "+String((char)(tagOffset+v->turret->ammoType))+"Ammo-Usage: "+String(ammoNeed);
	
			}
			if (v->turret->CanAttack())
			{
				cachedLines << " Attacks left: "+String(int(v->CountPossibleAttacks()))+"/"+String(v->GetAttacksPerRound());
				if (v->turret->ignoresVehicleMotion)
					cachedLines << "  (ignores movement)";
			}
			if (v->turret->CanDo(Objects::Component::Construct))
			{
				String opSpeed = v->turret->operationSpeed[Objects::Component::Construct].Describe(v->GetResearchState());
				cachedLines << " Converts "+String(Objects::ResourceTag(Objects::Resource::Minerals))+opSpeed+" per round";
			}
			if (v->turret->CanDo(Objects::Component::Repair))
			{
				String opSpeed = v->turret->operationSpeed[Objects::Component::Repair].Describe(v->GetResearchState());
				cachedLines << " Repairs "+String(Objects::ResourceTag(Objects::Resource::Minerals))+opSpeed+" per round";
			}
	
			cachedLines << " Tasks:";
			if (v->myTasks.IsEmpty())
			{
				if (v->unstable)
					cachedLines << "  Operation done. Relocate to complete";
				else
					cachedLines << "  idle";
			}
			else
			{
				PushCurrentColor();
				currentColor.rgb &= float3(0.75,0.8,1);
				foreach (v->myTasks,t)
				{
					cachedLines << "  * "+t->name;
					if (t->IsMode(Objects::CursorMode::AttackMode) && t->foci.Count() > 1)
						cachedLines.Last().text+= " (x"+String(t->foci.Count())+')';
					if (t->job && t->job->roundsToJobCompletion)
					{
						cachedLines.Last().text+= " ("+String(t->job->roundsToJobCompletion)+" round"+(t->job->roundsToJobCompletion != 1 ? "s)" : ")");
					}
				}
				PopCurrentColor();
			}
			#ifndef ONE_COMPOUND_PER_FACTION
				const Objects::ResourceVector max = v->GetMaxStorageCapacity();
				bool any = false;
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
					if (max.level[i] > 0)
						any = true;
				if (any)
				{
	
					cachedLines << "Resources in storage:";
					for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
						if (max.level[i] > 0)
						{
	
	//						cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<v->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(v->currentStorage.level[i])+"/"+String(max.level[i]);
							//cachedLines << "        : "<<v->currentStorage.level[i]<<"/"<<max.level[i]<<nl;
						}
				}
			#endif
		}
	
		bool storeListed = false;
	
		if (e.IsStructure())
		{
			#ifdef _DEBUG
			{
				cachedLines << " Connected to: ";
				const Objects::Structure&s = (const Objects::Structure&)e;
				foreach (s.connectedTo,c)
					cachedLines << "  "+c->lock()->GetName();
			}
			#endif
			const Objects::Tower*t = dynamic_cast<const Objects::Tower*>(&e);
			if (t)
			{
				cachedLines << " Attack Initiative: "+String((int)t->turret->initiative);
				if (t->turret->CanAttack())
				{
					const Objects::Tower*v = t;	//rename
					cachedLines << " Hrz. Range: "+t->DescribeFlatAttackRange();
					cachedLines << " Damage: "+t->turret->damageLevel.Describe(e.GetResearchState());
					cachedLines << " "+String((char)(tagOffset+v->turret->ammoType))+"Ammo-Usage: "+String(v->turret->ammoNeedPerShot.Describe(e.GetResearchState()));
				}
				if (t->turret->CanAttack())
					cachedLines << " Attacks left: "+String(int(t->CountPossibleAttacks()))+"/"+String(t->turret->attacksPerRound.Describe(e.GetResearchState()));
				//cachedLines << " Tasks:" << nl;
	
				//cachedLines << "Resources in storage:" << nl;
				//const Objects::ResourceVector max = t->GetMaxStorageCapacity();
				//for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				//	if (max.level[i] > 0)
				//		cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<t->storage.level[i]<<"/"<<max.level[i]<<nl;
			
			}
	
	
			const Objects::VehicleStore*st = dynamic_cast<const Objects::VehicleStore*>(&e);
			if (st)
			{
				storeListed = true;
				cachedLines << " Max Vehicles per Store: "+String(Objects::VehicleStore::MaxVehiclesPerStore);
	
	
				if (st->park)
				{
					cachedLines << " Vehicles stored in park ("+String(st->park->vehiclesStored)+"/"+String(st->GetParkVehicleCapacity())+"):";
					PushCurrentColor();
					currentColor.rgb &= float3(0.8f,0.9f,1.1f);
					count_t cnt = 0;
	
					{
						const Objects::VehicleStore *other = st;
						for (const auto&v : other->stored)
						{
							Objects::PMobileEntity v1 = v.entity.lock();
							ASSERT1__(v1,st->ToEntity().id);
							if (++cnt > 10)
							{
								cachedLines << "...";
								break;
							}
							cachedLines << "  "+v1->GetNiceName() +" (local)";
						}
					}
	
					currentColor.rgb &= float3(0.9f,0.9f,0.9f);
	
					for (const auto&_s : st->park->members)
					{
						Objects::PVehicleStore other = _s.lock();
						if (other.get() == st)
							continue;
						for (const auto&v : other->stored)
						{
							Objects::PMobileEntity v1 = v.entity.lock();
							ASSERT1__(v1,st->ToEntity().id);
							if (++cnt > 10)
							{
								cachedLines << "...";
								break;
							}
							cachedLines << "  "+v1->GetNiceName();
						}
						if (cnt > 10)
							break;
					}
					PopCurrentColor();
				}
				else
				{
					cachedLines << " Vehicles stored ("+String(st->stored.Count())+"/"+String(Objects::VehicleStore::MaxVehiclesPerStore)+"):";
					for (const auto&v : st->stored)
					{
						Objects::PMobileEntity v1 = v.entity.lock();
						ASSERT1__(v1,st->ToEntity().id);
						cachedLines << "  "+v1->GetNiceName();
					}
				}
			}
	
	
			const Objects::Structure*s = (const Objects::Structure*)(&e);
	
			#if defined _DEBUG && !defined ONE_COMPOUND_PER_FACTION
				cachedLines << " Compound ID: "+String(s->compoundID);
			#endif
			const Objects::Compound*net = nullptr;
			if (s->health > 0)
			{
				const Objects::Compound&network = s->GetCompound();
				net = &network;
				cachedLines << " Compound status:";
				if (!network.stable)
				{
					PushCurrentColor();
					if (e.isOwnedByMe)
						currentColor.rgb &= float3(1, 0.2, 0.2);
	
					cachedLines << "  UNSTABLE";
					if (!e.isOwnedByMe)
						cachedLines.Last().text += " (?)";
					PopCurrentColor();
				}
				cachedLines << "  Resource consumption:";
				cachedLines << "    (demand/supply)";
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				{
					PushCurrentColor();
					if (e.isOwnedByMe && network.total.demand.level[i] > network.total.supply.level[i])
						currentColor.rgb &= float3(1, 0.2, 0.2);
					cachedLines << "   " + String((char)(tagOffset+i))+ " " +String(network.total.demand.level[i])+  "/" +String( network.total.supply.level[i] );
					PopCurrentColor();
				}
				cachedLines << "  Resource storage:";
				for (index_t i = 0; i < Objects::Resource::NumberOfPossibilities; i++)
				{
					if (network.total.storageMax.level[i] > 0)
						cachedLines << "   " +String( (char)(tagOffset+i) ) + " " + String(network.total.storage.level[i]) + "/" + String( network.total.storageMax.level[i]);
				}
			}
			else
			{
				//cachedLines << " Ghost created in round "<<
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
						cachedLines << "  producing "+ps->queue.First().name;
						if (!active)
							cachedLines << "   completion (when enabled) in "+String(Objects::TimeResourceTag())+String(ps->productionRoundsLeft);
						else
							cachedLines << "   completion in "+String(Objects::TimeResourceTag())+String(ps->productionRoundsLeft);
					}
	
				}
				else
				{
	
	
				}
			}
	
			if (e.IsWorkingStructure())
			{
				const Objects::WorkingStructure*ws = (const Objects::WorkingStructure*)(&e);
				cachedLines << "";
				cachedLines << " Structure status:";
				if (ws->IsHibernating())
					cachedLines << "  Hibernating";
	
				if (ws->AwaitsUserInteraction())
					cachedLines << "  Awaiting user input";
				else
					cachedLines << (ws->IsEnabled() ? "  Enabled" : "  Disabled");
					
	
	
				{
							
					if (ws->IsMine())
					{
						const Objects::Mine*ms = (const Objects::Mine*)(ws);
						cachedLines << "  Efficiency: "+String(  int(ms->GetEfficiency()*100) )+ "%";
						if (ms->IsLinkedToCore())
							cachedLines << "  Linked to Core" ;
						else
						{
							PushCurrentColor();
							if (e.isOwnedByMe)
								currentColor.rgb &= float3(1,0.5,0.4);
							cachedLines << "  Disconnected from Core";
							if (!e.isOwnedByMe)
								cachedLines.Last().text += " (?)";
							PopCurrentColor();
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
									cachedLines << "  Remaining resistence: "+String(np->resistence);
								else
									cachedLines << "  Captured";
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
					cachedLines << " Consumes";
					if (!isEnabled)
						cachedLines.Last().text += " (if enabled)";
					cachedLines.Last().text +=':';
					if (!isEnabled)
						cachedLines << "    (demand/compound supply)";
						
	
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] < 0)
						{
							//cachedLines << "  "<<Objects::Resource::Reinterpret(i).ToString()<<": "<<-delta.level[i];
							if (isEnabled)
								cachedLines << "  "+String((char)(tagOffset+i))+" "+String(-delta.level[i]);
							else
							{
								PushCurrentColor();
								if (net && delta.level[i] + net->total.balance.level[i] < 0)
									currentColor.rgb &= float3(1,0.6,0.5);
								cachedLines << "  "+String((char)(tagOffset+i))+" "+String(-delta.level[i]);
									
								if (net)
								{
									cachedLines.Last().text += " / " + String( net->total.balance.level[i] );
									if (net->total.storage.level[i])
										cachedLines.Last().text += " (" + String(net->total.storage.level[i])+ " stored)";
								}
								//cachedLines << nl;
								PopCurrentColor();
	
							}
						}
				}
				any = false;
				for (index_t i = 0; i < delta.NumEntries; i++)
					if (delta.level[i] > 0)
						any = true;
				const Objects::Laboratory*lab = dynamic_cast<const Objects::Laboratory*>(ws);

				if (any || lab)
				{
					cachedLines << " Produces";
					bool isEnabled = ws->IsEnabled();
					if (!isEnabled)
						cachedLines.Last().text += " (if enabled)";
					cachedLines.Last().text +=':';
					if (lab)
						cachedLines << "  Research: "+String(lab->GetTotalResearchOutput());
				}
				if (any)
				{
					//cachedLines << " Produces:"<<nl;
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (delta.level[i] > 0)
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(delta.level[i]);
				}
				any = false;
				for (index_t i = 0; i < delta.NumEntries; i++)
					if (storageMax.level[i] > 0)
						any = true;
				if (any)
				{
					cachedLines << " Storage:";
					for (index_t i = 0; i < delta.NumEntries; i++)
						if (storageMax.level[i] > 0)
							cachedLines << "  "+String((char)(tagOffset+i))+" "+String(storage.level[i])+"/"+String(storageMax.level[i]);
				}
			}
	
	
		
		}
	
	
		if (!storeListed && e.IsHangar())
		{
			const Objects::Hangar&h = *dynamic_cast<const Objects::Hangar*>(&e);
			PushCurrentColor();
			currentColor.rgb &= float3(0.75,0.8,1);
			cachedLines << " Units in storage ("+String(h.stored.Count())+"/"+String(h.GetMaxStorage())+"):";
			for (const auto&v : h.stored)
			{
				Objects::PMobileEntity v1 = v.entity.lock();
				ASSERT1__(v1,h.ToEntity().id);
				cachedLines << "  "+v1->GetNiceName();
			}
			PopCurrentColor();
	
		}

		

	}

	void		Render(const Objects::WorldEntity&e)
	{

		UpdateCacheIfNeeded(e);
			
		//textout.GetActiveFont()->EndTagMode();
		textout.PushState();

		float len = 300.f / display.GetClientWidth();
		//if (len == 0)
			//len = textout.GetScaledWidth("012345678901234567890123456789012345");
		textout.MoveTo(1.f - len,0.95f);

		foreach (cachedLines,l)
		{
			textout.SetColor(l->color);
			textout << l->text << nl;
		}

		textout.PopState();
	
	}


}
