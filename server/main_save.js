const serveReq = async (req) => {
  const url = new URL(req.url)
  if (req.method === 'POST' && url.pathname === '/') {
    console.log('Incoming!')
    const fileName = `record_${Date.now()}.bin`
    let f
    try {
      f = await Deno.open(fileName, { write: true, create: true, truncate: true })
      await req.body.pipeTo(f.writable)
    } catch (e) {
      console.log('Error', e)
      return new Response(e.message, { status: 500 })
    } // No need for `finally` block, as `pipeTo()` closes the streams
    console.log('Ok!', fileName)
    return new Response('Ok')
  }
  return new Response('Void space, please return', { status: 404 })
}

const serverPort = +Deno.env.get('SERVEPORT') || 24118
const server = Deno.serve({ port: serverPort }, serveReq)
