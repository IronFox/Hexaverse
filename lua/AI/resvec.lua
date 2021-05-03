RV = {
	DivByZero = 1000000000	--returned when diving by zero in RV.Div
};
	
function RV.Add(a --[[ResVec]], b --[[ResVec]])	-- ResVec: Returns the sum of the local and given other resource vector
	local rs = {};
	for k,v in pairs(a) do
		rs[k] = v + b[k];
	end;
	return rs;
end;

function RV.Sub(a --[[ResVec]], b --[[ResVec]]) -- ResVec: Returns the difference of the local and given other resource vector
	local rs = {};
	for k,v in pairs(a) do
		rs[k] = v - b[k];
	end;
	return rs;
end;

function RV.Mul(a --[[ResVec]], factor--[[int]])  -- ResVec: Returns the product of the local resource vector and a given factor
	local rs = {};
	for k,v in pairs(a) do
		rs[k] = v * factor;
	end;
	return rs;
end;

function RV.LessOrEqual(a --[[ResVec]], b --[[ResVec]])	-- bool: Returns true if the local resource vector is less or equal than a given other vector in all resources
	for k,v in pairs(a) do
		if v > b[k] then
			return false;
		end;
	end;
	return true;
end;

function RV.GreaterOrEqual(a --[[ResVec]], b --[[ResVec]])	-- bool: Returns true if the local resource vector is greater or equal than a given other vector in all resources
	for k,v in pairs(a) do
		if v < b[k] then
			return false;
		end;
	end;
	return true;
end;


function RV.IsZero(a --[[ResVec]]) --bool
	for _,v in pairs(a) do
		if v ~= 0 then
			return false;
		end;
	end;
	return true;
end;


--[[
Div:
Calculates how often <b> fits into <a>.
Effectively the lowest resource division result, rounded towards zero.
If <b> is zero, the result is RV.DivByZero.
The result may be negative if the sign of <a> and <b> differ.
]]
function RV.Div(a --[[ResVec]], b --[[ResVec]]) --[[int]]
	local rs = RV.DivByZero;
	if RV.IsZero(a) then
		return 0;
	end;
	
	for k,v in pairs(a) do
		local other = b[k];
		if other ~= 0 then
			if math.abs(v) < math.abs(other) then
				return 0;
			end;
			local div = v / other;
			if v > 0 then
				v = math.floor(v);
			else
				v = math.ceil(v);
			end;
			
			if math.abs(div) < math.abs(rs) then
				rs = div;
			end;
		end;
	end;
	return rs;
end;


function RV.ToString(a --[[ResVec]]) --[[string]]
	local rs = '{';
	local first = true;
	for k,v in pairs(a) do
		if not first then
			rs = rs .. ',';
		end;
		first = false;
		rs = rs .. k .. ':' .. v;
	end;
	return rs .. '}';
end;
