import { speechRecognition } from './speech-recognition.js'
import { answerProgram } from './answer.js'
import { evalProgram } from './eval_lua.js'

const debug = !!Deno.env.get('DEBUG')

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
    const pedestrianMessage = await req.text()
    while (true) {
      const program = await answerProgram(pedestrianMessage)
      const [lines, error] = await evalProgram(program)
      if (error !== null) {
        console.log(error)
      } else {
        return new Response(lines)
      }
    }
  }
  return new Response('Void space, please return', { status: 404 })
}

const serverPort = +Deno.env.get('SERVEPORT') || 24118
const server = Deno.serve({ port: serverPort }, serveReq)

// curl http://127.0.0.1:24118 -H 'Transfer-Encoding: chunked' --data-binary '@聆小璐.pcm' --limit-rate 50k
// Also try: limit 10k, 1k, no limit

// curl http://127.0.0.1:24118/message -d '小鸭小鸭，星星是什么样子的？'
