--https://www.lua.org/pil/11.4.html

Queue = {};

function Queue.New()
  return {first = 0, last = -1}
end


function Queue.PushLeft (queue, value)
	local first = queue.first - 1
	queue.first = first
	queue[first] = value
end

function Queue.PushRight (queue, value)
	local last = queue.last + 1
	queue.last = last
	queue[last] = value
end

function Queue.PopLeft (queue)
	local first = queue.first
	if first > queue.last then error("queue is empty") end
	local value = queue[first]
	queue[first] = nil        -- to allow garbage collection
	queue.first = first + 1
	return value
end

function Queue.PopRight (queue)
	local last = queue.last
	if queue.first > last then error("queue is empty") end
	local value = queue[last]
	queue[last] = nil         -- to allow garbage collection
	queue.last = last - 1
	return value
end

function Queue.Foreach(queue, func)
	for i = queue.first, queue.last do
		func(queue[i]);
	end;
end

function Queue.PushAllRight(queue, array)
	for _,it in pairs(array) do
		Queue.PushRight(queue,it);
	end;
end

function Queue.IsEmpty(queue)
	return queue.first > queue.last;
end;

function Queue.IsNotEmpty(queue)
	return queue.first <= queue.last;
end;


