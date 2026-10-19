"use strict";
// SELECT-only by default. --commit requires separately verified human Slack approval.
const fs = require("node:fs");
const path = require("node:path");
const {execFileSync} = require("node:child_process");
const loginRoot = "C:/Users/joeun/Desktop/workspace/Game/Login";
const mysql = require(path.join(loginRoot,"node_modules/mysql2/promise"));
const env = require(path.join(loginRoot,"node_modules/dotenv")).parse(fs.readFileSync(path.join(loginRoot,".env")));
const args = process.argv.slice(2);
const commit = args.length===1 && args[0]==="--commit";
const expected = [
 {id:"5011",user:"6981",profile:"8192",login:"unity_ai_01",name:"unity_ai_01_c02"},
 {id:"5012",user:"6982",profile:"8193",login:"unity_ai_02",name:"unity_ai_02_c02"}
];
function requireCondition(ok){if(!ok)throw new Error("Precondition failed");}
function noClients(){
 const output=execFileSync("powershell.exe",["-NoProfile","-Command","$ErrorActionPreference='Stop'; $c=@(Get-NetTCPConnection -LocalPort 9999); if(@($c | Where-Object State -eq Listen).Count -ne 1){throw 'Listener changed'}; if(@($c | Where-Object State -eq Listen)[0].OwningProcess -ne 9060){throw 'Listener changed'}; @($c | Where-Object State -eq Established).Count"],{encoding:"utf8",stdio:["ignore","pipe","pipe"]});
 requireCondition(output.trim()==="0");
}
function serverStopped(){
 const output=execFileSync("powershell.exe",["-NoProfile","-Command","$ErrorActionPreference='Stop'; $p=@(Get-Process -Name MainServer -ErrorAction SilentlyContinue); if($p.Count -ne 0){throw 'Game server still running'}; $c=@(Get-NetTCPConnection -ErrorAction Stop | Where-Object {($_.LocalPort -eq 9999 -or $_.RemotePort -eq 9999) -and ($_.State -eq 'Listen' -or $_.State -eq 'Established')}); if($c.Count -ne 0){throw 'Game connection remains'}; 'STOPPED'"],{encoding:"utf8",stdio:["ignore","pipe","pipe"]});
 requireCondition(output.trim()==="STOPPED");
 const root="C:/Users/joeun/Desktop/workspace/Game-UnityClient";
 const wal=fs.readdirSync(root).filter(name=>/^(redo|backpressure)\.[0-9]+$/.test(name));
 requireCondition(wal.some(n=>n.startsWith("redo.")) && wal.some(n=>n.startsWith("backpressure.")));
 for(const name of wal)requireCondition(fs.statSync(path.join(root,name)).size===0);
}
function executionState(){if(commit)serverStopped();else noClients();}
async function connection(){return mysql.createConnection({host:env.DB_HOST,user:env.DB_USER,password:env.DB_PW,database:env.DB_DB,multipleStatements:false,supportBigNumbers:true,bigNumberStrings:true});}
async function targets(db,lock){
 const [rows]=await db.execute("SELECT c.char_id,c.user_id,c.profile_id,c.channel_id,c.deleted_at,p.name,u.login_id FROM game.characters c JOIN game.profile p ON p.profile_id=c.profile_id JOIN login.users u ON u.user_id=c.user_id WHERE c.char_id IN (?,?) ORDER BY c.char_id"+(lock?" FOR UPDATE":""),["5011","5012"]);
 requireCondition(rows.length===2);
 rows.forEach((v,i)=>{const e=expected[i]; requireCondition(String(v.char_id)===e.id && String(v.user_id)===e.user && String(v.profile_id)===e.profile && v.login_id===e.login && v.name===e.name && Number(v.channel_id)===1 && v.deleted_at===null);});
}
async function inventory(db,lock){
 const [rows]=await db.execute("SELECT char_id,OCTET_LENGTH(inventory) AS bytes,HEX(inventory)=REPEAT('00',360) AS all_zero FROM game.characters_inventory WHERE char_id IN (?,?) ORDER BY char_id"+(lock?" FOR UPDATE":""),["5011","5012"]);return rows;
}
function verifyRows(rows){requireCondition(rows.length===2);rows.forEach((r,i)=>requireCondition(String(r.char_id)===expected[i].id && Number(r.bytes)===360 && Number(r.all_zero)===1));}
let stage="arguments";
async function main(){
 requireCondition(args.length===0 || commit);
 executionState();
 const db=await connection(); let commitAttempted=false;
 try{
  stage="inventory table";
  const [tables]=await db.execute("SELECT ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA='game' AND TABLE_NAME='characters_inventory'");
  requireCondition(tables.length===1 && tables[0].ENGINE==="InnoDB");
  stage="inventory columns";
  const [columns]=await db.execute("SELECT COLUMN_NAME,COLUMN_TYPE,IS_NULLABLE,COLUMN_KEY,EXTRA FROM information_schema.COLUMNS WHERE TABLE_SCHEMA='game' AND TABLE_NAME='characters_inventory' ORDER BY ORDINAL_POSITION");
  requireCondition(columns.length===2 && columns[0].COLUMN_NAME==="char_id" && columns[0].COLUMN_TYPE==="bigint" && columns[0].IS_NULLABLE==="NO" && columns[0].COLUMN_KEY==="PRI" && columns[0].EXTRA==="" && columns[1].COLUMN_NAME==="inventory" && columns[1].COLUMN_TYPE==="blob" && columns[1].IS_NULLABLE==="YES" && columns[1].COLUMN_KEY==="" && columns[1].EXTRA==="");
  stage="inventory triggers";
  const [triggers]=await db.execute("SELECT TRIGGER_NAME FROM information_schema.TRIGGERS WHERE EVENT_OBJECT_SCHEMA='game' AND EVENT_OBJECT_TABLE='characters_inventory'");
  requireCondition(triggers.length===0);
  await db.query("SET TRANSACTION ISOLATION LEVEL REPEATABLE READ");
  await db.query(commit?"START TRANSACTION":"START TRANSACTION READ ONLY");
  stage="character identity";
  await targets(db,commit);
  stage="inventory rows missing";
  requireCondition((await inventory(db,commit)).length===0);
  stage="game connections";
  executionState();
  if(!commit){await db.rollback(); console.log("PASS SELECT-only preflight: exact two test characters; inventory rows missing; InnoDB/schema/triggers match. No INSERT executed.");return;}
  stage="insert approved empty rows";
  const blob=Buffer.alloc(360);
  const [insert]=await db.execute("INSERT INTO game.characters_inventory (char_id,inventory) VALUES (?,?),(?,?)",["5011",blob,"5012",blob]);
  requireCondition(insert.affectedRows===2);
  verifyRows(await inventory(db,false));
  executionState();
  commitAttempted=true;
  await db.commit();
  console.log("COMMIT acknowledged: two empty inventory rows, items=0.");
 }catch(error){
  if(!commitAttempted)await db.rollback();
  console.error(commitAttempted?"Commit outcome requires SELECT verification; do not rerun INSERT.":"Preparation stopped at "+stage+"; transaction rolled back. No automatic retry.");
  throw new Error("Stopped; credentials and SQL errors omitted");
 }finally{await db.end();}
 if(commit){
  const verify=await connection();
  try{await verify.query("START TRANSACTION READ ONLY"); await targets(verify,false);const rows=await inventory(verify,false);verifyRows(rows);await verify.rollback();for(const r of rows)console.log("PASS committed character="+r.char_id+"; bytes="+r.bytes+"; all_zero="+r.all_zero);}
  finally{await verify.end();}
 }
}
main().catch(()=>{console.error("Empty inventory preparation stopped. Inspect read-only state before any retry; credentials omitted.");process.exitCode=1;});
