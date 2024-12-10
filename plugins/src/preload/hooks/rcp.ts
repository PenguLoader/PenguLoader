type RcpAnnouceEvent = CustomEvent & {
  errorHandler: () => any;
  registrationHandler: (
    registrar: (provider) => Promise<any>
  ) => Promise<any> | void;
};

type CallbackType = 'before' | 'after';

interface Callback {
  (...args: any): void | Promise<void>;
}

interface PluginContainer {
  impl: null | object;
  state: 'preInit' | 'init' | 'postInit' | 'fulfilled';
}

type CallbackContainer = {
  _count: number;
} & {
  [k in CallbackType]?: Callback[]
};

const RCPE_PREF = 'riotPlugin.announce:';
const RCPE_PREF_LEN = RCPE_PREF.length;

const _plugins = new Map<string, PluginContainer>();
const _callbacks = new Map<string, CallbackContainer>();

function subscribePlugin(name: string) {
  const type = `${RCPE_PREF}${name}`;
  document.addEventListener(type, <any>onPluginAnnounce, {
    once: true,
    capture: false
  });
}

function onPluginAnnounce(event: RcpAnnouceEvent) {
  const name = event.type.substring(RCPE_PREF_LEN);
  const handler = event.registrationHandler;

  function handlerWrap(this: any, registrar: Parameters<typeof handler>[0]): ReturnType<typeof handler> {
    return handler.call(this, async (provider) => {
      const container: PluginContainer = { impl: null, state: 'preInit' };
      _plugins.set(name, container);

      await invokeCallbacks(name, 'before', provider);
      container.state = 'init';

      const api = await registrar(provider);
      container.impl = api;
      container.state = 'postInit';

      await invokeCallbacks(name, 'after', api);
      container.state = 'fulfilled';

      return api;
    });
  }

  Object.defineProperty(event, 'registrationHandler', {
    value: handlerWrap,
  });
}

async function invokeCallbacks(name: string, type: CallbackType, ...args: any[]) {
  const container = _callbacks.get(name);
  if (container === undefined)
    return;

  const callbacks = container[type];
  if (callbacks === undefined)
    return;

  while (callbacks.length > 0) {
    do {
      container._count -= callbacks.length;
      await Promise.allSettled(callbacks.splice(0).map(callback => callback(...args)));
    } while (callbacks.length > 0);
  }

  if (container._count === 0) {
    _callbacks.delete(name);
  }
}

function addCallback(name: string, callback: Callback, type: CallbackType) {
  let container = _callbacks.get(name);
  if (container === undefined) {
    container = {
      _count: 0,
      [type]: [],
    };
    _callbacks.set(name, container);
    subscribePlugin(name);
  }

  let callbacks = container[type];
  if (callbacks === undefined) {
    callbacks = [];
    container[type] = callbacks;
  }

  container._count++;
  callbacks.push(callback);
}

function ensureName(name: string) {
  name = String(name).toLowerCase();
  if (!name.startsWith('rcp-')) {
    return 'rcp-' + name;
  }
  return name;
}

function preInit(name: string, callback: (provider: any) => any): boolean {
  if (typeof callback !== 'function')
    throw new TypeError(`${callback} is not a function`);

  name = ensureName(name);
  const plugin = _plugins.get(name);

  if (plugin === undefined || plugin.state === 'preInit') {
    addCallback(name, callback, 'before');
    return true;
  }

  return false;
}

function postInit(name: string, callback: (api: any) => any, blocking: boolean = false) {
  if (typeof callback !== 'function')
    throw new TypeError(`${callback} is not a function`);

  name = ensureName(name);
  const plugin = _plugins.get(name);

  if (plugin !== undefined && plugin.state === 'fulfilled')
    return false;

  addCallback(name, blocking ? callback : (api: any) => void callback(api), 'after');
  return true;
}

function whenReadyOne(name: string) {
  return new Promise<any>(resolve => {
    if (!postInit(name, resolve)) {
      const plugin = _plugins.get(name)!;
      resolve(plugin.impl);
    }
  });
}

function whenReadyAll(names: string[]) {
  return Promise.all(names.map(name => whenReadyOne(name)));
}

function whenReady(name: string): Promise<any>;
function whenReady(names: string[]): Promise<any[]>;
function whenReady(param) {
  if (typeof param === 'string') {
    const name = ensureName(param);
    return whenReadyOne(name);
  }

  if (Array.isArray(param)) {
    const names = param.map(ensureName);
    return whenReadyAll(names);
  }

  throw new TypeError(`unexpected argument ${param}`);
}

function get(name: string) {
  name = ensureName(name);
  return _plugins.get(name)?.impl;
}

export const rcp = {
  preInit,
  postInit,
  whenReady,
  get,
};

Object.defineProperty(window, 'rcp', {
  value: rcp,
  enumerable: false,
  configurable: false,
  writable: false
});