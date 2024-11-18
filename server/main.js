const key = Deno.env.get('API_KEY') || prompt('API key:')

const requestLLM = async (messages) => {
  const req = await fetch(
    'https://api.lingyiwanwu.com/v1/chat/completions',
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + key,
      },
      body: JSON.stringify({
        model: 'yi-lightning',
        messages,
      }),
    }
  )
  const resp = await req.json()
  return resp
}

const responseText = (r) => {
  if (!(r.choices instanceof Array) ||
      r.choices.length !== 1 ||
      typeof r.choices[0] !== 'object' ||
      typeof r.choices[0].message !== 'object' ||
      r.choices[0].message.role !== 'assistant' ||
      typeof r.choices[0].message.content !== 'string')
    throw new Error('Incorrect format!')
  return r.choices[0].message.content
}

const run = async (message) => {
  const r1 = await requestLLM([
    { role: 'system', content: '你是雪地里一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一个彩色小灯，乘着自由的想象，它觉得变化的色彩也许容得下世间的一切。小鸭以灯光的颜色回应行人。' },
    { role: 'user', content: '小鸭小鸭，今天的天气怎么样？' },
  ])
  console.log(r1)
  const lightDescription = responseText(r1)

  const r2 = await requestLLM([
    { role: 'user', content:
      `你好！请你编写一个程序实现如下描述的灯效：“${lightDescription}”\n\n` +
      '灯珠的颜色由一个 JavaScript 函数 `frame_light()` 计算。这个函数将从外部每隔 100 ms 调用一次，它返回三个 0~255 范围内的数值，分别表示某个时刻灯光的 R、G、B 分量。需要保存的数据可置于全局变量。只需要给出你编写的 `frame_light()` 函数即可，循环执行的部分由运行环境提供，你不必编写。也不必给出额外的解释。'
    }
  ])
  console.log(r2)
  const responseWithCode = responseText(r2)
}

await run('小鸭小鸭，今天的天气怎么样？')
