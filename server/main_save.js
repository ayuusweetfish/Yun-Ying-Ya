import { speechRecognition } from './speech-recognition.js'
import { audioPacketStreamDecoder } from './packet_decode.js'

const serveReq = async (req) => {
  const url = new URL(req.url)
  const fileName = `/tmp/record_${Date.now()}.bin`
  if (req.method === 'POST' && url.pathname === '/') {
    console.log('Incoming! (Raw)')
    try {
      const f = await Deno.open(fileName, { write: true, create: true, truncate: true })
      await req.body.pipeTo(f.writable)
    } catch (e) {
      console.log('Error', e)
      return new Response(e.message, { status: 500 })
    } // No need for `finally` block, as `pipeTo()` closes the streams
    console.log('Ok!', fileName)
    return new Response('Ok')
  } else if (req.method === 'POST' && url.pathname === '/decode') {
    console.log('Incoming! (Decode)')
    try {
      const sr = (Deno.env.get('SR') === '1' ? await speechRecognition() : null)
      const f = await Deno.open(fileName, { write: true, create: true, truncate: true })
      const [b1, b2] = req.body.pipeThrough(audioPacketStreamDecoder()).tee()
      await Promise.all([
        (async () => await b1.pipeTo(f.writable))(),
        (async () => {
          for await (const value of b2) if (sr) sr.push(value)
        })(),
      ])
      console.log('Ok!', fileName)
      const s = (sr ? await sr.end() : '(speech recognition off)')
      return new Response(fileName + '\n' + s)
    } catch (e) {
      console.log(`Internal server error: ${e} ${e.stack}`)
      return new Response(e.message, { status: 500 })
    }
  }
  return new Response('Void space, please return', { status: 404 })
}

const serverPort = +Deno.env.get('SERVEPORT') || 24678
const server = Deno.serve({ port: serverPort }, serveReq)

// gzip < 聆小璐.pcm | curl http://127.0.0.1:24678 -H 'Transfer-Encoding: chunked' --data-binary @-
// ffmpeg -ar 16000 -f s16le -acodec pcm_s16le -i record_1732799532755.bin record_1732799532755.wav
