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
  local function assert_range_clamped(x, min, max, tolerance)
    x = assert_range(x, min - tolerance, max + tolerance)
    if x < min then x = min elseif x > max then x = max end
    return x
  end
  local function tint(r, g, b)
    return string.format('%d %d %d', round(r * 1000), round(g * 1000), round(b * 1000))
  end

  local function delay(t)
    local_line(t, 'D')
  end

  local function fade(r, g, b, t)
    r = assert_range_clamped(r, 0, 1, 0.2)
    g = assert_range_clamped(g, 0, 1, 0.2)
    b = assert_range_clamped(b, 0, 1, 0.2)
    t = assert_range(t, 0, MAX_T)
    local_line(t, 'F', tint(r, g, b))
    R, G, B = r, g, b
  end

  local function blink(r, g, b, n, t1, t2)
    r = assert_range_clamped(r, 0, 1, 0.2)
    g = assert_range_clamped(g, 0, 1, 0.2)
    b = assert_range_clamped(b, 0, 1, 0.2)
    n = assert_range(n, 1, MAX_T)
    t1 = assert_range(t1, 10, MAX_T)
    t2 = assert_range(t2, 10, MAX_T)
    local ramp = math.min(t1, t2) * 0.5
    for _ = 1, n do
      local_line(ramp, 'F', tint(r, g, b))
      local_line(t1 - ramp, 'D')
      local_line(ramp, 'F', tint(R, G, B))
      local_line(t2 - ramp, 'D')
    end
  end

  local function breath(r, g, b, n, t)
    r = assert_range_clamped(r, 0, 1, 0.2)
    g = assert_range_clamped(g, 0, 1, 0.2)
    b = assert_range_clamped(b, 0, 1, 0.2)
    n = assert_range(n, 1, MAX_T)
    t = assert_range(t, 10, MAX_T)
    for _ = 1, n do
      local_line(t, 'B', tint(r, g, b))
    end
  end

  return delay, fade, blink, breath
end)()
