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

const requestLLM_Google = (model, temperature, key) => async (messages) => {
  // Ref: https://ai.google.dev/api/generate-content#v1beta.models.generateContent
  const req = await fetch(
    'https://generativelanguage.googleapis.com/v1beta/models/' + model + ':generateContent?key=' + key,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        generationConfig: {
          temperature: temperature,
        },
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
  'gemini-1.5-flash', 0.8,
  Deno.env.get('API_KEY_GOOGLE') || prompt('API key (Google):')
)

export const answerDescription = async (pedestrianMessage) => {
  const t0 = Date.now()

  const [lightResp, lightDescription] = await requestLLM_YiLightning([
    { role: 'system', content: `
雪地里有一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一个彩色小灯，小鸭以灯光的颜色回应行人；乘着自由的想象，它觉得变化的色彩也许能承载世间的一切。

请你展开想象，描述小鸭的回应。可以采用各类变化效果（如渐变、呼吸、闪烁等等，以及不同的动画节奏），但请让动画尽量简洁，使行人可以直观地明白其中的含义，不要使用过多的动画段落。可以将颜色赋予文学性，但不必展开过多的描写，请更多关注小灯本身，用颜色与动画展现你的想象。请注意小灯发出的光不能制造深色效果，请以亮度明暗来表达；另外，不必在开头加入包括象征“小鸭思考”的动画段落。只回应本次对话即可，不用续写。
`.trim() },
    { role: 'user', content: pedestrianMessage },
  ])
  // console.log(Deno.inspect(lightResp, { depth: 99 }), Date.now() - t0)
  console.log(lightDescription)
  console.log(Date.now() - t0)

  return lightDescription
}

export const answerProgram = async (lightDescription) => {
  const t0 = Date.now()

  const [programResp, programFullText] = await requestLLM_Gemini15Flash([
    { role: 'system', content: `
请你按照给出的描述为一盏小灯编写动画效果。

程序以 Lua 语言编写，你可以使用以下函数，不要使用 \`os\` 库。动画的速度尽量慢一些，不要以过快的频率闪烁，在合适的地方加入等待（在可能的部分情形下，尽量使每个段落都能持续三秒以上；闪烁也可以慢些或多几次）。请细心地选取合适的颜色，以更好地传达你的想法。

动画函数（颜色分量取值范围均为 0~1）：
- delay(t): 等待 t 毫秒；
- fade(r, g, b, t): 从当前颜色过渡到新的颜色 (r, g, b)，历经 t 毫秒；
- blink(r, g, b, n, t1, t2): 在当前颜色与指定颜色 (r, g, b) 之间往返闪烁 n 次，每次持续 t1 毫秒、间隔 t2 毫秒，最后回到当前颜色；
- breath(r, g, b, n, t): 交替色呼吸——在当前颜色与指定颜色 (r, g, b) 之间往返呼吸 n 次，周期为 t 毫秒，最后回到当前颜色。
  - 如果需要明暗呼吸，请先采用 \`fade\` 过渡到目标颜色，再以更暗或更亮的颜色调用 \`breath\`。

初始颜色为透明 (0, 0, 0)。请在动画序列完成后结束，不必无限循环。
`.trim() },
    { role: 'user', content: lightDescription.trim() },
  ])
  // console.log(Deno.inspect(programResp, { depth: 99 }), Date.now() - t0)
  console.log(programFullText)
  console.log(Date.now() - t0)

  const codeSegments = [
    ...
    programFullText.matchAll(/^```[^\n]*\n(.*?)(?<=\n)```\s*$/smg)
      .map(([_, code]) => code)
  ]
  if (codeSegments.length === 0) throw new Error('LLM did not return valid code')
  return codeSegments.join('\n')
}

// ======== Test run ======== //
if (import.meta.main) {
  await answerProgram('小鸭小鸭，唱首歌吧！')
  await answerProgram('小鸭小鸭，星星是什么样子的？')
}
