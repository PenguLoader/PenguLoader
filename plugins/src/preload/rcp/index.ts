interface Callback {
  pre: boolean,
  callback: Function,
}

const callbacksMap = new Map<string, Callback[]>();

function subscribeRcp(name: string) {
  function listener(event: RcpAnnouceEvent) {
    const handler = event.registrationHandler;

    event.registrationHandler = function (registrar) {
      handler(async (context) => {
        const callbacks = callbacksMap.get(name)!;

        await Promise.allSettled(
          callbacks.filter(c => c.pre)
            .map(c => c.callback())
        );

        // console.log(context);
        const api = await registrar(context);

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

function preInit(name: string, callback: () => any) {
  if (typeof name === 'string' && typeof callback === 'function') {
    addHook(name, true, callback);
  }
}

function postInit(name: string, callback: (api: any) => any) {
  if (typeof name === 'string' && typeof callback === 'function') {
    addHook(name, false, callback);
  }
}

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

export const rcp = {
  preInit,
  postInit,
  whenReady,
};