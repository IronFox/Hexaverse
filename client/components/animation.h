#ifndef animationComponentH
#define animationComponentH

#include <container/buffer.h>
#include <math/vclasses.h>
#include <objects.h>
#include <protocol.h>
#include "../ExtRenderer/DirectX11.h"
#include "animation/abstractEvent.h"

/**
All animation-related aspects. Animation defines all processes involved when the server signals animation phase begin.
It also handles state updates from server messages
*/
namespace Animation
{




	extern float						atTime /*= 0.f*/;



	void		Create();
	void		Destroy();

	void		UpdateAutomation(const TCodeLocation&loc);

	#if 0
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
		typedef Buffer0<Protocol::WorldDelta,Swap>	DeltaField;
		std::shared_ptr<DeltaField >	worldDelta;
		Objects::PWorldEntity		allocatedTarget;	//for construction and motion. not to be used for other purposes as the original reference by id may change during animation
		std::shared_ptr<Protocol::MobileState>		vehicleState;
		std::shared_ptr<Protocol::StructureState>	structureState;
		std::shared_ptr<Protocol::ResourceUpdate>	resourceUpdate;

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
	#endif



	typedef M::TFloatRange<>	TTime;

	void		ScheduleEvent(const TTime&, const PEvent&);
	void		ScheduleEventWhenDone(const PEvent&);
	PEvent		FindFirstScheduledEventAtTime(const Protocol::Timestamp&st,const std::function<bool(const PEvent&)>&predicate);
	count_t		CountScheduledEvents(const std::function<bool(const PEvent&, const TTime&t)>&predicate);
	PEvent		FindScheduledEvent(const std::function<bool(const PEvent&, const TTime&t)>&predicate);
	bool		AlterScheduledEvent(const std::function<bool(const PEvent&, const TTime&t)>&predicate, const std::function<void(const PEvent&ev, TTime&t)>&updater);
	bool		AllDone();
	void		Flush();
	#ifdef HAS_WINDOW
		void	RenderActiveEvents();
	#endif



	template <typename T, typename...P>
	void		SmartSchedule( Protocol::Timestamp st, P&...param)
	{
		if (st.rightNow && AllDone())
		{
			T temp{param...};
			LogEvent(EventSignificance::Debug, "Animation: Executing '"+temp.Describe()+"'...");
			temp.Begin(true);
			temp.End();
			return;
		}
		auto ev = PEvent(new T(param...));
		if (st.rightNow)
			ScheduleEventWhenDone(ev);
		else
			ScheduleEvent(M::FloatRange(st.atTime,st.atTime), ev);
	}


	template <typename T, typename...P>
	void		SmartSchedule( Protocol::TimeWindow tw, P&...param)
	{
		if (tw.begin.rightNow && AllDone())
		{
			T temp{param...};
			LogEvent(EventSignificance::Debug, "Animation: Executing '"+temp.Describe()+"'...");
			temp.Begin(true);
			temp.End();
			return;
		}
		auto ev = PEvent(new T(param...));
		if (tw.begin.rightNow)
			ScheduleEventWhenDone(ev);
		else
			ScheduleEvent(M::FloatRange(tw.begin.atTime,tw.begin.atTime+tw.duration), ev);
	}



}



#endif
