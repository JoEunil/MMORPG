'use strict';
// Inspect only dedicated inventory row existence, without changing game data.
const fs = require('node:fs');
const path = require('node:path');
const loginRoot = 'C:/Users/joeun/Desktop/workspace/Game/Login';
const mysql = require(path.join(loginRoot, 'node_modules/mysql2/promise'));
const env = require(path.join(loginRoot, 'node_modules/dotenv')).parse(fs.readFileSync(path.join(loginRoot, '.env')));
async function main() {
  const db = await mysql.createConnection({ host: env.DB_HOST, user: env.DB_USER,
    password: env.DB_PW, database: env.DB_DB, multipleStatements: false });
  try {
    await db.query('START TRANSACTION READ ONLY');
    const [rows] = await db.execute('SELECT char_id, OCTET_LENGTH(inventory) AS bytes FROM game.characters_inventory WHERE char_id IN (?,?) ORDER BY char_id', [5011, 5012]);
    console.log('READ ONLY dedicated inventory metadata');
    for (const id of [5011, 5012]) {
      const row = rows.find(r => Number(r.char_id) === id);
      console.log('character=' + id + (row ? '; row=present; blobBytes=' + row.bytes : '; row=missing'));
    }
    await db.rollback();
  } finally { await db.end(); }
}
main().catch(() => { console.error('Read-only inventory inspection failed; credentials omitted.'); process.exitCode = 1; });
