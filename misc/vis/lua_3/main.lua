local load_duck = function (s)
  local prog = {}
  local fenv = {
    math = math,
    string = string,
    tonumber = tonumber,
    tostring = tostring,
    select = select,
    type = type,
    error = error,
    print = print,
    line = function (l)
      -- Parse program line
      local line = {}
      for w in l:gmatch('%S+') do
        if #line + 1 ~= 2 then w = tonumber(w) end
        line[#line + 1] = w
      end
      line['orig_text'] = string.format('[%d] ', #prog + 1) .. l
      prog[#prog + 1] = line
      print(l)
    end,
  }
  local f, e = load(io.open('sys.lua'):read('a') .. s, 'duck')
  if e then print(e) end
  local f = setfenv(f, fenv)
  f()
  return prog
end

local prog_str = io.popen('pbpaste'):read('a')
local prog_str_matched = prog_str:match('```.-\n(.+)\n```')
if prog_str_matched then prog_str = prog_str_matched end
local prog = load_duck(prog_str)
local eval_prog = function (T)
  local R, G, B = 0, 0, 0   -- Current colour
  if T < 0 then return R, G, B, '' end
  for i = 1, #prog do
    local t = prog[i][1]
    local ty = prog[i][2]
    if T > t then
      T = T - t
      if ty == 'F' then
        local _, _, r, g, b = unpack(prog[i])
        R, G, B = r, g, b
      end
    else
      local orig_text = prog[i]['orig_text']
      if ty == 'D' then
        return R, G, B, orig_text
      elseif ty == 'F' then
        local progress = T / t
        local _, _, r, g, b = unpack(prog[i])
        if progress < 0.5 then progress = progress * progress * 2
        else progress = 1 - (1 - progress) * (1 - progress) * 2 end
        return
          R + (r - R) * progress,
          G + (g - G) * progress,
          B + (b - B) * progress,
          orig_text
      elseif ty == 'B' then
        local progress = T / t
        local _, _, r, g, b = unpack(prog[i])
        progress = (1 - math.cos(progress * (math.pi * 2))) / 2
        return
          R + (r - R) * progress,
          G + (g - G) * progress,
          B + (b - B) * progress,
          orig_text
      else
        error('Unknown operation type "' .. ty .. '"')
      end
    end
  end
  return R, G, B, ''
end

local T = 0
local r, g, b = 0, 0, 0
local orig_text = orig_text

function love.update(dt)
  T = T + dt
  r, g, b, orig_text = eval_prog(T * 1000)
  r = r / 1000
  g = g / 1000
  b = b / 1000
end

function love.draw()
  local base_r, base_g, base_b = 0.1, 0.1, 0.1
  love.graphics.clear(base_r, base_g, base_b)
  love.graphics.setColor(
    base_r + (1 - base_r) * r,
    base_g + (1 - base_g) * g,
    base_b + (1 - base_b) * b,
    1)
  local w, h = love.graphics.getDimensions()
  local x = w / 2
  local y = h / 2
  local radius = math.min(x, y) * 0.3
  love.graphics.circle('fill', x, y, radius)
  love.graphics.setColor(0.5, 0.5, 0.5)
  love.graphics.circle('line', x, y, radius)
  love.graphics.setColor(0.8, 0.8, 0.8)
  love.graphics.printf(string.format('%.2f', T), 0, h * 0.75, w, 'center')
  love.graphics.printf(orig_text, 0, h * 0.79, w, 'center')
  love.graphics.printf(string.format('%.2f  %.2f  %.2f', r, g, b), 0, h * 0.83, w, 'center')
end

function love.keypressed(key)
  if key == 'left' then T = T - 1
  elseif key == 'right' then T = T + 1
  end
end
