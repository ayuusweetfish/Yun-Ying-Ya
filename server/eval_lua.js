import { LuaFactory } from 'npm:wasmoon@1.16.0'
const luaFactory = new LuaFactory()

const clamp = (x, a, b) => Math.max(a, Math.min(b, x))

const checkNum = (...nums) => {
  for (const n of nums)
    if (!isFinite(n)) throw new Error('Invalid numeric argument to Lua system function')
}

export const evalProgram = async (program) => {
  const l = await luaFactory.createEngine()

  const s = []
  let T = 0
  let [R, G, B] = [0, 0, 0]

  try {
    l.global.set('line', (line) => s.push(line + '\n'))
    // `wasmoon` does not provide toggles for individual libraries, so we remove them here
    l.global.set('print', () => {})
    l.global.set('io', undefined) // `wasmoon` accepts `undefined` instead of `null`
    l.global.set('os', undefined)

    await l.doString(`
debug.sethook(function (ev) error('Lua timeout') end, '', 1e6)

local delay, fade, blink, breath = (function ()
  local MAX_T = 90000

  local T = 0
  local R, G, B = 0, 0, 0

  local round = function (x) return math.floor(x + 0.5) end

  local local_line = function (t, ...)
    T = T + t
    if T > MAX_T then error('Total duration too long') end
    local s = string.format('%d', math.max(1, round(t)))
    for i = 1, select('#', ...) do
      local a = select(i, ...)
      s = s .. ' ' .. (type(a) == 'number' and string.format('%d', round(a)) or tostring(a))
    end
    line(s)
  end

  local function assert_range(x, min, max)
    x = tonumber(x)
    if x == nil then error('Cannot parse numeric argument') end
    if x < min or x > max then error('Numeric argument out of expected range') end
    return x
  end
  local function tint(r, g, b)
    return string.format('%d %d %d', round(r * 1000), round(g * 1000), round(b * 1000))
  end

  local function delay(t)
    local_line(t, 'D')
  end

  local function fade(r, g, b, t)
    r = assert_range(r, 0, 1)
    g = assert_range(g, 0, 1)
    b = assert_range(b, 0, 1)
    t = assert_range(t, 0, MAX_T)
    local_line(t, 'F', tint(r, g, b))
    R, G, B = r, g, b
  end

  local function blink(r, g, b, n, t1, t2)
    r = assert_range(r, 0, 1)
    g = assert_range(g, 0, 1)
    b = assert_range(b, 0, 1)
    n = assert_range(n, 1, MAX_T)
    t1 = assert_range(t1, 10, MAX_T)
    t2 = assert_range(t2, 10, MAX_T)
    local ramp = math.min(t1, t2) * 0.2
    for _ = 1, n do
      local_line(ramp, 'F',  tint(r, g, b))
      local_line(t1 - ramp, 'D')
      local_line(ramp, 'F',  tint(R, G, B))
      local_line(t2 - ramp, 'D')
    end
  end

  local function breath(r, g, b, n, t)
    r = assert_range(r, 0, 1)
    g = assert_range(g, 0, 1)
    b = assert_range(b, 0, 1)
    n = assert_range(n, 1, MAX_T)
    t = assert_range(t, 10, MAX_T)
    for _ = 1, n do
      local_line(t, 'B', tint(r, g, b))
    end
  end

  return delay, fade, blink, breath
end)()
`
      + program)
  } catch (e) {
    throw e
  } finally {
    l.global.close()
  }

  return s.join('')
}

// ======== Test run ======== //
if (import.meta.main) {
  console.log(await evalProgram(`
    -- Initial color is transparent (0, 0, 0)

    -- 小鸭身上的彩色小灯微微闪烁了一下
    blink(0.8, 0.8, 0.2, 1, 200, 500) --  浅黄色闪烁一次

    delay(1000)

    -- 随后泛起了一层淡淡的、柔和的星辉。那光芒是浅浅的银白色，像初冬夜空中的一片细雪，慢慢地扩散成一个柔和的渐变光圈，带着一丝淡蓝的光晕。
    fade(0.8, 0.8, 1, 3000) -- 逐渐过渡到浅银白色

    -- 灯光轻轻地呼吸着，仿佛星光在夜空中微微闪动，像是要传达那些遥远而宁静的存在。
    breath(0.7, 0.7, 0.9, 3, 3000) -- 浅蓝色呼吸三次


    delay(1000)

    -- 随着小灯的呼吸节奏，银白的星光与淡蓝交替闪动，仿佛一颗星星在寒夜中摇曳，透出寒冷而孤独的美丽。
    for i = 1, 3 do
      fade(0.8, 0.8, 1, 1000) -- 银白色
      delay(500)
      fade(0.7, 0.7, 0.9, 1000) -- 浅蓝色
      delay(500)
    end

    delay(1000)

    -- 光芒持续了几秒后，逐渐减弱，最后以一个缓慢的淡出结束了回应，仿佛星星隐没在黎明的天际。
    fade(0, 0, 0, 3000) -- 缓慢淡出到黑色

    -- while true do end
  `))
}
