import { Database } from 'jsr:@db/sqlite@0.12'

const db = new Database('log.db')

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
