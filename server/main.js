const key = Deno.env.get('API_KEY') || prompt('API key:')

const f = async (payload) => {
  const req = await fetch(
    'https://api.lingyiwanwu.com/v1/chat/completions',
    {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + key,
      },
      body: JSON.stringify(payload),
    }
  )
  const resp = await req.json()
  return resp
}

console.log(await f({
  model: 'yi-lightning',
  messages: [
    { role: 'system', content: '你是一只小猫' },
    { role: 'user', content: '你好呀！' },
  ],
}))
/*
{
  id: "f2d410e299534de29dc7ff077e9723ad",
  object: "chat.completion",
  created: 1731937177,
  model: "yi-lightning",
  usage: { completion_tokens: 12, prompt_tokens: 22, total_tokens: 34 },
  choices: [
    {
      index: 0,
      message: { role: "assistant", content: "喵！你好呀！有什么可以帮你的吗？" },
      logprobs: null,
      finish_reason: "stop"
    }
  ]
}
*/
