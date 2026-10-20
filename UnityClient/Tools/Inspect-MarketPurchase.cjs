'use strict';
// Read-only eligibility and before/after snapshot for the designated local test characters.
const fs = require('node:fs');
const path = require('node:path');
const loginRoot = 'C:/Users/joeun/Desktop/workspace/Game/Login';
const mysql = require(path.join(loginRoot, 'node_modules/mysql2/promise'));
const env = require(path.join(loginRoot, 'node_modules/dotenv')).parse(fs.readFileSync(path.join(loginRoot, '.env')));
async function main() {
  const db = await mysql.createConnection({host: env.DB_HOST, user: env.DB_USER,
    password: env.DB_PW, database: env.DB_DB, supportBigNumbers: true, bigNumberStrings: true, multipleStatements: false});
  try {
    await db.query('START TRANSACTION READ ONLY');
    for (const character of [5011, 5012]) {
      const [money] = await db.execute('SELECT diamond, total_spent FROM bazaar.characters_diamond WHERE char_id = ?', [character]);
      const [outbox] = await db.execute('SELECT delivery_status, COUNT(*) AS count FROM bazaar.buyer_outbox WHERE char_id = ? GROUP BY delivery_status', [character]);
      console.log('character=' + character + '; diamondRow=' + (money.length > 0) + '; diamond=' + (money[0]?.diamond ?? 'absent') + '; totalSpent=' + (money[0]?.total_spent ?? 'absent'));
      for (const row of outbox) console.log('character=' + character + '; outbox=' + row.delivery_status + '; count=' + row.count);
      if (!outbox.length) console.log('character=' + character + '; outboxRows=0');
    }
    const [listings] = await db.execute("SELECT listing_id, seller_id, item_id, quantity, price FROM bazaar.bazaar WHERE item_type = 1 AND status = 'TRADING' ORDER BY listing_id DESC LIMIT 3");
    for (const row of listings) console.log('listing=' + row.listing_id + '; seller=' + row.seller_id + '; item=' + row.item_id + '; quantity=' + row.quantity + '; price=' + row.price);
    await db.rollback();
  } finally { await db.end(); }
}
main().catch(error => {
  const code = /^[A-Z_0-9]+$/.test(error.code || '') ? error.code : 'UNKNOWN';
  console.error('Read-only market purchase inspection failed; code=' + code + '; credentials omitted.');
  process.exitCode = 1;
});
