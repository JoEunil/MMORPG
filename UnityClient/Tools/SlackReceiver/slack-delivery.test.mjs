import { test } from 'node:test';
import assert from 'node:assert/strict';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { acceptReply, acceptEnvelope, loadState, saveState } from './slack-delivery.mjs';

const config = { team: 'allowed-team', channel: 'allowed-channel', user: 'allowed-user', threads: new Set(['1.0', '4.0']) };
const reply = { type: 'message', channel: config.channel, user: config.user, thread_ts: '1.0', ts: '2.0', text: '!coord confirm' };

test('replayed reply is delivered once, including after receiver restart', () => {
  const dir = mkdtempSync(join(tmpdir(), 'slack-dedup-'));
  try {
    const path = join(dir, 'state.json');
    let state = { thread: '1.0', deliveries: {} };
    let calls = 0;
    let acked = false;
    const run = () => acceptReply(state, reply, config, () => saveState(path, state), () => { acked = true; }, () => {
      assert.equal(loadState(path).deliveries['2.0'].status, 'pending');
      assert.equal(acked, true);
      calls++;
      return true;
    });
    assert.equal(run(), 'queued');
    assert.equal(run(), 'duplicate');
    state = loadState(path);
    assert.equal(run(), 'duplicate');
    assert.equal(calls, 1);
  } finally { rmSync(dir, { recursive: true }); }
});

test('unrelated channel, thread, user, bots, edits and unprefixed messages never wake Coordinator', () => {
  const state = { thread: '1.0', deliveries: {} };
  for (const change of [{ channel: 'other' }, { user: 'other' }, { thread_ts: 'other' },
    { bot_id: 'bot' }, { app_id: 'app' }, { subtype: 'message_changed' }, { text: 'confirm' }]) {
    assert.equal(acceptReply(state, { ...reply, ...change }, config, () => assert.fail(), () => {}, () => assert.fail()), 'ignored');
  }
});

test('failed or interrupted delivery remains blocked from automatic replay', () => {
  const state = { thread: '1.0', deliveries: {} };
  assert.equal(acceptReply(state, reply, config, () => {}, () => {}, () => false), 'needs_review');
  assert.equal(acceptReply(state, reply, config, () => {}, () => {}, () => assert.fail()), 'duplicate');
  state.deliveries['2.0'].status = 'pending';
  assert.equal(acceptReply(state, reply, config, () => {}, () => {}, () => assert.fail()), 'duplicate');
});

test('separate replies with identical text remain separate user messages', () => {
  const state = { thread: '1.0', deliveries: {} };
  let count = 0;
  for (const ts of ['2.0', '3.0']) acceptReply(state, { ...reply, ts }, config, () => {}, () => {}, () => { count++; return true; });
  assert.equal(count, 2);
});

test('each registered change thread carries its own source and persists no message body', () => {
  const state = { deliveries: {} };
  const delivered = [];
  for (const [thread_ts, ts] of [['1.0', '2.0'], ['4.0', '5.0']]) {
    assert.equal(acceptReply(state, { ...reply, thread_ts, ts }, config, () => {}, () => {},
      (text, id, thread) => { delivered.push([text, id, thread]); return true; }), 'queued');
    assert.equal(state.deliveries[ts].thread, thread_ts);
    assert.equal('text' in state.deliveries[ts], false);
  }
  assert.deepEqual(delivered.map(item => item[2]), ['1.0', '4.0']);
});

test('ambiguous queue exceptions stay reserved and cannot be auto-replayed', () => {
  const state = { deliveries: {} };
  assert.throws(() => acceptReply(state, reply, config, () => {}, () => {}, () => { throw new Error('unknown outcome'); }));
  assert.equal(state.deliveries[reply.ts].status, 'pending');
  assert.equal(acceptReply(state, reply, config, () => {}, () => {}, () => assert.fail()), 'duplicate');
});

