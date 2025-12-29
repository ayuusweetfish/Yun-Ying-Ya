import { lua, lauxlib, lualib, to_luastring, to_jsstring } from 'npm:fengari@0.1.5'

const clamp = (x, a, b) => Math.max(a, Math.min(b, x))

const checkNum = (...nums) => {
  for (const n of nums)
    if (!isFinite(n)) throw new Error('Invalid numeric argument to Lua system function')
}

const systemFunctions = await Deno.readTextFile('sys.lua')

export const evalProgram = async (program) => {
  const L = lauxlib.luaL_newstate()
  lauxlib.luaL_requiref(L, to_luastring('_G'), lualib.luaopen_base, 1)
  lauxlib.luaL_requiref(L, to_luastring('debug'), lualib.luaopen_debug, 1)
  lauxlib.luaL_requiref(L, to_luastring('math'), lualib.luaopen_math, 1)
  lauxlib.luaL_requiref(L, to_luastring('string'), lualib.luaopen_string, 1)
  lauxlib.luaL_requiref(L, to_luastring('table'), lualib.luaopen_table, 1)

  const output = []

  lua.lua_pushjsfunction(L, (L) => {
    const line = to_jsstring(lua.lua_tostring(L, 1))
    output.push(line + '\n')
    return 0
  })
  lua.lua_setglobal(L, to_luastring('line'))

  lua.lua_pushnil(L)
  lua.lua_setglobal(L, to_luastring('print'))

  const fullSource =
    `debug.sethook(function (ev) error('Lua timeout', 0) end, '', 1e6)\ndebug = nil\n`
    + systemFunctions + '\n' + program
  const status = lauxlib.luaL_dostring(L, to_luastring(fullSource))

  if (status !== lua.LUA_OK) {
    const err = to_jsstring(lua.lua_tostring(L, -1))
    throw new Error('Lua: ' + err)
  }

  return output.join('')
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
