import { appendFileSync, mkdirSync } from 'node:fs';
import path from 'node:path';
import { randomUUID } from 'node:crypto';

// Protocol metadata only: never log arguments, model data, credentials or images.
export function auditTransport(transport, filename) {
  const session = randomUUID();
  let client = null;
  const pending = new Map();
  function record(event) {
    try {
      mkdirSync(path.dirname(filename), { recursive: true });
      appendFileSync(filename, JSON.stringify({ time: new Date().toISOString(),
        session, pid: process.pid, parentPid: process.ppid, client, ...event }) + '\n');
    } catch { /* Diagnostics must never break the protocol. */ }
  }
  const send = transport.send.bind(transport);
  transport.send = async (message, ...rest) => {
    await send(message, ...rest);
    const request = pending.get(message.id);
    if (!request) return;
    pending.delete(message.id);
    record({ event: 'response', ...request,
      success: !message.error && message.result?.isError !== true,
      ...(request.method === 'tools/list' ? { tools: message.result?.tools?.map(t => t.name) ?? [] } : {}) });
  };
  record({ event: 'process_started' });
  return () => {
    const receive = transport.onmessage;
    transport.onmessage = (message, extra) => {
      if (message.method === 'initialize') {
        const info = message.params?.clientInfo;
        client = info ? { name: String(info.name).slice(0, 120), version: String(info.version).slice(0, 80) } : null;
      }
      if (message.id !== undefined && ['initialize', 'tools/list', 'tools/call'].includes(message.method)) {
        pending.set(message.id, { method: message.method,
          ...(message.method === 'tools/call' ? { tool: String(message.params?.name).slice(0, 100) } : {}) });
      }
      receive?.(message, extra);
    };
  };
}
