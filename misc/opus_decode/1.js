const wasmModule = new WebAssembly.Module(await Deno.readFile('duck_opus_decoder.wasm'))

const wasmInstance = new WebAssembly.Instance(wasmModule, {
  wasi_snapshot_preview1: {
    fd_seek: () => console.log('fd_seek'),
    fd_close: () => console.log('fd_close'),
    fd_write: () => console.log('fd_write'),
    proc_exit: () => console.log('proc_exit'),
  },
})
const decoder = wasmInstance.exports.create()
console.log(decoder)
