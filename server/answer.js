import { logNetwork } from './log_db.js'

const loggedFetchJSON = async (url, options) => {
  const t0 = Date.now()
  const req = await fetch(url, options)
  const respText = await req.text()
  await logNetwork(url, options.body, respText, Date.now() - t0)
  console.log(url, respText)
  return JSON.parse(respText)
}

const requestLLM_OpenAI = (endpoint, model, temperature, maxTokens, key) => async (messages) => {
  const resp = await loggedFetchJSON(
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
        max_tokens: maxTokens,
        temperature: temperature,
      }),
    }
  )

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
  const resp = await loggedFetchJSON(
    'https://generativelanguage.googleapis.com/v1beta/models/' + model + ':generateContent?key=' + key,
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({
        generationConfig: {
          temperature: temperature,
          // Early-stop on terminations, in case
          // the model intends to elaborate on the rationale
          stopSequences: ['```\n\n'],
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
  'https://api.lingyiwanwu.com/v1/chat/completions',
  'yi-lightning', 1.2, 5000,
  Deno.env.get('API_KEY_01AI') || prompt('API key (01.AI):')
)
const requestLLM_DeepSeek3 = requestLLM_OpenAI(
  'https://api.deepseek.com/chat/completions',
  'deepseek-chat', 1.5, 5000,
  Deno.env.get('API_KEY_DEEPSEEK') || prompt('API key (DeepSeek):')
)
const requestLLM_QwenMax = requestLLM_OpenAI(
  'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  'qwen-max', 1.8, 1024 /* Max: 65536 */,
  Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')
)
const requestLLM_QwenPlus = requestLLM_OpenAI(
  'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  'qwen-plus', 1.0 /* [0, 2) */, 1024 /* Max: 32768 */,
  Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')
)
const requestLLM_QwenFlash = requestLLM_OpenAI(
  'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  'qwen-flash', 1.0 /* [0, 2) */, 1024 /* Max: 32768 */,
  Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')
)
const requestLLM_QwenCoder = requestLLM_OpenAI(
  'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  'qwen3-coder', 1.0, 4096 /* Max: 65536 */,
  Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')
)
const requestLLM_QwenCoderFlash = requestLLM_OpenAI(
  'https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions',
  'qwen3-coder-flash', 1.0, 4096 /* Max: 65536 */,
  Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')
)
const requestLLM_Gemini15Flash = requestLLM_Google(
  'gemini-1.5-flash', 0.8,
  Deno.env.get('API_KEY_GOOGLE') || prompt('API key (Google):')
)
const requestLLM_Gemini20FlashExp = requestLLM_Google(
  'gemini-2.0-flash-exp', 0.8,
  Deno.env.get('API_KEY_GOOGLE') || prompt('API key (Google):')
)

export const answerDescription = async (pedestrianMessage) => {
  const systemPrompt = `
雪地里有一只由雪堆成的小鸭子。小鸭不会说话，但它的身体里有一盏小灯，小鸭以灯光的颜色回应行人；乘着自由的想象，它觉得变化的色彩也许能承载世间的一切。

请你展开想象，简洁明了地描述小鸭的回应。可以将色彩赋予含义，用颜色与动画展现你的想象。可以采用各类变化效果（如渐变、呼吸、闪烁等等，以及不同的动画节奏），自由展开联想、避免单一，运用至少两种颜色，但请保持简洁，使行人可以直观地明白其中的含义。请注意小灯发出的光不能制造深色效果，请以亮度明暗或色调纯度来表达。用文字描述灯光效果即可，不必设计参数。
  `.trim()

  const [lightResp, lightDescription] = await requestLLM_QwenFlash([
  /*
    { role: 'system', content: systemPrompt },
    { role: 'user', content: pedestrianMessage },
  */
    { role: 'user', content: `${systemPrompt}\n\n行人对小鸭说：“${pedestrianMessage}”` },
  ])

  return lightDescription
}

export const answerProgram = async (lightDescription) => {
  const [programResp, programFullText] = await requestLLM_QwenCoderFlash([
    { role: 'system', content: `
请你按照后续给出的描述为一盏小灯编写动画效果。

程序以 Lua 语言编写，你可以调用以下系统提供的函数，不要使用 \`os\` 库。动画的速度不要太快，**特别是避免过快频率（单次 1 秒以内）的闪烁**，除非特别需要。在合适的地方加入等待，尽量使大部分段落都能持续三秒以上，为观众留出观察与体会的时间。请细心地选取合适的颜色，以更好地传达你的想法；请注意小灯发出的光不能制造深色效果，请以亮度明暗或色调纯度来表达。在时长方面，也不必被 1 秒的整数倍限制住，请灵活调整时长的整体比例，使其更加流畅，不要使某些段落太长或使重要的段落太短。编写程序前可以先规划各段的时长。

动画函数（颜色分量取值范围均为 0~1，**所有时间 t ≥ 300 毫秒**）：
- delay(t): 等待 t 毫秒；
- fade(r, g, b, t): 从当前颜色过渡到新的颜色 (r, g, b)，历经 t 毫秒；
- blink(r, g, b, n, t1, t2): 在当前颜色与指定颜色 (r, g, b) 之间往返闪烁 n 次，每次持续 t1 毫秒、间隔 t2 毫秒，最后回到当前颜色；
- breath(r, g, b, n, t): 交替色呼吸——在当前颜色与指定颜色 (r, g, b) 之间往返呼吸 n 次，每次周期为 t 毫秒（即，总时长为 t * n 毫秒），最后回到当前颜色。
  - 如果需要明暗呼吸，请先采用 \`fade\` 过渡到目标颜色，再以更暗或更亮的颜色调用 \`breath\`。
  - 如果呼吸的过程中需要被其他效果打断，请拆分成多个 n（次数）更少的呼吸，然后在中间插入所需的效果。
  - 如果需要变化的呼吸或闪烁速度，请同样拆分成多个。
  - 渐变与闪烁颜色之间不要太接近，尽量使明暗之间的差异明显一些（**单通道颜色值变化 0.3 以上**），也可考虑稍稍改变色相。

初始颜色为透明 (0, 0, 0)。请将程序置于顶层作用域，不要置于函数内；在动画序列完成后结束，不必无限循环。
`.trim() },
    { role: 'user', content: lightDescription.trim() },
  ])

  const codeSegments = [
    ...
    programFullText.matchAll(/(?<=^|\n)```[^\n]*\n(.*?)(?:(?<=\n)```(?=\s*(?:\n|$))|$)/gs)
      .map(([_, code]) => code)
    // We early stop at code block terminations, so accept unclosed blocks in the response
  ]
  if (codeSegments.length === 0) throw new Error('LLM did not return valid code')
  return codeSegments.join('\n')
}

// ======== Test run ======== //
if (import.meta.main) {
  // console.log(await answerProgram(await answerDescription('小鸭小鸭，唱首歌吧！')))
  console.log(await answerProgram(await answerDescription('小鸭小鸭，星星是什么样子的？')))
  // console.log(await answerDescription('小鸭小鸭，唱首歌吧！'))
  // console.log(await answerDescription('小鸭小鸭，今天天气真好！'))
  // console.log(await answerDescription('小鸭小鸭，你的心情怎么样？'))
  // console.log(await answerDescription('小鸭小鸭，我今天过生日，可以祝我生日快乐吗！'))
  // console.log(await answerDescription('小鸭小鸭，你见过雷雨吗？'))
  // console.log(await answerDescription('小鸭小鸭，彩虹是什么样子的？'))
  // console.log(await answerDescription('小鸭小鸭，你见过日落吗？'))
}
