local T = 0
local function delay(t)
  T = T + t / 1000
  if T >= 30 then os.exit() end
end
local R, G, B = 0, 0, 0
local function output(r, g, b)
  if r == nil then return R, G, B end
  R, G, B = r, g, b
  print(string.format('%5.2f  %.3f %.3f %.3f', T, r, g, b))
end

local fenv = {
  math = math,
  os = { time = os.time },

  delay = delay,
  output = output,
}
local f, e = load(io.read('a') .. '\nreturn light', 'duck', 't', fenv)
local light = f()
light()
