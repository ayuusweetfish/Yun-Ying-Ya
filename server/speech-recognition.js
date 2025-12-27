const API_KEY = Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')

import crypto from 'node:crypto'

export const speechRecognition = () => new Promise((resolve, reject) => {
  const ws = new WebSocket(
    'wss://dashscope.aliyuncs.com/api-ws/v1/inference', {
      headers: {
        'Authorization': `Bearer ${API_KEY}`,
      },
    })

  let ret
  let creationPromiseDone = false
  let creationTimeoutTimer

  let expectedClose = false

  ws.onopen = (ev) => {
    console.log('open')
    ws.send(JSON.stringify({
      header: {
        action: 'run-task',
        task_id: crypto.randomUUID(),
        streaming: 'duplex',
      },
      payload: {
        task_group: 'audio',
        task: 'asr',
        function: 'recognition',
        model: 'gummy-chat-v1',
        input: {},
        parameters: {
          sample_rate: 16000,
          format: 'pcm',
          transcription_enabled: true,
          translation_enabled: false,
          max_end_silence: 1000,
        },
      },
    }))
  }
  ws.onclose = (ev) => {
    console.log('close', ev.code, ev.reason, ev.wasClean)
    if (!expectedClose) console.log('unexpected close')
  }
  ws.onerror = (ev) => {
    console.log('error', ev.message)
    if (!creationPromiseDone) {
      reject(new Error(ev.message))
      creationPromiseDone = true
      clearTimeout(creationTimeoutTimer)
    }
    expectedClose = true
    ws.close()
  }
  ws.onmessage = (ev) => {
    try {
      const o = JSON.parse(ev.data)
      console.log('data', Deno.inspect(o, { depth: 99 }))
      if (o.header && o.header.event === 'task-started' && !creationPromiseDone) {
        resolve(ret)
        creationPromiseDone = true
        clearTimeout(creationTimeoutTimer)
      }
    } catch (e) {
      console.log(e.message)
      return
    }
  }

  creationTimeoutTimer = setTimeout(
    () => ws.onerror({ message: 'Start-up imeout' }), 5000)

  const push = (pcm) => {
  }

  const end = () => new Promise((resolve, reject) => {
    resolve('TODO.')
  })

  ret = {
    push,
    end,
  }
})
