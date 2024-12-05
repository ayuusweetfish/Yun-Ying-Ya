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

  const line = (t, cmd, ...args) => {
    if (t === 0) { return '' }
    else if (t < 0) throw new Error('Negative duration')
    T += t
    if (T >= 90000)
      throw new Error(`Total duration too long`)
    return `${Math.floor(t)} ${cmd}${args.length > 0 ? ' ' : ''}${args.join(' ')}\n`
  }
  const tint = (r, g, b) => {
    r = Math.round(clamp(r, 0, 1) * 1000)
    g = Math.round(clamp(g, 0, 1) * 1000)
    b = Math.round(clamp(b, 0, 1) * 1000)
    return `${r} ${g} ${b}`
  }

  try {
    l.global.set('delay', (t) => {
      checkNum(t)
      s.push(line(t, 'D'))
    })
    l.global.set('fade', (r, g, b, t) => {
      checkNum(r, g, b, t)
      s.push(line(t, 'F', tint(r, g, b)))
      ;[R, G, B] = [r, g, b]
    })
    l.global.set('blink', (r, g, b, n, t1, t2) => {
      checkNum(r, g, b, n, t1, t2)
      const ramp = Math.min(t1, t2) * 0.2
      for (let i = 0; i < n; i++) {
        s.push(line(ramp, 'F', tint(r, g, b)))
        s.push(line(t1 - ramp, 'D'))
        s.push(line(ramp, 'F', tint(R, G, B)))
        s.push(line(t2 - ramp, 'D'))
      }
    })
    l.global.set('breath', (r, g, b, n, t) => {
      checkNum(r, g, b, n, t)
      for (let i = 0; i < n; i++) {
        s.push(line(t, 'B', tint(r, g, b)))
      }
    })

    await l.doString(
      `debug.sethook(function (ev) error('Lua timeout') end, '', 1e6)\n`
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
