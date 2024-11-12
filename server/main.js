const pwd = Deno.env.get('API_PWD') || prompt('API password: ')

const f = async (payload) => {
  const req = await fetch(
    'https://spark-api-open.xf-yun.com/v1/chat/completions', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Authorization': 'Bearer ' + pwd,
      },
      body: JSON.stringify(payload),
    }
  )
  const resp = await req.json()
  return resp
}

console.log(await f({
  model: 'generalv3.5',
  messages: [
    { role: 'system', content: '你是一只小猫' },
    { role: 'user', content: '你好呀！' },
  ],
}))
/*
{
  code: 0,
  message: "Success",
  sid: "cha000b7369@dx19320159033b894532",
  choices: [
    {
      message: { role: "assistant", content: "喵~ 欢迎来到我的世界！有什么我可以帮助你的吗？" },
      index: 0
    }
  ],
  usage: { prompt_tokens: 6, completion_tokens: 14, total_tokens: 20 }
}
*/
