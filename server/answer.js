const requestLLM_OpenAI = (endpoint, model, temperature, key) => async (messages) => {
  const req = await fetch(
    endpoint,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + key,
      },
      body: JSON.stringify({
        model: model,
        messages,
        max_tokens: 5000,
        temperature: temperature,
      }),
    }
  )
  const resp = await req.json()

  // Extract text
  if (!(resp.choices instanceof Array) ||
      resp.choices.length !== 1 ||
      typeof resp.choices[0] !== 'object' ||
      typeof resp.choices[0].message !== 'object' ||
      resp.choices[0].message.role !== 'assistant' ||
      typeof resp.choices[0].message.content !== 'string')
    throw new Error('Incorrect schema!')
  const text = resp.choices[0].message.content

  return [resp, text]
}

const requestLLM_Google = (model, key) => async (messages) => {
  // Ref: https://ai.google.dev/api/generate-content#v1beta.models.generateContent
  const req = await fetch(
    'https://generativelanguage.googleapis.com/v1beta/models/' + model + ':generateContent?key=' + key,
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

  // Extract text
  const { candidates } = resp
  const [ candidate ] = candidates
  const { content } = candidate
  const { parts } = content
  if (!(parts instanceof Array)) throw new Error('Incorrect schema!')
  const text = parts.map(({ text }) => text || '').join('')

  return [resp, text]
}

const requestLLM_YiLightning = requestLLM_OpenAI(
  'https://api.lingyiwanwu.com/v1/chat/completions', 'yi-lightning', 1.2,
  Deno.env.get('API_KEY_01AI') || prompt('API key (01.AI):')
)
const requestLLM_Gemini15Flash = requestLLM_Google(
  'gemini-1.5-flash', 
  Deno.env.get('API_KEY_GOOGLE') || prompt('API key (Google):')
)

const run = async (pedestrianMessage) => {
  const t0 = Date.now()

  const [lightResp, lightFullText] = await requestLLM_YiLightning([
    { role: 'system', content: `
雪地里有一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一个彩色小灯，小鸭以灯光的颜色回应行人；乘着自由的想象，它觉得变化的色彩也许能承载世间的一切。

请你展开想象，描述小鸭的回应。可以采用各类变化效果（如渐变、呼吸、闪烁等等，以及不同的动画节奏），但请让动画尽量简洁，使行人可以直观地明白其中的含义，不要使用过多的动画段落。可以将颜色赋予文学性，但不必展开过多的描写，请更多关注小灯本身，用颜色与动画展现你的想象。请尽量减少使用深色；另外，不必在开头加入包括象征“小鸭思考”的动画段落。只回应本次对话即可，不用续写。
`.trim() },
    { role: 'user', content: pedestrianMessage },
  ])
  console.log(Deno.inspect(lightResp, { depth: 99 }), Date.now() - t0)
  console.log('----')
  console.log(lightFullText)
/*
  console.log('----')
  const code = (lightFullText.match(/^```[^\n]+\n(.*?)(?<=\n)```\s*$/sm) || [])[1]
  console.log(code)
*/
}

// await run('小鸭小鸭，唱首歌吧！')
await run('小鸭小鸭，星星是什么样子的？')
