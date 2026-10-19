import { existsSync, readFileSync, writeFileSync, renameSync } from 'node:fs';

export function loadState(path) {
  return existsSync(path) ? JSON.parse(readFileSync(path, 'utf8')) : { thread: null, deliveries: {} };
}

export function saveState(path, state) {
  writeFileSync(`${path}.tmp`, JSON.stringify(state, null, 2));
  renameSync(`${path}.tmp`, path);
}

// A persisted reservation prevents replay after a crash between queueing and saving success.
// Such ambiguous reservations require manual review, never an automatic retry.
export function acceptEnvelope(state, envelope, config, persist, ack, deliver) {
  if (envelope?.type !== 'events_api' || envelope.payload?.type !== 'event_callback' ||
      envelope.payload?.team_id !== config.team) {
    ack();
    return 'ignored';
  }
  return acceptReply(state, envelope.payload.event, config, persist, ack, deliver);
}

export function acceptReply(state, event, config, persist, ack, deliver) {
  if (event?.type !== 'message' || event.subtype || event.bot_id || event.app_id ||
      event.channel !== config.channel || event.user !== config.user ||
      !config.threads.has(event.thread_ts) ||
      !/^\d+\.\d+$/.test(event.ts ?? '') || typeof event.text !== 'string') {
    ack();
    return 'ignored';
  }
  // Keep legacy prefix stripping and trimming. Short approvals match the whole raw
  // single line without trimming, so quotes, surrounding whitespace and suffixes fail.
  const text = event.text.startsWith('!coord ') ? event.text.slice(7).trim() :
    (!/[\r\n]/.test(event.text) && /^승인 [A-Z][A-Z0-9_-]{0,23}$/.test(event.text) ? event.text : '');
  if (!text || text.length > 4000) { ack(); return 'ignored'; }
  if (state.deliveries[event.ts]) {
    ack();
    return 'duplicate';
  }
  state.deliveries[event.ts] = { status: 'pending', thread: event.thread_ts };
  persist();
  ack();
  state.deliveries[event.ts].status = deliver(text, event.ts, event.thread_ts) ? 'queued' : 'needs_review';
  persist();
  return state.deliveries[event.ts].status;
}
