const key = Deno.env.get('API_KEY') || prompt('API key:')

const API_ENDPOINT = 'https://api.lingyiwanwu.com/v1/chat/completions'
const API_MODEL = 'yi-lightning'

{
const API_ENDPOINT = 'https://spark-api-open.xf-yun.com/v1/chat/completions'
const API_MODEL = 'generalv3.5'
}

const requestLLM = async (messages) => {
  const req = await fetch(
    API_ENDPOINT,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + key,
      },
      body: JSON.stringify({
        model: API_MODEL,
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

const run = async (pedestrianMessage) => {
  const t0 = Date.now()

  const r1 = await requestLLM([
    { role: 'system', content: '你是雪地里一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一个彩色小灯，小鸭以灯光的颜色回应行人；乘着自由的想象，它觉得变化的色彩也许能承载世间的一切。' },
    { role: 'user', content: pedestrianMessage },
  ])
  console.log(r1, Date.now() - t0)
  const lightDescription = responseText(r1)

  const r2 = await requestLLM([
    { role: 'system', content: '你是一名交互动画工程师，你了解色彩与动画原理，认真对待细节，采用合适的混色、过渡、缓动函数等方式来实现赏心悦目的动态色彩效果。' },
    { role: 'user', content:
      `你好！请你编写程序实现这样的灯效：“${lightDescription}”\n\n` +
      '灯珠的颜色由一个 JavaScript 函数 `frame_light()` 计算。这个函数每隔 100 ms 被调用一次，它返回三个 0~255 范围内的数值，依次表示当前时刻灯光的 R、G、B 分量。需要保存的数据可置于全局变量。只需给出你编写的 `frame_light()` 函数即可，循环执行的部分由运行环境提供，你不必编写。不必给出过多的解释，但请多加注意时间与速率方面，避免过快或过慢。'
    }
  ])
  console.log(r2, Date.now() - t0)
  const responseWithCode = responseText(r2)

  const code = (responseWithCode.match(/^```[^\n]+\n(.*?)(?<=\n)```$/sm) || [])[1]
  console.log(code)
}

await run('小鸭小鸭，星星是什么样子的？')
