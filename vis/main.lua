local load_duck = function (s)
  local f = setfenv(load(s), {
    math = math,
  })
  return function ()
    local status, r, g, b = pcall(f)
    if status then
      return math.floor(r + 0.5), math.floor(g + 0.5), math.floor(b + 0.5)
    else
      return nil
    end
  end
end

local duck_step_fn = load_duck(io.popen('pbpaste'):read('a'))

local T = 0
local frame_n = 0

local r, g, b = duck_step_fn()

function love.update(dt)
  T = T + dt
  while T >= 0.01 do
    r, g, b = duck_step_fn()
    T = T - 0.01
    frame_n = frame_n + 1
  end
end

function love.draw()
  love.graphics.clear(0.98, 0.98, 0.975, 1)
  love.graphics.setColor(r / 255, g / 255, b / 255)
  local w, h = love.graphics.getDimensions()
  local x = w / 2
  local y = h / 2
  local r = math.min(x, y) * 0.3
  love.graphics.circle('fill', x, y, r)
  love.graphics.setColor(0.1, 0.1, 0.1)
  love.graphics.circle('line', x, y, r)
  love.graphics.printf(tostring(frame_n), 0, h * 0.75, w, 'center')
end
