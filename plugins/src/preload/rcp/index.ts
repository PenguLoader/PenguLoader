interface Callback {
  pre: boolean,
  callback: Function,
}

const pluginMap = new Map<string, any>();
const callbacksMap = new Map<string, Callback[]>();

function subscribeRcp(name: string) {
  function listener(event: RcpAnnouceEvent) {
    const handler = event.registrationHandler;

    event.registrationHandler = function (registrar) {
      handler(async (provider) => {
        const callbacks = callbacksMap.get(name)!;

        await Promise.allSettled(
          callbacks.filter(c => c.pre)
            .map(c => c.callback(provider))
        );

        const api = await registrar(provider);
        pluginMap.set(name, api);

        callbacks.filter(c => !c.pre)
          .forEach(c => c.callback(api));

        return api;
      });
    };
  }

  const type = `riotPlugin.announce:${name}`;
  document.addEventListener(type, <any>listener, {
    once: true,
    capture: false
  });
}

function addHook(name: string, pre: boolean, callback: Function) {
  let callbacks: Callback[];

  if (callbacksMap.has(name)) {
    callbacks = callbacksMap.get(name)!;
  } else {
    callbacksMap.set(name, callbacks = []);
    subscribeRcp(name);
  }

  callbacks.push({
    pre,
    callback
  });
}

function preInit(name: string, callback: (provider: any) => any) {
  if (typeof name === 'string' && typeof callback === 'function') {
    addHook(name, true, callback);
  }
}

function postInit(name: string, callback: (api: any) => any) {
  if (typeof name === 'string' && typeof callback === 'function') {
    addHook(name, false, callback);
  }
}

// Wait for plugin(s) loaded asynchronously
function whenReady(name: string): Promise<any>;
function whenReady(names: string[]): Promise<any[]>;
function whenReady(param) {
  if (typeof param === 'string') {
    return new Promise<any>(resolve => {
      postInit(param, resolve);
    });
  } else if (Array.isArray(param)) {
    return Promise.all(param.map(name =>
      new Promise<any>(resolve => {
        postInit(name, resolve);
      })
    ));
  }
}

// Get a plugin sunchronously, returns undefined if it's not loaded
function get(name: string): object | undefined {
  name = String(name).toLowerCase();
  if (!name.startsWith('rcp-'))
    name = 'rcp-' + name;
  return pluginMap.get(name);
}

export const rcp = {
  get,
  preInit,
  postInit,
  whenReady,
};