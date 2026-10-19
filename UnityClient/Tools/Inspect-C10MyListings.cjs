'use strict';
// Read-only counts for the two designated test characters; no seeding or trading.
const fs = require('node:fs');
const path = require('node:path');
const loginRoot = 'C:/Users/joeun/Desktop/workspace/Game/Login';
const mysql = require(path.join(loginRoot, 'node_modules/mysql2/promise'));
const env = require(path.join(loginRoot, 'node_modules/dotenv')).parse(fs.readFileSync(path.join(loginRoot, '.env')));
async function main() {
  const db = await mysql.createConnection({host: env.DB_HOST, user: env.DB_USER,
    password: env.DB_PW, database: env.DB_DB, multipleStatements: false});
  try {
    await db.query('START TRANSACTION READ ONLY');
    const [rows] = await db.execute("SELECT seller_id, status, COUNT(*) AS listing_count FROM bazaar.bazaar WHERE seller_id IN (?, ?) AND status <> 'CANCELLED' GROUP BY seller_id, status ORDER BY seller_id, status", [5011, 5012]);
    console.log('READ ONLY existing non-cancelled listing counts');
    for (const character of [5011, 5012]) {
      const owned = rows.filter(row => Number(row.seller_id) === character);
      console.log('character=' + character + '; total=' + owned.reduce((sum, row) => sum + Number(row.listing_count), 0));
      for (const row of owned) console.log('status=' + row.status + '; count=' + row.listing_count);
    }
    await db.rollback();
  } finally { await db.end(); }
}
main().catch(error => {
  const code = /^[A-Z_0-9]+$/.test(error.code || '') ? error.code : 'UNKNOWN';
  console.error('Read-only my-list inspection failed; code=' + code + '; credentials omitted.');
  process.exitCode = 1;
});
