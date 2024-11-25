const key = Deno.env.get('API_KEY') || prompt('API key:')

{
const API_ENDPOINT = 'google'
}

const API_ENDPOINT = 'https://api.lingyiwanwu.com/v1/chat/completions'
const API_MODEL = 'yi-lightning'

{
const API_ENDPOINT = 'https://spark-api-open.xf-yun.com/v1/chat/completions'
const API_MODEL = 'generalv3.5'
}

const requestLLMOpenAI = async (step, messages) => {
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
        max_tokens: 5000,
      }),
    }
  )
  const resp = await req.json()
  return resp
}

const responseTextOpenAI = (r) => {
  if (!(r.choices instanceof Array) ||
      r.choices.length !== 1 ||
      typeof r.choices[0] !== 'object' ||
      typeof r.choices[0].message !== 'object' ||
      r.choices[0].message.role !== 'assistant' ||
      typeof r.choices[0].message.content !== 'string')
    throw new Error('Incorrect format!')
  return r.choices[0].message.content
}

const requestLLMGoogle = async (step, messages) => {
  // https://ai.google.dev/api/generate-content#v1beta.models.generateContent
  // const API_MODEL = (step === 1 ? 'gemini-1.5-flash' : 'gemini-1.5-pro')
  const API_MODEL = 'gemini-1.5-flash'
  const req = await fetch(
    'https://generativelanguage.googleapis.com/v1beta/models/' + API_MODEL + ':generateContent?key=' + key,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        systemInstruction: { parts: [{ text:
          messages.filter(({ role }) => role === 'system')
                  .map(({ content }) => content).join('\n'),
        }] },
        contents:
          messages.filter(({ role }) => role === 'user')
                  .map(({ content }) => ({
                    role: 'user',
                    parts: [{ text: content }],
                  })),
      }),
    }
  )
  const resp = await req.json()
  return resp
}

const responseTextGoogle = (r) => {
  const { candidates } = r
  const [ candidate ] = candidates
  const { content } = candidate
  const { parts } = content
  if (!(parts instanceof Array)) throw new Error('Incorrect format!')
  return parts.map(({ text }) => text || '').join('')
}

const requestLLM = (API_ENDPOINT === 'google' ? requestLLMGoogle : requestLLMOpenAI);
const responseText = (API_ENDPOINT === 'google' ? responseTextGoogle : responseTextOpenAI);

const run = async (pedestrianMessage) => {
  const t0 = Date.now()

  const r1 = await requestLLM(1, [
    { role: 'system', content: '你是雪地里一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一个彩色小灯，小鸭以灯光的颜色回应行人；乘着自由的想象，它觉得变化的色彩也许能承载世间的一切。\n\n请你先展开想象，描述小鸭的回应，再在结尾用简短的几句话概括具体效果（例如，“先缓缓亮起……，再逐渐过渡到……”）。' },
    { role: 'user', content: pedestrianMessage },
  ])
  console.log(Deno.inspect(r1, { depth: 99 }), Date.now() - t0)
  const lightFullText = responseText(r1)
  const lightDescription = (lightFullText.trim().match(/\n(.+)$/) || [])[1]
  if (!lightDescription) throw new Error('Malformed response')

  const r2 = await requestLLM(2, [
    { role: 'system', content: '你是一名交互动画工程师，正在为一盏小灯编写动画效果。你了解色彩与动画原理，认真对待细节，采用合适的混色、过渡、缓动函数等方式来实现赏心悦目的动态色彩。' },
    { role: 'user', content:
      `“${lightDescription}”\n\n` +
      '灯珠的颜色由一个 JavaScript 函数 `frame_light()` 产生。它可以通过函数 `output(R, G, B)` 输出颜色，参数为 0~255 范围内的数值，依次表示灯光的 R、G、B 分量。若要等待，请勿使用 `Date` 类或 `requestAnimationFrame()`；请调用 `delay(t)`，参数为等待的毫秒数。只需实现 `frame_light()` 即可，不必实现其余上述提及的函数。如果需要缓动函数，请一并编写。不必作过多的解释，但请多加注意时间与速率方面，避免过快或过慢。'
    }
  ])
  console.log(Deno.inspect(r2, { depth: 99 }), Date.now() - t0)
  const responseWithCode = responseText(r2)

  const code = (responseWithCode.match(/^```[^\n]+\n(.*?)(?<=\n)```\s*$/sm) || [])[1]
  console.log(code)
}

// await run('小鸭小鸭，唱首歌吧！')
await run('小鸭小鸭，星星是什么样子的？')
