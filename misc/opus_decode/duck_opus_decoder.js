const wasmModule = new WebAssembly.Module(await Deno.readFile('duck_opus_decoder.wasm'))

export const duckOpusDecoder = () => {
  const wasmInstance = new WebAssembly.Instance(wasmModule, {
    wasi_snapshot_preview1: {
      fd_seek: () => console.log('fd_seek'),
      fd_close: () => console.log('fd_close'),
      fd_write: () => console.log('fd_write'),
      proc_exit: () => console.log('proc_exit'),
    },
  })
  const initErr = wasmInstance.exports.init()
  if (initErr !== 0) {
    throw new Error(`Error initialising instance: code ${initErr}`)
  }
  const inBufAddr = wasmInstance.exports.in_buffer()
  const outBufAddr = wasmInstance.exports.out_buffer()

  const decode = (packet) => {
    ;(new Uint8Array(wasmInstance.exports.memory.buffer, inBufAddr)).set(packet)
    const n = wasmInstance.exports.decode(packet.length)
    if (n < 0) {
      throw new Error(`Error decoding: code ${n}`)
    }
    const outBuf = new Int16Array(wasmInstance.exports.memory.buffer, outBufAddr, n)
    return outBuf
  }

  return { decode }
}

// ======== Test run ======== //
if (import.meta.main) {
  const d = duckOpusDecoder()
  console.log(d.decode([
    0x48, 0x0b, 0xe4, 0xe0, 0xfa, 0xe3, 0xdc, 0xbe,
    0x35, 0x44, 0xb0, 0xb7, 0xec, 0x79, 0x3a, 0x80,
  ]))
}
