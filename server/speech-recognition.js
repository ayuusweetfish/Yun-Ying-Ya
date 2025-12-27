const API_KEY = Deno.env.get('API_KEY_ALIYUN') || prompt('API key (Aliyun Bailian):')

import crypto from 'node:crypto'

export const speechRecognition = () => new Promise((resolve, reject) => {
  const ws = new WebSocket(
    'wss://dashscope.aliyuncs.com/api-ws/v1/inference', {
      headers: {
        'Authorization': `Bearer ${API_KEY}`,
      },
    })

  const taskId = crypto.randomUUID()

  let ret
  let creationPromiseDone = false
  let creationTimeoutTimer

  let recognitionResult = ''

  let expectedClose = false
  let remoteEarlyFinish = false

  let endPromiseResolve
  let endPromiseTimer

  ws.onopen = (ev) => {
    console.log('open')
    ws.send(JSON.stringify({
      header: {
        action: 'run-task',
        task_id: taskId,
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
      // console.log('data', Deno.inspect(o, { depth: 99 }))
      if (!o.header) return   // Malformed
      if (o.header.event === 'task-started' && !creationPromiseDone) {
        resolve(ret)
        creationPromiseDone = true
        clearTimeout(creationTimeoutTimer)
      } else if (o.header.event === 'result-generated') {
        recognitionResult = o.payload.output.transcription.text
        if (o.payload.output.transcription.sentence_end) {
          if (endPromiseResolve) {
            clearTimeout(endPromiseTimer)
            endPromiseResolve(recognitionResult)
          } else {
            remoteEarlyFinish = true
          }
        }
      } else if (o.header.event === 'task-failed') {
        ws.onerror({ message: `Server signalled task failure (${o.header.error_message})` })
      } else if (o.header.event === 'task-finished') {
        expectedClose = true
        ws.close()
      }
    } catch (e) {
      console.log(e.message)
      return
    }
  }

  creationTimeoutTimer = setTimeout(
    () => ws.onerror({ message: 'Start-up timeout' }), 5000)

  const sendAudio = (pcm) => {
    if (remoteEarlyFinish) return
    ws.send(pcm)
  }

  const FRAME = 4096
  let residue = Buffer.alloc(0)
  const push = (pcm) => {
    // XXX: Maybe avoid allocations
    const buf = Buffer.concat([residue, pcm])
    const sendLen = buf.length - buf.length % FRAME
    residue = Buffer.alloc(buf.length % FRAME)
    // residue[:] = buf[sendLen:]
    buf.copy(residue, 0, sendLen)
    sendAudio(buf.slice(0, sendLen))
  }

  const end = () => new Promise((resolve, reject) => {
    ws.send(JSON.stringify({
      header: {
        action: 'finish-task',
        task_id: taskId,
        streaming: 'duplex',
      },
      payload: {
        input: {},
      },
    }))
    if (remoteEarlyFinish) {
      resolve(recognitionResult)
    } else {
      endPromiseResolve = resolve
      endPromiseTimer = setTimeout(() => {
        if (endPromiseTimer !== null) {
          endPromiseResolve = null
          expectedClose = true
          ws.close()
          reject(new Error('Server did not return valid recognition results in time'))
        }
      }, 5000)
      if (residue.length > 0) sendAudio(residue)
    }
  })

  ret = {
    push,
    end,
  }
})
