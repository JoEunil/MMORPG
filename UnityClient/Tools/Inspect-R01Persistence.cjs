'use strict';
// Read-only inspection of the two dedicated characters. Never prints connection settings.
const fs = require('node:fs');
const path = require('node:path');
const loginRoot = 'C:/Users/joeun/Desktop/workspace/Game/Login';
const mysql = require(path.join(loginRoot, 'node_modules/mysql2/promise'));
const env = require(path.join(loginRoot, 'node_modules/dotenv')).parse(fs.readFileSync(path.join(loginRoot, '.env')));
async function main() {
  const connection = await mysql.createConnection({ host: env.DB_HOST, user: env.DB_USER,
    password: env.DB_PW, database: env.DB_DB, supportBigNumbers: true, bigNumberStrings: true,
    multipleStatements: false });
  try {
    await connection.query('START TRANSACTION READ ONLY');
    const [rows] = await connection.execute('SELECT char_id,channel_id,zone_id,last_pos_x,last_pos_y,level,exp,hp,mp,max_hp,max_mp,attack,dir FROM game.characters WHERE char_id IN (?,?) ORDER BY char_id', [5011, 5012]);
    if (rows.length !== 2) throw new Error('Expected two dedicated character rows.');
    const result = { utc: new Date().toISOString(), mode: 'READ ONLY', characters: rows };
    console.log(JSON.stringify(result));
    await connection.rollback();
  } finally { await connection.end(); }
}
main().catch(() => { console.error('Read-only persistence inspection failed; connection details omitted.'); process.exitCode = 1; });
