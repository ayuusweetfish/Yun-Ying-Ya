import { speechRecognition } from './speech-recognition.js'

const serveReq = async (req) => {
  const url = new URL(req.url)
  if (req.method === 'POST' && url.pathname === '/') {
    const sr = await speechRecognition()
    const reader = req.body.getReader()
    while (true) {
      const result = await reader.read()
      if (result.done) break

      sr.push(result.value)
    }
    try {
      const s = await sr.end()
      return new Response(s)
    } catch (e) {
      return new Response(e.message, { status: 500 })
    }
  }
  return new Response('Void space, please return', { status: 404 })
}

const serverPort = +Deno.env.get('SERVEPORT') || 24118
const server = Deno.serve({ port: serverPort }, serveReq)

// curl -X POST http://127.0.0.1:24118 --data-binary '@聆小璐.pcm' --limit-rate 50k
// Also try: limit 1k, no limit
