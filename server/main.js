import { speechRecognition } from './speech-recognition.js'
import { answerDescription, answerProgram } from './answer.js'
import { evalProgram } from './eval_lua.js'

const debug = !!Deno.env.get('DEBUG')

const retry = async (fn, attempts, errorMsgPrefix) => {
  for (let i = 0; i < attempts; i++) {
    try {
      return fn()
    } catch (e) {
      console.log(`${errorMsgPrefix}: ${e}`)
      if (i === attempts - 1) throw e
      continue
    }
  }
}

const serveReq = async (req) => {
  const url = new URL(req.url)
  if (req.method === 'POST' && url.pathname === '/') {
    console.log('connected!')
    const sr = await speechRecognition()
    try {
      for await (const value of req.body) sr.push(value)
    } catch (e) {
      console.log(`Error reading response: ${e.message}`)
    }
    try {
      const s = await sr.end()
      console.log(`returning response "${s}"`)
      return new Response(s)
    } catch (e) {
      return new Response(e.message, { status: 500 })
    }
  } else if (req.method === 'POST' && url.pathname === '/message' && debug) {
    try {
      const pedestrianMessage = await req.text()
      const description = await retry(
        async () => await answerDescription(pedestrianMessage),
        3, 'Error fetching description')
      const lines = await retry(
        async () => await evalProgram(await answerProgram(description)),
        3, 'Error making program'
      )
      return new Response(lines)
    } catch (e) {
      console.log(`Internal server error: ${e}`)
      return new Response(e.message, { status: 500 })
    }
  }
  return new Response('Void space, please return', { status: 404 })
}

const serverPort = +Deno.env.get('SERVE_PORT') || 24678
const server = Deno.serve({ port: serverPort }, serveReq)

// curl http://127.0.0.1:24678 -H 'Transfer-Encoding: chunked' --data-binary '@聆小璐.pcm' --limit-rate 50k
// Also try: limit 10k, 1k, no limit

// curl http://127.0.0.1:24678/message -d '小鸭小鸭，星星是什么样子的？'
