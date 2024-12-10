import { rcp } from './rcp';

interface EventData {
  data: any;
  uri: string;
  eventType: 'Create' | 'Update' | 'Delete';
}

interface ApiListener {
  (message: EventData): void;
}

let _ws: WebSocket;
const _eventQueue = Array<string>();
const _listenersMap = new Map<string, Array<ApiListener>>();

rcp.preInit('rcp-fe-common-libs', async function (provider) {
  const { _endpoint } = provider.context.socket;
  _ws = new WebSocket(_endpoint, 'wamp');
  _ws.addEventListener('open', () => {
    for (const e of _eventQueue.splice(0, _eventQueue.length)) {
      _ws.send(JSON.stringify([5, e]));
    }
  });
  _ws.addEventListener('message', handleMessage);
  window.addEventListener('beforeunload', () => _ws.close());
});

function handleMessage(e: MessageEvent<string>) {
  const [type, endpoint, data] = JSON.parse(e.data);
  if (type === 8 && _listenersMap.has(endpoint)) {
    const listeners = _listenersMap.get(endpoint)!;
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

  if (_listenersMap.has(endpoint)) {
    const arr = _listenersMap.get(endpoint);
    arr!.push(listener);
  } else {
    _listenersMap.set(endpoint, [listener]);
  }

  if (_ws?.readyState === 1) {
    _ws.send(JSON.stringify([5, endpoint]));
  } else {
    _eventQueue.push(endpoint);
  }

  return {
    disconnect: () => disconnect(api, listener),
  };
}

function disconnect(api: string, listener: ApiListener) {
  const endpoint = buildApi(api);
  if (_listenersMap.has(endpoint)) {
    const arr = _listenersMap.get(endpoint)!.filter(x => x !== listener);
    if (arr.length === 0) {
      _ws.send(JSON.stringify([6, endpoint]));
      _listenersMap.delete(endpoint);
    } else {
      _listenersMap.set(endpoint, arr);
    }
    return true;
  }
  return false;
}

export const lcu = {
  observe,
  disconnect,
};