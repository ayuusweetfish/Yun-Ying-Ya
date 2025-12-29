import { DatabaseSync } from 'node:sqlite'

const db = new DatabaseSync('log.db')

db.prepare(`
  CREATE TABLE IF NOT EXISTS interactions (
    timestamp INTEGER,
    audio BLOB,
    message TEXT,
    description TEXT,
    program TEXT,
    assembly TEXT
  )
`).run()

db.prepare(`
  CREATE TABLE IF NOT EXISTS network (
    url TEXT,
    payload TEXT,
    response TEXT,
    time INTEGER
  )
`).run()

const stmtLogNetwork = db.prepare(`
  INSERT INTO network VALUES (?, ?, ?, ?)
`)
stmtLogNetwork.db = db  // Prevent garbage collection closing connection
export const logNetwork = async (url, payload, response, time) => {
  stmtLogNetwork.run(url, payload, response, time)
}

const stmtLogInteractionStart = db.prepare(`
  INSERT INTO interactions VALUES (?, ?, ?, NULL, NULL, NULL) RETURNING rowid
`)
export const logInteractionStart = async (timestamp, audio, message) => {
  const { rowid } = stmtLogInteractionStart.get(timestamp, audio, message)
  return rowid
}
const stmtLogInteractionFill = db.prepare(`
  UPDATE interactions
  SET description = ?, program = ?, assembly = ?
  WHERE rowid = ?
`)
export const logInteractionFill = async (rowid, description, program, assembly) => {
  stmtLogInteractionFill.run(description, program, assembly, rowid)
}

/*
  To extract audio recordings:

  sqlite3 log.db "SELECT writefile(timestamp || '.pcm', audio) FROM interactions WHERE length(audio) > 0 ORDER BY rowid DESC LIMIT 5"
  rsync --remove-source-files ssh_host:/path/to/duck/server/*.pcm .
  for i in [0-9]*.pcm; do ffmpeg -ar 16000 -f s16le -acodec pcm_s16le -i $i ${i%.pcm}.wav && rm $i; done
*/
