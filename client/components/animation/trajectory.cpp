#include "trajectory.h"
#include <container/buffer.h>
#include <math/vclasses.h>
#include "../visual/hud/hexGrid.h"
#include "../objectNetworkHelper.h"

namespace Animation
{
	using namespace DeltaWorks;


	#ifdef HAS_WINDOW

		DX11::ObjectBuffer<float4>	trajectoryProgression,trajectoryTint;
		OpenAL::Wave				artilleryShot,missileHiss/*,constructSound*/,beamSound;

		DX11::BaseInstructionBlock	trajectoryVisualizationBase;

		namespace DX11 = Engine::DX11;
		namespace OpenAL = Engine::OpenAL;
	#endif


	/*static*/ void Trajectory::StaticCreate()
	{
		#ifdef HAS_WINDOW
			trajectoryTint.Create(float4(1));
			trajectoryProgression.Create(float4(1));
			DX11::Shader::Binding bindings[] =
			{
				DX11::Shader::Binding(trajectoryTint, "Tint"),
				DX11::Shader::Binding(trajectoryProgression, "Progression"),
			};
			trajectoryVisualizationBase.binding = Objects::stdVehicleShader.binding;
			trajectoryVisualizationBase.shader.LoadFromFile("shaders/projectilePath.hlsl", "vsMain", "psMain", bindings, ARRAYSIZE(bindings));
			trajectoryVisualizationBase.UpdateLayout();
			trajectoryVisualizationBase.SetTriangles();

			artilleryShot = OpenAL::Load("resources/objectSounds/artilleryShot.wav");
			missileHiss = OpenAL::Load("resources/objectSounds/missileHiss.wav");
			beamSound = OpenAL::Load("resources/objectSounds/beam.wav");
		#endif
	}

	/*static*/ void Trajectory::StaticDestroy()
	{
		#ifdef HAS_WINDOW
			trajectoryVisualizationBase = DX11::InstructionBlock32();
			trajectoryProgression.Clear();
			trajectoryTint.Clear();
			OpenAL::Discard(missileHiss);
			OpenAL::Discard(beamSound);
			OpenAL::Discard(artilleryShot);
		#endif
	}


	Trajectory::Trajectory(Protocol::VagueAttackTrajectory&a):Super(TUniqueID::Empty())
	{
		attackerTurret = Objects::Component::allTurretComponents.Find(a.turretType);
		ASSERT_NOT_NULL1__(attackerTurret,(int)a.turretType);
		attackRange = a.turretRange;
		attackAngle = a.attackAngle,
		attackerPosition = a.sourcePosition;
		explosionCenter = a.traceTo;
		explosionRadius = a.explosionRadius;
		isAutoTarget = false;
		eventTime = a.time.begin.atTime;
	}

	Trajectory::Trajectory(Protocol::PreciseAttackTrajectory&a):Super(a.attacker)
	{
		attackerTurret = nullptr;	//later
		attackAngle = a.attackAngle;
		explosionCenter = a.traceTo;

		isAutoTarget = a.isAutoAttack;
		eventTime = a.time.begin.atTime;
	}

	/*virtual override*/ D::String		Trajectory::Describe() const
	{
		return "Trajectory ("+(attackerTurret ? attackerTurret->typeName : "null")+",range="+String(attackRange)+",pos="+Vec::toString(attackerPosition)+",angle="+String(attackAngle)+")";
	}


