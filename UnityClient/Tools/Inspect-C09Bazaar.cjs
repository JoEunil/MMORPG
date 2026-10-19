'use strict';
// Only aggregate existing tradable listings; never seed or modify market data.
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
    const [rows] = await db.execute("SELECT item_type, COUNT(*) AS listing_count FROM bazaar.bazaar WHERE item_type IN (1,2,3) AND status = 'TRADING' GROUP BY item_type ORDER BY item_type");
    console.log('READ ONLY existing TRADING listing counts');
    for (const type of [1, 2, 3]) {
      const row = rows.find(r => Number(r.item_type) === type);
      console.log('itemType=' + type + '; listings=' + (row ? row.listing_count : 0));
    }
    await db.rollback();
  } finally { await db.end(); }
}
main().catch(error => {
  const code = /^[A-Z_0-9]+$/.test(error.code || '') ? error.code : 'UNKNOWN';
  console.error('Read-only bazaar inspection failed; code=' + code + '; credentials omitted.');
  process.exitCode = 1;
});
