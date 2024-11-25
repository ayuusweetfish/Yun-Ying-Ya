const APP_ID = Deno.env.get('APP_ID') || prompt('App ID:')
const API_KEY = Deno.env.get('API_KEY') || prompt('API key:')
const API_SECRET = Deno.env.get('API_SECRET') || prompt('API secret:')

import { encodeBase64 } from 'jsr:@std/encoding/base64'
import { Buffer } from 'node:buffer'

const rfc1123 = (d) => d.toLocaleString('en-GB', {
  timeZone: 'UTC',
  hour12: false,
  weekday: 'short',
  year: 'numeric',
  month: 'short',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
}).replace(/(?<=\d),/, '') + ' GMT'

const { createHmac } = await import('node:crypto')

// Input: plain text and key
// Output: Base64 encoded
const base64_HMAC_SHA256 = (v, k) => createHmac('sha256', k).update(v).digest('base64')

export const speechRecognition = () => new Promise((resolve, reject) => {
  const host = 'iat-api.xfyun.cn'
  const date = rfc1123(new Date())
  const requestLine = `GET /v2/iat HTTP/1.1`
  const signedString = `host: ${host}\ndate: ${date}\n${requestLine}`
  const signature = base64_HMAC_SHA256(signedString, API_SECRET)

  const authString = `api_key="${API_KEY}", algorithm="hmac-sha256", headers="host date request-line", signature="${signature}"`
  const auth = btoa(authString)

  const url =
    `wss://iat-api.xfyun.cn/v2/iat?authorization=${encodeURIComponent(auth)}` +
    `&date=${encodeURIComponent(date)}&host=${host}`

  let ret
  let promiseFinished = false   // For the current call (creation)

  let recognitionResult = []

  let endPromiseResolve = null  // For the `end()` call
  let endPromiseTimer = null

  const ws = new WebSocket(url)
  ws.onopen = (ev) => {
    console.log('open')
    resolve(ret)
    promiseFinished = true
  }
  ws.onclose = (ev) => {
    console.log('close')
  }
  ws.onerror = (ev) => {
    console.log('error', ev.message)
    if (!promiseFinished) {
      reject(ev.message)
      promiseFinished = true
    }
    ws.close()
  }
  ws.onmessage = (ev) => {
    try {
      const o = JSON.parse(ev.data)
      // console.log('data', Deno.inspect(o, { depth: 99 }))
      const sentence =
        o.data.result.ws.map((wsEntry) =>
          wsEntry.cw.map((cwEntry) => cwEntry.w).join('')).join('')
      recognitionResult.push(sentence)
      if (o.data.status === 2) {
        if (endPromiseResolve) {
          clearTimeout(endPromiseTimer)
          endPromiseResolve(recognitionResult.join(''))
        }
        ws.close()
      }
    } catch (e) {
      console.log(e.message)
      return
    }
  }

  let first = true
  const sendAudio = (pcm, last) => {
    if (pcm.length === 0 && !last) { return }
    console.log('send audio', pcm.length, !!last, Date.now())
    const o = {
      data: {
        status: 1,
        format: 'audio/L16;rate=16000',
        encoding: 'raw',
        audio: encodeBase64(pcm),
      }
    }
    if (first) {
      first = false
      Object.assign(o, {
        common: { app_id: APP_ID },
        business: {
          language: 'zh_cn',
          domain: 'iat',
          accent: 'mandarin',
        },
      })
      o.data.status = 0
    }
    if (last) {
      o.data.status = 2
    }
    ws.send(JSON.stringify(o))
  }

  const FRAME = 1280
  let residue = Buffer.alloc(0)
  let lastSend = new Date(0)
  const push = (pcm) => {
    const buf = Buffer.concat([residue, pcm])
    if (new Date() - lastSend < 40) {
      residue = buf
    } else {
      const sendLen = buf.length - buf.length % FRAME
      residue = Buffer.alloc(buf.length % FRAME)
      // residue[:] = buf[sendLen:]
      buf.copy(residue, 0, sendLen)
      sendAudio(buf.slice(0, sendLen))
      lastSend = new Date()
    }
  }

  const end = () => new Promise((resolve, reject) => {
    endPromiseResolve = resolve
    endPromiseTimer = setTimeout(() => {
      if (endPromiseTimer !== null) {
        endPromiseResolve = null
        ws.close()
        reject(new Error('Server did not return valid recognition results in time'))
      }
    }, 5000)
    if (new Date() - lastSend < 40) {
      setTimeout(() => {
        // Workaround: server may not respond timely if sent a large chunk marked finish?
        sendAudio(residue, false)
        sendAudio(Buffer.alloc(0), true)
      }, Math.max(10, 40 - (new Date() - lastSend)))
    } else {
      sendAudio(residue, false)
      sendAudio(Buffer.alloc(0), true)
    }
  })

  ret = {
    push,
    end,
  }
})

if (0) {
// ffmpeg -i 6-聆小璐-助理.wav -t 4 -ar 16000 -f s16le -acodec pcm_s16le 聆小璐.pcm

const r = await speechRecognition()
const fileContent = await Deno.readFile('聆小璐.pcm')
r.push(fileContent)
const s = await r.end()
console.log(s)

}
