#ifndef animationComponentH
#define animationComponentH

#include <container/buffer.h>
#include <math/vclasses.h>
#include <objects.h>
#include <protocol.h>
#include "../ExtRenderer/DirectX11.h"

/**
All animation-related aspects. Animation defines all processes involved when the server signals animation phase begin.
It also handles state updates from server messages
*/
namespace Animation
{


	typedef Buffer0<M::TVec4<> >	Trajectory;

	#ifdef HAS_WINDOW
		namespace DX11 = Engine::DX11;
		namespace OpenAL = Engine::OpenAL;
		struct TTrajectoryAnimation
		{
			Trajectory					activeTrajectory;
			Objects::Geometry			trajectoryGeometry;
			DX11::InstructionBlock32	instruction;
			float						maxProjectileProjection,projectileAt=0, beamHeight;
			M::TVec3<>					beamBase;
			OpenAL::Source				missileHissSource;
		};
	#endif

	extern float						atTime /*= 0.f*/;

	static const float ExplosionTime	 = 0.1f;


	bool		LocateProjectile(float3&outPosition, float3&out);
	void		Create();
	void		Destroy();
	void		AppendTask(const Objects::PVehicle&v, const Protocol::MobileTask*t);
	void		ApplyState(const Objects::PMobileEntity&v, Protocol::MobileState&vs, bool isNew, bool isNewForEMP);
	void		UpdateState(const Objects::PStructure&s,Protocol::StructureState&vs);
	void		ExplodeEntity(const Objects::PWorldEntity&target);
	void		ExplodeEntity(const Objects::PWorldEntity&target, const std::function<void()>&onQuarterDone);

	void		UpdateAutomation(const Objects::PVehicle&v);
	void		UpdateAutomation(const TCodeLocation&loc);


	struct Primitive
	{
		enum type_t
		{
			//Movement,	//handled separately
			Construction,
			NewMobileEntity,
			TurretRotation,
			Attack,
			Explosion,
			TerrainChange,
			MovementStream,
			MobileState,
			StructureState,
			DestroyEntity,
			ResourceUpdate,
		}						type;
		count_t					overwritten=0;
		//Grid::LCellIdI			location;
		typedef Buffer0<Protocol::WorldDelta,Swap>	DeltaField;
		std::shared_ptr<DeltaField >	worldDelta;
		M::TFloatRange<>			time;
		Objects::PWorldEntity	allocatedTarget;	//for construction and motion. not to be used for other purposes as the original reference by id may change during animation
		TUniqueID				targetEntityID;
		Objects::Component::Turret*		attackerTurret;	//reference targetEntityID to retrieve the identity of the attacker (precise attack only)
		float					attackRange,attackAngle;
		M::TVec3<>					attackerPosition;

		M::TVec3<>					explosionCenter;	//if Explosion
		float					explosionRadius;	//if Explosion
		bool					empExplosion;		//if Explosion
		std::shared_ptr<Protocol::MobileState>		vehicleState;
		std::shared_ptr<Protocol::StructureState>	structureState;
		std::shared_ptr<Protocol::ResourceUpdate>	resourceUpdate;
		#ifdef HAS_WINDOW
			std::shared_ptr<TTrajectoryAnimation>	trajectoryAnimation;
		#endif
		M::TVec2<>					rotateTo,rotateFrom;
		M::TVec3<>					rotateToWorld;
		union
		{
			bool				explode;
			bool				isAutoTarget;
		};
		UINT8					removalType;

		struct Path
		{
			Buffer0<std::pair<Grid::LCellIdI,float> >	rawPath;
			bool				changed;

			struct TNode
			{
				float3	dir,
						normal,
						position;
			};

			Array<TNode>		nodes;
			//TUniqueID			targetID;
		

			/**/				Path():changed(true)	{}

			void				Rebuild(const Objects::PMobileEntity&target);
			void				ApplyTo(const Objects::PMobileEntity&target, float t);
			bool				Contains(const Grid::LCellIdI&loc)	{foreach (rawPath,p) if (p->first == loc) return true; return false;}
		};

		
		std::shared_ptr<Path>	path;	//if MovementStream


		/**/					Primitive():time(M::FloatRange<float>(-1,-1)),explosionCenter(float3(-1)),explosionRadius(-1)
		{}


		void					Init(Protocol::WorldDelta&delta);

		void					Init(Protocol::StructureConstruction&sc, const Objects::PStructure&s);

		void					Init(Protocol::NewMobileEntity&sc, const Objects::PMobileEntity&v);
		void					Init(Protocol::MobileState&vs);
		void					Init(Protocol::StructureState&vs);
		void					Init(Protocol::MobileEntityMotion&motion/*, const Objects::PVehicle&v*/);
		void					Append(Protocol::MobileEntityMotion&motion);
		void					Init(Protocol::DestroyEntity&e);
		void					Init(Protocol::VagueAttackTrajectory&a);
		void					Init(Protocol::PreciseAttackTrajectory&a);
		void					Init(Protocol::Explosion&ex);

		void					Init(Protocol::EntityTurretRotation&rot);

		void					Init(Protocol::ResourceUpdate&up);
		enum result_t
		{
			Continue,
			Done

		};


		void					Begin();
		void					End();

		result_t				Update(float t);
		#ifdef HAS_WINDOW
			void				Render() const;
		#endif
	};



	extern Buffer0<Primitive>		primitives,actives,whenDone;

	inline bool AllDone()
	{
		return (primitives.IsEmpty() && actives.IsEmpty());
	}

}



#endif
