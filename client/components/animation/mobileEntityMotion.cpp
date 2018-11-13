#include "mobileEntityMotion.h"
#include <expectedBehavior.h>
#include "delayedMobileInsertion.h"

namespace Animation
{

	void MobileEntityMotion::Path::Rebuild(const Objects::PMobileEntity&target)
	{
		changed = false;

		nodes.SetSize(rawPath.Count());
		for (index_t i = 0; i < rawPath.Count(); i++)
		{
			const Grid::LCellIdI&a = rawPath[i].first;
			nodes[i].position = Grid::low.cells[a.stackID].centerVertex.xyz * Grid::ToRadius(a.height);
		}

		for (index_t i = 0; i+1 < rawPath.Count(); i++)
		{
			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = rawPath[i].first,
								&b = rawPath[i+1].first;
			const float3&p0 = na.position;
			const float3&p1 = nb.position;
			M::TVec3<> n,d = p1 - p0;
			Vec::cross(p0,d,n);
			na.normal += n;
			nb.normal += n*0.9f;
			na.dir += d;
			nb.dir += d*0.9f;
		}
		ASSERT__(nodes.IsNotEmpty());	//this shouldn't be possible
		{
			//Vec::makeNormalTo(nodes.First().dir,nodes.First().position);
			Vec::makeNormalTo(nodes.Last().dir,nodes.Last().position);
		}
		if (target->IsVehicle())
			nodes.First().dir = -target->system.y.xyz;
		foreach (nodes, n)
		{
			Vec::setLen0(n->dir,Objects::vehicleScale);
			Vec::setLen0(n->normal,Objects::vehicleScale);
		}
	}

	void MobileEntityMotion::Path::ApplyTo(const Objects::PMobileEntity&target, float t)
	{
		ASSERT__(!changed);


		//String out;
		//foreach (rawPath,p)
			//out += ","+String(p->second);
		//LogEvent(EventSignificance::Debug,target.get()," Applying at "+String(t)+" in {"+out+"}");

		for (index_t i = 0; i+1 < rawPath.Count(); i++)
		{
			//if (t < rawPath[i].second)
			//{
				//return;
			if (t > rawPath[i+1].second)
				continue;

			TNode	&na = nodes[i],
					&nb = nodes[i+1];

			const Grid::LCellIdI&a = rawPath[i].first,
								&b = rawPath[i+1].first;
			const M::TVec3<>&d0 = Grid::low.cells[a.stackID].cornerCenter; //centerVertex.xyz;
			float3 p0 = d0  * Grid::ToRadiusf(float(a.height));
			const M::TVec3<>&d1 = Grid::low.cells[b.stackID].cornerCenter;// centerVertex.xyz;
			float3 p1 = d1 * Grid::ToRadiusf(float(b.height));

			float3 c0 = p0 + na.dir;
			float3 c1 = p1 - nb.dir;

			float relative = std::max(0.f,(t - rawPath[i].second) / (rawPath[i+1].second - rawPath[i].second));

			//LogEvent(EventSignificance::Debug,target.get()," Relative ("+String(i)+"/"+String(rawPath.Count())+")="+String(relative));
			//!((i+1)% stepsPerRound);

			//M::TVec3<> d = p1 - p0;
			//if (a.height == b.height)

			
			float fj = relative;

			M::TVec3<> p;
			Vec::resolveBezierCurvePoint(p0,c0,c1,p1,fj,p);

			M::TMatrix4<>&sys = target->system;
			#ifdef HAS_WINDOW
				const M::TVec3<> prev = sys.w.xyz;
			#endif
			if (!target->IsVehicle())
			{
				sys.z.xyz = p;
				Vec::normalize0(sys.z.xyz);

				Vec::cross(sys.y.xyz,sys.z.xyz,sys.x.xyz);
				Vec::normalize0(sys.x.xyz);
				Vec::cross(sys.z.xyz,sys.x.xyz,sys.y.xyz);
				Vec::normalize0(sys.y.xyz);
			}
			else
			{
				M::TVec3<> n,d,axis;
				Vec::resolveBezierCurveAxis(p0,c0,c1,p1,fj,axis);
				Vec::normalize0(axis);
				axis = -axis;

				Vec::cross(axis,p,n);
				Vec::normalize0(n);
				//target->Relocate(rawPath[i].first,false);
				target->location = rawPath[i].first;
				sys.x.xyz = n;
				sys.y.xyz = axis;
				Vec::cross(n,axis,sys.z.xyz);
				Vec::normalize0(sys.z.xyz);
			}
			#ifdef HAS_WINDOW
				target->ambientSound.SetPosition(sys.w.xyz);
				target->ambientSound.SetVelocity((sys.w.xyz - prev) / Engine::timing.delta);
				float x = t / rawPath.Last().second;
				float activity = 1.f - (M::Sqr(((x - 0.5f)*2.f)));
				target->ambientSound.SetPitch(1.f + activity * 0.5f);
				target->ambientSound.SetGain(std::min(target->GetSoundLevel() + activity,1.f));
				target->activitySound.SetPosition(sys.w.xyz);
				target->activitySound.SetVelocity((sys.w.xyz - prev) / Engine::timing.delta);
			#endif

			target->system.w.xyz = p;
			Mat::resetBottomRow(target->system);

			const float scale = Objects::WorldEntity::GetScaleAtRadius(Vec::length(target->system.w.xyz));

			Mat::invertSystem(target->system, target->inverse);
			target->system.x.xyz *= scale;
			target->system.y.xyz *= scale;
			target->system.z.xyz *= scale;
			target->inverse.x.xyz /= scale;
			target->inverse.y.xyz /= scale;
			target->inverse.z.xyz /= scale;
			target->inverse.w.xyz /= scale;
			return;
		}
	}