	/*virtual override*/ void Trajectory::Begin()
	{
		//	allocatedTarget = Objects::GetEntity(targetEntityID);
		//if (allocatedTarget)
		bool isFriendly = false;
		bool doRegister = true;
		#ifdef HAS_WINDOW
			TUniqueID attackerID = TUniqueID::Empty();
		#endif

		Grid::LCellIdI attackerCell = Grid::LCellIdI::Invalid;
		if (!attackerTurret)
		{
			Objects::PWorldEntity attacker = Objects::GetEntity(targetEntityID);
			ASSERT1__(attacker,targetEntityID);
			attackerCell = attacker->location;
			float zOffset;
			Objects::PTurretBase base= attacker->GetTurretBase(zOffset);
			ASSERT1__(base,attacker->GetName());
			attackerTurret = base->turret;
			ASSERT_NOT_NULL1__(attackerTurret,attacker->GetName());
			attackRange = base->GetFlatAttackRange();
			explosionRadius = base->GetAttackExplosionRadius();
			attackerPosition = attacker->GetWorldPosition();
			attackerPosition += Grid::low.cells[attacker->location.stackID].cornerCenter * zOffset;
			#ifdef HAS_WINDOW
				attackerID = attacker->id;
			#endif
			doRegister = !attacker->isOwnedByMe || isAutoTarget;
			isFriendly = attacker->owner->IsAlliedWithMe();
		}
		{
			#ifdef HAS_WINDOW
				((DX11::BaseInstructionBlock&) trajectoryAnimation.instruction) = trajectoryVisualizationBase;
				Objects::Component::Turret::TraceAnimationTrajectory(
					attackerTurret->trajectoryType, attackRange,attackAngle, attackerPosition,explosionCenter,
					trajectoryAnimation.activeTrajectory);

				trajectoryAnimation.trajectoryGeometry.Clear();
				#ifdef _DEBUG
					LogEvent(EventSignificance::Debug, "Compiling trajectory with "+String(trajectoryAnimation.activeTrajectory.Count())+" steps, starting from "+Vec::toString(trajectoryAnimation.activeTrajectory.First())+", ending "+Vec::toString(trajectoryAnimation.activeTrajectory.Last()));
				#endif
				trajectoryAnimation.maxProjectileProjection = Objects::Component::Turret::CompileTrajectory(trajectoryAnimation.activeTrajectory,Objects::Component::Turret::Animation,trajectoryAnimation.trajectoryGeometry,0.25f);
				trajectoryAnimation.instruction.vbo.CreateFromStructs(trajectoryAnimation.trajectoryGeometry.GetLOD(0).vertices);
				trajectoryAnimation.instruction.ibo.Create(trajectoryAnimation.trajectoryGeometry.GetLOD(0).indices);

				if (attackerTurret->trajectoryType == Objects::TrajectoryType::VerticalBeam)
				{

					UINT32 fine;
					if (attackerCell != Grid::LCellIdI::Invalid)
						fine = Grid::low.grid.GetCell(attackerCell.stackID).higherEquivalent;
					else
						fine = Grid::low.Locate(attackerPosition);
					int h = World::actual.GetStack(fine).FindTopClearOverNonClear(true,false);
					if (h == InvalidIndex)
						h = Grid::CellStack::BottomHeight;
					const float rh = Grid::ToRadius(h);
					trajectoryAnimation.beamBase = Grid::high.cells[fine].cornerCenter * rh;
					trajectoryAnimation.beamHeight = Vec::length(attackerPosition) - rh;
				}


				auto smokeTrailIndex = Objects::Renderer::RegisterSmokeTrail(trajectoryAnimation.activeTrajectory,Vec::dot(attackerPosition) > M::Sqr(Grid::ToRadius(Grid::CellStack::TopHeight+1)));
				if (doRegister)
				{
					HUD::HexGrid::EventLog::RegisterAttack(eventTime, isFriendly,attackerPosition, attackerID, smokeTrailIndex);
				}

				switch (attackerTurret->trajectoryType)
				{
					case Objects::TrajectoryType::Artillery:
					{
						OpenAL::PlayOnce(attackerPosition,artilleryShot,1.f,10.f);
						//for (index_t i = 0; i < ARRAYSIZE(trajectoryFireSource); i++)
						//{
						//	OpenAL::Source&s = trajectoryFireSource[i];
						//	s.SetPosition(attackerPosition);
						//	s.SetWaveBuffer(artilleryShot);
						//	s.Play();
						//}
					}
					break;
					case Objects::TrajectoryType::Missile:
					{
						trajectoryAnimation.localizedSoundSource.Create();
						#ifdef _DEBUG
							trajectoryAnimation.localizedSoundSource.name = "localized attack sound";
						#endif
						trajectoryAnimation.localizedSoundSource.SetReferenceDistance(10.f,0.5f);
						trajectoryAnimation.localizedSoundSource.SetWaveBuffer(missileHiss);
						trajectoryAnimation.localizedSoundSource.SetLooping(true);
						trajectoryAnimation.localizedSoundSource.SetPosition(attackerPosition);
						trajectoryAnimation.localizedSoundSource.Play();
					}
					break;
					case Objects::TrajectoryType::VerticalBeam:
						OpenAL::PlayOnce(attackerPosition,beamSound,1.f,10.f);
					break;
				}

						

			#endif


		}
}

	/*virtual override*/ void Trajectory::Update(float x)
	{
		#ifdef HAS_WINDOW
			trajectoryAnimation.projectileAt = x;
			index_t at = std::min(index_t(x * trajectoryAnimation.activeTrajectory.GetLength()),trajectoryAnimation.activeTrajectory.GetLength()-1);
			const M::TVec4<>&v = trajectoryAnimation.activeTrajectory[at];
			trajectoryAnimation.localizedSoundSource.SetPosition(v.xyz * v.w);
			float volume = M::Sqr(M::Sqr(1.f - x))*0.75f + 0.25f;
			trajectoryAnimation.localizedSoundSource.SetPitch(0.75f + x);
		#endif
	}

	/*virtual override*/ void Trajectory::End()
	{
		for (index_t i = 0; i < Objects::Marker::all.Count(); i++)
		{
			const Objects::Marker&m = Objects::Marker::all[i];
			if (m.type != Objects::Marker::Type::HostileContact)
				continue;
			if (Vec::quadraticDistance(Grid::ToWorld(m.location),explosionCenter) < M::Sqr(explosionRadius))
			{
				LogEvent(EventSignificance::Common,nullptr,m.GetName()+ " considered near occuring explosion. Dropping marker");
				ObjectHelper::SignalMarkerDeletion(m.id);
				i--;
			}
		}
		#ifdef HAS_WINDOW
			trajectoryAnimation.localizedSoundSource.Stop();
		#endif
	}


	#ifdef HAS_WINDOW
		/*virtual override*/ void Trajectory::Render() const
		{
			if (attackerTurret->trajectoryType != Objects::TrajectoryType::VerticalBeam)
			{
				trajectoryProgression.Update(float4(trajectoryAnimation.projectileAt * trajectoryAnimation.maxProjectileProjection,1.f,5.f,1.f));
				display.Render(trajectoryAnimation.instruction);
			}
			else
			{
				const float f = trajectoryAnimation.projectileAt;
				static const float maxWidth = 0.125f;
				float w = maxWidth * (1.f - M::Sqr(1.f - f));
				float4 color = float4(1,f,f,f);
				Objects::Renderer::RenderBeam(trajectoryAnimation.beamBase,trajectoryAnimation.beamHeight,w,color,color);
			}
		}
	#endif

}



