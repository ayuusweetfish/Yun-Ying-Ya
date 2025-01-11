import { Buffer } from 'node:buffer'
import { duckOpusDecoder } from './duck_opus_decoder.js'

// Decodes packets into 16-bit PCM,
// encodes the values in platform endianness into a `Uint8Array`

export const audioPacketStreamDecoder = () => {
  let buffer = Buffer.alloc(0)
  const decoder = duckOpusDecoder()

  return new TransformStream({
    transform(chunk, controller) {
      buffer = Buffer.concat([buffer, chunk])
      while (buffer.length > 0) {
        if (buffer.length < 1) break
        let n = buffer[0]
        let p = 1
        if (n > 240) {
          if (buffer.length < 2) break
          n = (n - 240) * 256 + buffer[1] - 15
          p = 2
        }
        if (buffer.length < p + n) break
        const payload = buffer.slice(p, p + n)
        const pcm16 = decoder.decode(payload)
        const pcm16le = new Uint8Array(
          pcm16.buffer.slice(pcm16.byteOffset).transferToFixedLength(pcm16.byteLength)
        )
        controller.enqueue(pcm16le)
        buffer = buffer.slice(p + n)
      }
    },

    flush() {
      if (buffer.length > 0) {
        console.warn('Incomplete packet at end of the stream')
      }
    },
  })
}
