ResearchState = {}
ResearchState.__index = ResearchState
function ResearchState.New()
	local rs = {}
	setmetatable(rs,ResearchState)
	return rs
end

function ResearchState:SelectUnlock(tech)
	print "hello"
end


UnlockProgress = {}
UnlockProgress.__index = UnlockProgress
function UnlockProgress.New()
	local rs = {}
	setmetatable(rs,UnlockProgress)
	rs.progress = 0;
	return rs
end





unlockGroup = API.CreateResearch(nil,"Unlock");

artillery = UnlockProgress.New();

API.CreateResearch(unlockGroup,"Artillery","Artillery",function(state) return state:SelectUnlock("Artillery") end);






--[[
Account = {}
Account.__index = Account

function Account.create(balance)
   local acnt = {}             -- our new object
   setmetatable(acnt,Account)  -- make Account handle lookup
   acnt.balance = balance      -- initialize our object
   return acnt
end

function Account:withdraw(amount)
   self.balance = self.balance - amount
end

-- create and use an Account
acc = Account.create(1000)
acc:withdraw(100)
]]--