	/**/					MobileEntityMotion::MobileEntityMotion(Protocol::MobileEntityMotion&m):Super(m.timestamp,m.timeNeeded,m.id)
	{
		path.rawPath.Append(std::make_pair(m.fromLocation,m.timestamp.atTime));
		path.rawPath.Append(std::make_pair(m.toLocation,m.timestamp.atTime + m.timeNeeded));
		path.changed = true;
	}

	void					MobileEntityMotion::Append(Protocol::MobileEntityMotion&m)
	{
		EXPECT(path.rawPath.Last().first == m.fromLocation);
		EXPECT2(targetEntityID == m.id,targetEntityID,m.id);
		path.rawPath.Append(std::make_pair(m.toLocation,m.timestamp.atTime + m.timeNeeded));
		path.changed = true;
		time.Include(M::FloatRange<float>(m.timestamp.atTime,m.timestamp.atTime + m.timeNeeded));

	}

	/*virtual override*/ void MobileEntityMotion::Begin()
	{		
		auto e = Objects::GetEntity(targetEntityID);
		EXPECT1(e,targetEntityID);
		EXPECT1(e->IsMobile(),e->GetNameAndOwner());
		allocatedTarget = To<Objects::MobileEntity>::Cast(e);
		LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Starting movement stream at "+String(currentTime)+"/"+ToString(this->time));
		ASSERT1__(allocatedTarget,targetEntityID);
		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
		ASSERT1__(v,allocatedTarget->GetName());
		ASSERT1__(!v->isInMovementStream ,v->location);

		if (!insertIntoGridLater.IsSet(v->id))
		{
			Objects::RemoveFromGrid(v,v->location);
			insertIntoGridLater.Set(v->id);
		}
		v->isInMovementStream = true;

		path.Rebuild(v);
	}

	/*virtual override*/ void MobileEntityMotion::Update( float x )
	{
		path.ApplyTo(allocatedTarget,currentTime);
		DBG_ASSERT_LESS_OR_EQUAL__(currentTime,time.max);
	}


	/*virtual override*/ void MobileEntityMotion::End()
	{
		LogEvent(EventSignificance::Debug, allocatedTarget.get() ,"Ending movement stream at "+String(currentTime)+"/"+ToString(this->time));

		Objects::PMobileEntity v = To<Objects::MobileEntity>::DynamicCast(allocatedTarget);
		EXPECT(v);
		path.ApplyTo(v,path.rawPath.Last().second);
		//v->Relocate(path->rawPath.Last().first,false);
		v->location = path.rawPath.Last().first;
		//Objects::InsertVehicleIntoGrid(v,v->location);	will be done later
		if (v->owner->SharesSensorsWithMe())
		{
			float t = 0.f;
			v->MoveSensoryToCurrentLocation(t);
		}
		ASSERT1__(v->isInMovementStream,v->id);
		v->isInMovementStream = false;

		allocatedTarget.reset();	//for debugging purposes
		#ifdef HAS_WINDOW
			if (v->myTasks.IsNotEmpty())
			{
				auto&task = v->myTasks.Peek();
				while (task.foci.IsNotEmpty() && path.Contains(task.foci.Peek().target))
					task.foci.Pop();
				//this->path->nodes
				v->myTasks.Peek().Replot(v->location,v.get(),Objects::FocusChangeType::FirstSegmentChanged,true);
			}
		#endif
	}

	/*virtual override*/ D::String		MobileEntityMotion::Describe() const
	{
		return "MobileEntityMotion("+ToString(targetEntityID)+": ->"+ToString(path.rawPath.Last().first)+")";
	}


}
