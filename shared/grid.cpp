#include <global_root.h>
#include "grid.h"
#include <fractal/segment_map.h>
#include <fractal/interface.h>
#include <math/complex.h>

/*static*/ UINT32			BaseGrid::IsolatedCornerIndex[IsolatedCornerCount];
/*static*/ bool				BaseGrid::CornerIsIsolated[12] = {false};


/*static*/ bool BaseGrid::Contains(const TCellNeighborhood&n, UINT32 cellID)
{
	for (int i = 0; i < n.numNeighbors; i++)
		if (n.neighborID[i] == cellID)
			return true;
	return false;
}

/*static*/ UINT32 BaseGrid::FindIntersection(const TCellNeighborhood&n0, const TCellNeighborhood&n1)
{
	for (int i = 0; i < n0.numNeighbors; i++)
	{
		UINT32 id = n0.neighborID[i];
		if (Contains(n1, id))
			return id;
	}
	return 0xFFFFFFFF;
}

/*static*/ UINT32 BaseGrid::RequireIntersection(const TCellNeighborhood&n0, const TCellNeighborhood&n1)
{
	UINT32 id = FindIntersection(n0, n1);
	ASSERT__(id != 0xFFFFFFFF);
	return id;
}


/*static*/ void	BaseGrid::RadialInterpolation(const float3&v0, const float3&v1, float at, float3&out)
{
	TVec3<> axis;
	Vec::cross(v0, v1, axis);
	float a0 = Vec::angle(v0, v1);
	//float angle = a0 * M_PIF/180.f;
	Vec::normalize(axis);
	TMatrix3<> rot;
	Mat::rotationMatrix(-a0*at, axis, rot);
	Mat::mult(rot, v0, out);
}




/*static*/ void	BaseGrid::RadialInterpolation(const float3&base, const float3&vx, const float3&vy, float xat, float yat, float3&out)
{

	//float4	q0,q1,q;
	//float3	axis0,axis1;
	//Obj::triangleNormal(base,Vector<>::zero,vx,axis0);
	//Obj::triangleNormal(base,Vector<>::zero,vy,axis1);
	//Vec::normalize(axis0);
	//Vec::normalize(axis1);
	//float angle0 = Vec::angle(base,vx);
	//float angle1 = Vec::angle(base,vy);

	//float3 v0 = axis0 * angle0 * xat;
	//float3 v1 = axis1 * angle1 * yat;
	//float3 v = v0 + v1;
	//TMatrix3<> rot;
	//Mat::rotationMatrix(v.length(),v.Normalized0(),rot);
	//Mat::mult(rot,base,out);

	//ComplexMath::_q2Quaternion(angle0,axis0,q0);
	//ComplexMath::_q2Quaternion(angle1,axis1,q1);

	//q = q0 * xat + q1 * yat;
	//q.Normalize();

	//ComplexMath::_qRotateCCW(q,base,out);


	float3 summed;
	float3 a,b;

	RadialInterpolation(base, vy, yat, a);
	RadialInterpolation(vx, vy, yat, b);

	RadialInterpolation(a, b, xat / (1.f - yat), out);
	summed += out;


	RadialInterpolation(base, vx, xat, a);
	RadialInterpolation(vy, vx, xat, b);

	RadialInterpolation(a, b, yat / (1.f - xat), out);
	summed += out;

	out = summed/2;

	/*
	geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]], geometry.vertexField[geometry.triangleField[faceIndex * 3 + 2]], geometry.vertexField[geometry.triangleField[faceIndex * 3 + 0]], fx, fy, out);


			//float3 dx = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 2]] - geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]];
			//float3 dy = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 0]] - geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]];
			//float3 out;
			//out = geometry.vertexField[geometry.triangleField[faceIndex * 3 + 1]] + dx * fx + dy * fy;
			//if (normalize)
			//	Vec::normalize(out);
			return out;
		*/

}
/*static*/ void BaseGrid::UpdateRadii(Array<TNetCell>&cells)
{
	foreach(cells, cell)
	{
		float r = std::numeric_limits<float>::max();
		for (int j = 0; j < cell->numCorners; j++)
			cell->cornerCenter += cell->cornerVertex[j].xyz;
		cell->cornerCenter.Normalize();
		//cell->cornerCenter /= cell->numCorners;

		//BaseGrid::TNetCell&cell = cells[i];
		for (int j = 0; j < cell->numCorners; j++)
			r = std::min(r, Vec::quadraticDistance(cell->cornerCenter, cell->cornerVertex[j].xyz));
		cell->minCornerDistance = sqrt(r);
	}
}


static void Relax(BaseGrid::TCell*cells, count_t numCells)	//dont work well
{
	return;
	Timer::Time t0 = timer.Now();

	float radius = 0.f;
	for (index_t i = 0; i < numCells; i++)
	{
		const BaseGrid::TCell&cell = cells[i];
		float3 sum;
		for (int j = 0; j < cell.neighborhood.numNeighbors; j++)
			sum += cells[cell.neighborhood.neighborID[j]].center;
		//sum /= cell.neighborhood.numNeighbors;
		Vec::normalize(sum);
		float r = 0;
		for (int j = 0; j < cell.neighborhood.numNeighbors; j++)
			r += Vec::distance(cells[cell.neighborhood.neighborID[j]].center,sum);
		radius += r / cell.neighborhood.numNeighbors;
	}

	radius /= numCells;
	//ShowMessage(radius);

	Array<float3>	next(numCells);

	for (index_t generation = 0; generation < 100; generation++)
	{
		for (index_t i = 0; i < numCells; i++)
		{
			next[i] = cells[i].center;
		}
		for (index_t i = 0; i < numCells; i++)
		{
			 BaseGrid::TCell&cell = cells[i];
			//float3 sum;
			//for (int j = 0; j < cell.neighborhood.numNeighbors; j++)
			//	sum += cells[cell.neighborhood.neighborID[j]].center;
			//Vec::normalize(sum);
			//Vec::scale(sum, 1.5f, next[i]);
			//Vec::normalize(next[i]);
			//next[i] = sum;
			//float r = 0;
			for (int j = 0; j < cell.neighborhood.numNeighbors; j++)
			{
				float dist = Vec::distance(cells[cell.neighborhood.neighborID[j]].center, next[i]);
				float change = (radius ) / dist;
				if (change > 1)
				{
					change = pow(change, 0.2f);
					float3 v = cell.center;
					Vec::scale(cells[cell.neighborhood.neighborID[j]].center, change, v);
					next[i] += v - cell.center;
				}

			}
			Vec::normalize(next[i]);

			//	r += ;
			//r /= cell.neighborhood.numNeighbors;
			//float stretch = (radius - r) / r;
			//for (int j = 0; j < cell.neighborhood.numNeighbors; j++)
			//{
			//	float3 delta = (cells[cell.neighborhood.neighborID[j]].center - sum) * stretch;
			//	next[cell.neighborhood.neighborID[j]] += delta;
			//	//Vec::scale(sum, next[cell.neighborhood.neighborID[j]], stretch);
			//}
			//next[i] = sum / cell.neighborhood.numNeighbors;
		}
		for (index_t i = 0; i < numCells; i++)
		{
			cells[i].center = next[i];
		}

	}
	//ShowMessage(timer.GetSecondsSince(t0));


}
