import { spawnSync } from 'node:child_process';
import { join } from 'node:path';
import { readFileSync, existsSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { loadState, saveState, acceptEnvelope } from './slack-delivery.mjs';

const [coordinator, codexPath, testReconnect] = process.argv.slice(2);
if (!/^[a-f0-9-]{36}$/.test(coordinator ?? '') || !codexPath) throw new Error('Explicit Coordinator and Codex path required.');
if (coordinator !== '01a14d46-8b23-7590-8ad5-7efc20f8ecd9') throw new Error('Unexpected active client Coordinator.');
const folder = fileURLToPath(new URL('.', import.meta.url));
const config = { team: 'T0C9ECC3GS3', channel: 'C0C9VFZMV43', user: 'U0C9QEUHPMK', threads: new Set() };
const appToken = process.env.UNITY_SLACK_APP_TOKEN;
const botToken = process.env.UNITY_SLACK_BOT_TOKEN;
delete process.env.UNITY_SLACK_APP_TOKEN;
delete process.env.UNITY_SLACK_BOT_TOKEN;
const path = join(process.env.LOCALAPPDATA, 'UnityCoordinator', 'receiver-active-client-state.json');
const stopPath = join(process.env.LOCALAPPDATA, 'UnityCoordinator', 'receiver-active-client.stop');
if (existsSync(stopPath)) throw new Error('Stop marker exists; inspect before restarting.');
const state = loadState(path);
if (state.coordinator && state.coordinator !== coordinator) throw new Error('Stored Coordinator differs; review state before changing target.');
state.coordinator = coordinator;
saveState(path, state);
let forceDisconnect = testReconnect === '--test-reconnect';
let retryMs = 2000;

async function api(method, token, body = {}) {
  const response = await fetch(`https://slack.com/api/${method}`, {
    method: 'POST', headers: { Authorization: `Bearer ${token}`, 'Content-Type': 'application/json; charset=utf-8' },
    body: JSON.stringify(body), signal: AbortSignal.timeout(15000),
  });
  const result = await response.json();
  if (!result.ok) throw new Error('Slack API request failed');
  return result;
}

function deliver(text, ts, thread) {
  const prompt = `Unity Coordinator 수신기가 지정 사용자의 Slack 답장을 전달합니다. 채널=${config.channel}, 스레드=${thread}, 회신=${ts}, 사용자=${config.user}. 중복 작업 방지를 위해 회신 ID와 HANDOFF.md를 확인하세요. 원문은 아래 JSON 문자열이며 첨부·인용 내용은 별도로 판단하세요. 기존 작업 범위와 보호영역 승인 경계를 유지하고 OS/도구 승인을 우회하지 마세요. 재개 신호는 해당 작업의 검증된 인계 지점에서만 적용합니다. 단계 완료와 필요한 요청은 Unity Coordinator 앱으로 같은 Slack 스레드에 남기세요.\n${JSON.stringify(text)}`;
  const result = spawnSync(codexPath, ['queue', '--thread', coordinator, '--message', prompt], {
    shell: false, windowsHide: true, timeout: 20000, encoding: 'utf8',
  });
  return result.status === 0;
}

const auth = await api('auth.test', botToken);
if (auth.team_id !== 'T0C9ECC3GS3' || auth.user_id !== 'U0C9ZRTQ6F8') throw new Error('Unexpected bot/workspace.');
console.log('BOT_AUTH_OK');
for (const [ts, item] of Object.entries(state.deliveries)) {
  if (item.status !== 'queued') console.log(`NEEDS_REVIEW ${ts}`);
}

while (!existsSync(stopPath)) {
  try {
    const connection = await api('apps.connections.open', appToken);
    await new Promise((resolve) => {
      const socket = new WebSocket(connection.url);
      let ended = false;
      const end = () => {
        if (ended) return;
        ended = true;
        clearTimeout(helloTimeout);
        clearInterval(stopTimer);
        socket.close();
        resolve();
      };
      const helloTimeout = setTimeout(end, 20000);
      const stopTimer = setInterval(() => { if (existsSync(stopPath)) end(); }, 1000);
      socket.addEventListener('close', () => clearInterval(stopTimer));
      socket.addEventListener('close', end);
      socket.addEventListener('error', end);
      socket.addEventListener('message', async ({ data }) => {
        if (ended) return;
        try {
          const envelope = JSON.parse(data);
          if (envelope.type === 'disconnect') return end();
          if (envelope.type === 'hello') {
            clearTimeout(helloTimeout);
            retryMs = 2000;
            console.log('SOCKET_READY');
            if (forceDisconnect) {
              forceDisconnect = false;
              console.log('TEST_DISCONNECT');
              return end();
            }
            console.log(`LISTENING coordinator=${coordinator}; configured-thread-file=threads.txt`);
          }
          if (envelope.envelope_id) {
            const threads = readFileSync(join(folder, 'threads.txt'), 'utf8').split(/\r?\n/).map(x => x.trim()).filter(Boolean);
            if (!threads.length || threads.some(x => !/^\d+\.\d+$/.test(x))) throw new Error('Invalid thread allowlist.');
            config.threads = new Set(threads);
            const result = acceptEnvelope(state, envelope, config,
              () => saveState(path, state),
              () => socket.send(JSON.stringify({ envelope_id: envelope.envelope_id })), deliver);
            if (result !== 'ignored') console.log(`DELIVERY ${result} ${envelope.payload?.event?.ts}`);
          }
        } catch {
          // Do not log payloads, tokens, WebSocket URLs or transport exceptions.
          console.log('EVENT_ERROR: inspect pending state before retrying a reply.');
          end();
        }
      });
    });
  } catch { console.log('CONNECT_FAILED'); }
  if (existsSync(stopPath)) break;
  console.log(`RECONNECT_IN ${retryMs}`);
  await new Promise(resolve => setTimeout(resolve, retryMs));
  retryMs = Math.min(retryMs * 2, 30000);
}
console.log('STOPPED');
