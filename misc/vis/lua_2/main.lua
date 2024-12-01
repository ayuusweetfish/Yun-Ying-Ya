local program_t = 0
local program_r, program_g, program_b = 0, 0, 0

local delay = function (t)
  program_t = program_t + t / 1000
end

local output = function (r, g, b)
  if r == nil then
    return program_r, program_g, program_b
  end
  program_r, program_g, program_b = r, g, b
  coroutine.yield(program_t, r, g, b)
end

local load_duck = function (s)
  fenv = {
    math = math,
    os = { time = os.time },
    delay = delay,
    output = output,
  }
  local f, e = load(s .. '\nreturn light', 'duck')
  if e then print(e) end
  local f = setfenv(f, fenv)
  return f()
end

local duck_fn = load_duck(io.popen('pbpaste'):read('a'))
local co_duck = coroutine.create(function ()
  while true do duck_fn() end
end)

local T = 0
local t0, r0, g0, b0 = 0, 0, 0, 0
local t1, r1, g1, b1 = 0, 0, 0, 0

function love.update(dt)
  T = T + dt
  while T > t1 do
    t0, r0, g0, b0 = t1, r1, g1, b1
    local result
    result, t1, r1, g1, b1 = coroutine.resume(co_duck)
    if not result then
      print('Error')
      break
    else
      print(t1, r1, g1, b1)
    end
  end
end

function love.draw()
  love.graphics.clear(0.98, 0.98, 0.975, 1)
  love.graphics.setColor(r0, g0, b0)
  local w, h = love.graphics.getDimensions()
  local x = w / 2
  local y = h / 2
  local r = math.min(x, y) * 0.3
  love.graphics.circle('fill', x, y, r)
  love.graphics.setColor(0.1, 0.1, 0.1)
  love.graphics.circle('line', x, y, r)
  love.graphics.printf(string.format('%.2f', T), 0, h * 0.75, w, 'center')
end
