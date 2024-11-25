const API_KEY = Deno.env.get('API_KEY') || prompt('API key:')
const API_SECRET = Deno.env.get('API_SECRET') || prompt('API secret:')

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

const speechRecognition = async () => {
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

  const ws = new WebSocket(url)
  ws.onopen = (ev) => {
    console.log('open')
  }
  ws.onclose = (ev) => {
    console.log('close')
  }
  ws.onerror = (ev) => {
    console.log('error', ev.message)
  }
  ws.onmessage = (ev) => {
    console.log(data, Deno.inspect(ev.data))
  }
}

await speechRecognition()
