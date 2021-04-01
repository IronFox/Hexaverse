

function Sqr(x)
	return x*x;
end;

function QuadraticDistance(u,v)
	return Sqr(u.x - v.x) + Sqr(u.y - v.y) + Sqr(u.z - v.z);
end;

function ToSuborbit(loc --[[Loc]]) --[[Loc]]
	return Game.Grid.ToLocation(loc.cell,Game.Grid.suborbitalHeight);
end;

function Seeing(observer --[[Unit]], observed --[[Loc]]) --[[bool]]
	if (observed.isSuborbital) then
		if (observer.location.isSuborbital) then
			local distance = QuadraticDistance(observer.location.world, observed.world);
			return distance < Sqr(observer.scanningRange);
		else
			if (observer.suborbitalScanningRange ~= nil) then
				local distance = QuadraticDistance(ToSuborbit(observer.location).world, observed.world);
				return distance < Sqr(observer.suborbitalScanningRange);
			end;
			return false;
		end;
	else
		if (observer.location.isSuborbital) then
			
			local distance = QuadraticDistance(observer.location.world, observed.world);
			if (distance < Sqr(observer.scanningRange)) then
				return true;
			end;
			if (observer.groundScanningRange ~= nil) then
				distance = QuadraticDistance(observer.location.world, ToSuborbit(observed).world);
				return distance < Sqr(observer.groundScanningRange);
			end;
			return false;
		else
			local distance = QuadraticDistance(observer.location.world, observed.world);
			return distance < Sqr(observer.scanningRange);
		end;
	end;
end;
