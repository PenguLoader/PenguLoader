import { rcp } from './hooks';

interface EventData {
  data: any;
  uri: string;
  eventType: 'Create' | 'Update' | 'Delete';
}

interface ApiListener {
  (message: EventData): void;
}

let ws: WebSocket;
const eventQueue = Array<string>();
const listenersMap = new Map<string, Array<ApiListener>>();

rcp.preInit('rcp-fe-common-libs', async function (provider) {
  const { _endpoint } = provider.context.socket;
  ws = new WebSocket(_endpoint, 'wamp');
  ws.addEventListener('open', () => {
    for (const e of eventQueue.splice(0, eventQueue.length)) {
      ws.send(JSON.stringify([5, e]));
    }
  });
  ws.addEventListener('message', handleMessage);
  window.addEventListener('beforeunload', () => ws.close());
});

function handleMessage(e: MessageEvent<string>) {
  const [type, endpoint, data] = JSON.parse(e.data);
  if (type === 8 && listenersMap.has(endpoint)) {
    const listeners = listenersMap.get(endpoint)!;
    for (const callback of listeners) {
      setTimeout(() => callback(<EventData>data), 0);
    }
  }
}

function buildApi(api: string): string {
  if (api === 'all') return 'OnJsonApiEvent';
  api = api.toLowerCase().replace(/^\/+|\/+$/g, '');
  return 'OnJsonApiEvent_' + api.replace(/\//g, '_');
}

function observe(api: string, listener: ApiListener) {
  if (typeof api !== 'string' || api === ''
    || typeof listener !== 'function')
    return false;

  const endpoint = buildApi(api);
  listener = listener.bind(self);

  if (listenersMap.has(endpoint)) {
    const arr = listenersMap.get(endpoint);
    arr!.push(listener);
  } else {
    listenersMap.set(endpoint, [listener]);
  }

  if (ws?.readyState === 1) {
    ws.send(JSON.stringify([5, endpoint]));
  } else {
    eventQueue.push(endpoint);
  }

  return {
    disconnect: () => disconnect(api, listener),
  };
}

function disconnect(api: string, listener: ApiListener) {
  const endpoint = buildApi(api);
  if (listenersMap.has(endpoint)) {
    const arr = listenersMap.get(endpoint)!.filter(x => x !== listener);
    if (arr.length === 0) {
      ws.send(JSON.stringify([6, endpoint]));
      listenersMap.delete(endpoint);
    } else {
      listenersMap.set(endpoint, arr);
    }
    return true;
  }
  return false;
}

export const socket = {
  observe,
  disconnect,
};