import { speechRecognition } from './speech-recognition.js'
import { answerDescription, answerProgram } from './answer.js'
import { evalProgram } from './eval_lua.js'
import { logInteractionStart, logInteractionFill } from './log_db.js'
import { audioPacketStreamDecoder } from './packet_decode.js'
import { Buffer } from 'node:buffer'

const debug = !!Deno.env.get('DEBUG')

const retry = async (fn, attempts, errorMsgPrefix) => {
  for (let i = 0; i < attempts; i++) {
    try {
      return await fn()
    } catch (e) {
      console.log(`${errorMsgPrefix}: ${e}`)
      if (i === attempts - 1) throw e
      continue
    }
  }
}

const answerAssembly = async (t0, audio, pedestrianMessage) => {
  const logId = await logInteractionStart(t0.getTime(), audio, pedestrianMessage)
  const description = await retry(
    async () => await answerDescription(pedestrianMessage),
    3, 'Error fetching description')
  const [program, assembly] = await retry(
    async () => {
      const program = await answerProgram(description)
      const assembly = await evalProgram(program)
      return [program, assembly]
    },
    3, 'Error making program'
  )
  await logInteractionFill(logId, description, program, assembly)
  return assembly
}

const serveReq = async (req) => {
  const t0 = new Date()
  const url = new URL(req.url)
  if (req.method === 'POST' && url.pathname === '/') {
    console.log('connected!')
    const sr = await speechRecognition()
    const payloadBlocks = []  // Uint8Array[] that concatenates to raw request payload
    try {
      const [b1, b2] = req.body.tee()
      await Promise.all([
        (async () => {
          for await (const value of b1) {
            payloadBlocks.push(value)
          }
        })(),
        (async () => {
          for await (const value of b2.pipeThrough(audioPacketStreamDecoder())) {
            sr.push(value)
          }
        })(),
      ])
    } catch (e) {
      console.log(`Error reading request: ${e.message} ${e.stack}`)
    }
    const combinedPayload = new Uint8Array(Buffer.concat(payloadBlocks))
    try {
      const s = await sr.end()
      console.log(`message "${s}"`)
      const pedestrianMessage = s ? `小鸭小鸭，${s}` : '小鸭小鸭，你好你好！'
      const assembly = await answerAssembly(t0, combinedPayload, pedestrianMessage)
      return new Response(assembly)
    } catch (e) {
      console.log(`Internal server error: ${e} ${e.stack}`)
      return new Response(e.message, { status: 500 })
    }
  } else if (req.method === 'POST' && url.pathname === '/message' && debug) {
    try {
      const pedestrianMessage = await req.text()
      const assembly = await answerAssembly(t0, null, pedestrianMessage)
      return new Response(assembly)
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
