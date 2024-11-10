local s = io.read('a')

print('以下是程序生成的颜色，有不同步长的数据供参考。可以帮忙检查一下是否符合你的设想吗？回复“是”或“否”即可，不必额外作解释。')
local f = load(s)
local i = 0
local last_step = nil
while i < 1000 do
  local step = (i < 200 and 10 or 50)
  if step ~= last_step then
    print(string.format('（以下步长为 %d ms）', step * 10))
    last_step = step
  end
  if i == 0 then
    local r, g, b = f()
    print(string.format('%d ms: #%02x%02x%02x',
      i * 10, math.floor(r + 0.5), math.floor(g + 0.5), math.floor(b + 0.5)))
  end
  for _ = 1, step - 1 do f() end
  local r, g, b = f()
  i = i + step
  print(string.format('%d ms: #%02x%02x%02x',
    i * 10, math.floor(r + 0.5), math.floor(g + 0.5), math.floor(b + 0.5)))
end
