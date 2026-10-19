'use strict';
// C02-DB-01/r1, approved by U0C9QEUHPMK at Slack ts1792303103.818899.
// No credentials in arguments/output. Default is read-only inspection; --commit inserts four rows once.
const fs = require('node:fs');
const path = require('node:path');
const { execFileSync } = require('node:child_process');
const loginRoot = 'C:/Users/joeun/Desktop/workspace/Game/Login';
const mysql = require(path.join(loginRoot, 'node_modules/mysql2/promise'));
const env = require(path.join(loginRoot, 'node_modules/dotenv')).parse(fs.readFileSync(path.join(loginRoot, '.env')));
const targets = [
  { login: 'unity_ai_01', name: 'unity_ai_01_c02', x: 10, y: 10 },
  { login: 'unity_ai_02', name: 'unity_ai_02_c02', x: 12, y: 10 }
];
const commit = process.argv.length === 3 && process.argv[2] === '--commit';
if (process.argv.length > 2 && !commit) throw new Error('Only --commit is accepted.');
let connection;
let transaction = false;
let commitAttempted = false;
function requireThat(condition, label) { if (!condition) throw new Error(label); }
function offline() {
  const count = execFileSync('powershell.exe', ['-NoProfile', '-Command',
    '@(Get-NetTCPConnection -ErrorAction Stop | Where-Object { $_.LocalPort -eq 9999 -and $_.State -eq "Established" }).Count'],
  { encoding: 'utf8', windowsHide: true }).trim();
  requireThat(count === '0', 'Game TCP connections must be zero; no session interruption is allowed.');
}
async function query(sql, values = []) { return (await connection.execute(sql, values))[0]; }
async function main() {
  offline();
  connection = await mysql.createConnection({ host: env.DB_HOST, user: env.DB_USER,
    password: env.DB_PW, database: env.DB_DB, supportBigNumbers: true, bigNumberStrings: true,
    multipleStatements: false });
  const tables = await query("SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='game' AND TABLE_NAME IN ('profile','characters')");
  requireThat(tables.length === 2 && tables.every(r => r.ENGINE === 'InnoDB'), 'Expected two transactional InnoDB tables.');
  const triggers = await query("SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE EVENT_OBJECT_SCHEMA='game' AND EVENT_OBJECT_TABLE IN ('profile','characters')");
  requireThat(triggers.length === 0, 'Unexpected triggers; stop before writes.');
  const columns = await query("SELECT TABLE_NAME,COLUMN_NAME,COLUMN_DEFAULT,EXTRA FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='game' AND TABLE_NAME IN ('profile','characters')");
  const defaults = { level: '1', exp: '0', hp: '10000', mp: '10000', max_hp: '10000', max_mp: '10000', attack: '10', dir: '0' };
  for (const [name, value] of Object.entries(defaults)) {
    const c = columns.find(r => r.TABLE_NAME === 'characters' && r.COLUMN_NAME === name);
    requireThat(c && !c.EXTRA, 'Missing/generated character column: ' + name);
    if (String(c.COLUMN_DEFAULT) !== value)
      console.log(`Schema default difference: ${name}=${c.COLUMN_DEFAULT}; approved new-row value=${value} will be explicit. No schema change.`);
  }
  const version = columns.find(r => r.TABLE_NAME === 'profile' && r.COLUMN_NAME === 'version');
  const deleted = columns.find(r => r.TABLE_NAME === 'characters' && r.COLUMN_NAME === 'deleted_at');
  requireThat(version && !version.EXTRA && deleted && !deleted.EXTRA,
    'Profile version/deleted columns missing or generated.');
  await connection.beginTransaction();
  transaction = true;
  const users = await query("SELECT user_id,login_id FROM login.users WHERE login_id IN (?,?) FOR UPDATE", targets.map(t => t.login));
  requireThat(users.length === 2 && new Set(users.map(r => String(r.user_id))).size === 2 &&
    targets.every(t => users.filter(r => r.login_id === t.login).length === 1), 'Expected two unique approved accounts.');
  const ids = targets.map(t => String(users.find(r => r.login_id === t.login).user_id));
  const existing = await query('SELECT char_id FROM game.characters WHERE user_id IN (?,?) FOR UPDATE', ids);
  const collisions = await query('SELECT profile_id FROM game.profile WHERE name IN (?,?) FOR UPDATE', targets.map(t => t.name));
  requireThat(existing.length === 0 && collisions.length === 0, 'Existing characters/name collisions; no overwrite or repeat insert.');
  offline();
  if (!commit) {
    await connection.rollback(); transaction = false;
    console.log('INSPECTION PASS: approved accounts unique/offline; no characters/name collisions; InnoDB/columns/no triggers verified. Approved row values will be explicit. No inserts.');
    return;
  }
  const created = [];
  for (let i = 0; i < targets.length; i++) {
    const t = targets[i];
    const p = await query('INSERT INTO game.profile(name,version) VALUES(?,1)', [t.name]);
    requireThat(p.affectedRows === 1, 'Profile insert must affect one row.');
    const profileID = String(p.insertId);
    const c = await query('INSERT INTO game.characters(user_id,channel_id,profile_id,zone_id,last_pos_x,last_pos_y,level,exp,hp,mp,max_hp,max_mp,attack,dir,deleted_at) VALUES(?,1,?,1,?,?,1,0,10000,10000,10000,10000,10,0,NULL)', [ids[i], profileID, t.x, t.y]);
    requireThat(c.affectedRows === 1, 'Character insert must affect one row.');
    created.push({ login: t.login, userID: ids[i], profileID, characterID: String(c.insertId), name: t.name, x: t.x, y: t.y });
  }
  const rows = await query('SELECT c.*,p.name,p.version FROM game.characters c JOIN game.profile p ON p.profile_id=c.profile_id WHERE c.user_id IN (?,?) ORDER BY c.user_id', ids);
  requireThat(rows.length === 2, 'Final approved-account row count must be exactly two.');
  for (const expected of created) {
    const r = rows.find(r => String(r.char_id) === expected.characterID);
    requireThat(r && String(r.user_id) === expected.userID && String(r.profile_id) === expected.profileID &&
      r.name === expected.name && Number(r.version) === 1 && Number(r.channel_id) === 1 && Number(r.zone_id) === 1 &&
      Number(r.last_pos_x) === expected.x && Number(r.last_pos_y) === expected.y && r.deleted_at === null &&
      Object.entries(defaults).every(([k, v]) => String(r[k]) === v), 'Final row identity/value verification failed.');
  }
  offline();
  commitAttempted = true;
  await connection.commit(); transaction = false;
  const lines = ['C02-DB-01/r1 COMMITTED: exactly 2 profiles + 2 characters.',
    'Approval: U0C9QEUHPMK ts1792303103.818899. No schema/accounts/existing rows/inventory/currency writes.',
    'UTC: ' + new Date().toISOString(), ...created.map(r =>
      `${r.login}: userID=${r.userID}, characterID=${r.characterID}, profileID=${r.profileID}, name=${r.name}, channel/zone=1, position=(${r.x},${r.y})`),
    'No automatic deletion/reset. Normal game testing may persist these character states.'];
  const output = path.resolve(__dirname, '../Captures/c02-db-preparation.txt');
  fs.writeFileSync(output, lines.join('\n') + '\n');
  console.log(lines.join('\n'));
}
main().catch(async error => {
  if (transaction && connection) { try { await connection.rollback(); } catch { } }
  // Never serialize database errors, SQL, connection configuration, or credentials.
  console.error(commitAttempted ? 'COMMIT/REPORT OUTCOME MUST BE CHECKED: do not rerun inserts; inspect approved rows.' :
    'STOP/ROLLBACK: ' + (error.code ? String(error.code) : error.message));
  process.exitCode = 1;
}).finally(async () => { if (connection) await connection.end(); });
