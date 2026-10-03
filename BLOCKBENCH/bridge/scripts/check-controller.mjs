export async function checkController(config, timeoutMs = 6000) {
  const response = await fetch(`http://127.0.0.1:${config.bridgePort}/rpc`, {
    method: 'POST',
    headers: {
      Authorization: `Bearer ${config.token}`,
      'Content-Type': 'application/json',
    },
    body: JSON.stringify({ name: 'status', args: {} }),
    signal: AbortSignal.timeout(timeoutMs),
  });
  const payload = await response.json();
  if (!response.ok || payload.error)
    throw new Error(
      payload.error || `Status RPC returned HTTP ${response.status}.`,
    );
  const status = payload.result;
  if (status?.connected === false || typeof status?.blockbench !== 'string')
    throw new Error('Blockbench controller did not return a valid status.');
  return status;
}