test('only event_callback in an events_api envelope for the fixed workspace is delivered', () => {
  const envelope = { type: 'events_api', payload: { type: 'event_callback', team_id: config.team, event: reply } };
  for (const invalid of [
    { ...envelope, type: 'interactive' },
    { ...envelope, payload: { ...envelope.payload, type: 'other' } },
    { ...envelope, payload: { ...envelope.payload, team_id: 'other-team' } },
    { ...envelope, payload: { ...envelope.payload, team_id: undefined } },
  ]) {
    let acked = false;
    assert.equal(acceptEnvelope({ deliveries: {} }, invalid, config, () => assert.fail(),
      () => { acked = true; }, () => assert.fail()), 'ignored');
    assert.equal(acked, true);
  }
  let delivered = 0;
  assert.equal(acceptEnvelope({ deliveries: {} }, envelope, config, () => {}, () => {},
    () => { delivered++; return true; }), 'queued');
  assert.equal(delivered, 1);
});

test('short approval forwards the exact human text after persisted reservation and acknowledgement', () => {
  for (const text of ['승인 C06', '승인 R01-DEPLOY_04', '승인 A' + '1'.repeat(23)]) {
    const state = { deliveries: {} };
    let saved = 0, acked = false;
    assert.equal(acceptReply(state, { ...reply, text }, config, () => { saved++; }, () => { acked = true; },
      (received, id, thread) => {
        assert.equal(saved, 1);
        assert.equal(acked, true);
        assert.equal(state.deliveries[id].status, 'pending');
        assert.deepEqual([received, id, thread], [text, reply.ts, reply.thread_ts]);
        return true;
      }), 'queued');
    assert.equal(saved, 2);
    assert.equal('text' in state.deliveries[reply.ts], false);
  }
});

test('short approval keeps workspace, human, thread, channel, bot and edit restrictions', () => {
  const event = { ...reply, text: '승인 C06' };
  const envelope = { type: 'events_api', payload: { type: 'event_callback', team_id: config.team, event } };
  for (const change of [{ user: 'other' }, { thread_ts: 'other' }, { channel: 'other' },
    { bot_id: 'bot' }, { app_id: 'app' }, { subtype: 'message_changed' }]) {
    assert.equal(acceptEnvelope({ deliveries: {} }, { ...envelope, payload: { ...envelope.payload, event: { ...event, ...change } } },
      config, () => assert.fail(), () => {}, () => assert.fail()), 'ignored');
  }
  assert.equal(acceptEnvelope({ deliveries: {} }, { ...envelope, payload: { ...envelope.payload, team_id: 'other' } },
    config, () => assert.fail(), () => {}, () => assert.fail()), 'ignored');
});

test('short approval duplicate ID remains blocked across delivery outcomes', () => {
  const event = { ...reply, text: '승인 C06' };
  const state = { deliveries: {} };
  assert.equal(acceptReply(state, event, config, () => {}, () => {}, () => false), 'needs_review');
  assert.equal(acceptReply(state, event, config, () => assert.fail(), () => {}, () => assert.fail()), 'duplicate');
  state.deliveries[event.ts].status = 'pending';
  assert.equal(acceptReply(state, event, config, () => assert.fail(), () => {}, () => assert.fail()), 'duplicate');
});

test('short approvals reject malformed, quoted, multiline and appended commands without trimming', () => {
  for (const text of [undefined, null, 7, {}, ['승인 C06'], '승인', '승인 ', '승인 c06', '승인 06',
    '승인 C06/r1', '승인 A' + '1'.repeat(24), '승인 C06 계속', '승인 C06; restart', '승인 C06 && restart',
    '승인 C06\n', '승인 C06\r', '승인 C06\r\n', '승인 C06\nrestart', '\n승인 C06',
    '> 승인 C06', '"승인 C06"', '`승인 C06`', ' 승인 C06', '승인 C06 ', '승인\tC06', '승인 C06\t']) {
    let acked = false;
    assert.equal(acceptReply({ deliveries: {} }, { ...reply, text }, config, () => assert.fail(),
      () => { acked = true; }, () => assert.fail()), 'ignored');
    assert.equal(acked, true);
  }
});

test('legacy coord commands keep prefix removal and outer trimming', () => {
  const sent = [];
  assert.equal(acceptReply({ deliveries: {} }, { ...reply, text: '!coord   confirm  ' }, config, () => {}, () => {},
    text => { sent.push(text); return true; }), 'queued');
  assert.deepEqual(sent, ['confirm']);
});
