const length = Symbol("length");

type CallbackType = "before" | "after";

interface Callback {
  (...args: any): void | Promise<void>;
}

interface PluginContainer {
  impl: null | object;
  state: "preInit" | "init" | "postInit" | "fulfilled"
}

type CallbackContainer = {
  [length]: number;
} & {
  [k in CallbackType]?: Callback[];
}

class RCP {
  static readonly PREF = "riotPlugin.announce:";
  static readonly PREF_LEN = this.PREF.length;

  static isAnnounceEvent(event: Event): event is RcpAnnouceEvent {
    return event.type.startsWith(this.PREF);
  }

  constructor() {
    const self = this;
    const { dispatchEvent } = document;
    function dispatchEventWrap(this: any, event: Event): boolean {
      if (RCP.isAnnounceEvent(event)) self.onPluginAnnounce(event);
      return dispatchEvent.call(this, event);
    }
    Object.defineProperty(document, "dispatchEvent", { value: dispatchEventWrap });
  }

  private readonly pluginRegistry = new Map<string, PluginContainer>();
  private readonly callbacks = new Map<string, CallbackContainer>();

  private onPluginAnnounce(event: RcpAnnouceEvent) {
    const self = this;
    const name = event.type.substring(RCP.PREF_LEN);
    
    const { registrationHandler } = event;

    function registrationHandlerWrap(this: any, registrar: Parameters<typeof registrationHandler>[0]): ReturnType<typeof registrationHandler> {
      return registrationHandler.call(this, async function(provider) {
        const container: PluginContainer = { impl: null, state: "preInit" };
        self.pluginRegistry.set(name, container);
        // callbacks called immidiatly, without adding microtasks to the queue
        // so, no edgecases where pre/post init callback added after it already
        // cant be called, but before state of plugin changes (point of not doing it after await)
        await self.invokeCallbacks("before", name, () => { container.state = "init"; }, provider);
        const api = (container.impl = await registrar(provider));
        container.state = "postInit";
        await self.invokeCallbacks("after", name, () => { container.state = "fulfilled"; }, api);
        return api;
      });
    }
  
    Object.defineProperty(event, "registrationHandler", {
      value: registrationHandlerWrap,
    })
  }

  private async invokeCallbacks(type: CallbackType, name: string, callback: () => void, ...args: any[]) {
    const container = this.callbacks.get(name);
    if (container == undefined) return void callback();
    const callbacks = container[type];
    if (callbacks == undefined) return void callback();
    //while older callbacks dont finished, new ones stll can be added 
    while (callbacks.length > 0) do {
      container[length] -= callbacks.length;
      await Promise.allSettled(callbacks.splice(0).map(callback => callback(...args)));
    } while (callbacks.length > 0);
    if (container[length] == 0) this.callbacks.delete(name);
    callback();
  }

  private addCallback(type: CallbackType, name: string, callback: Callback) {
    let container = this.callbacks.get(name);
    if (container == undefined) this.callbacks.set(name, (container = { [type]: [], [length]: 0 }));
    const arr = container[type] ?? (container[type] = []);
    container[length]++;
    arr.push(callback);
  }

  public preInit(name: string, callback: (provider: any) => any): boolean {
    name = String(name);
    if (typeof callback !== "function") throw new TypeError(`${callback} is not a function`);
    const plugin = this.pluginRegistry.get(name);
    if (plugin == undefined || plugin.state == "preInit") return (this.addCallback("before", name, callback), true);
    return false;
  }

  public postInit(name: string, callback: (api: any) => any, blocking: boolean = false){
    name = String(name);
    if (typeof callback !== "function") throw new TypeError(`${callback} is not a function`);
    const plugin = this.pluginRegistry.get(name);
    if (plugin !== undefined && plugin.state === "fulfilled") return false;
    this.addCallback("after", name, blocking ? callback : (api: any) => void callback(api));
    return true;
  }

  private whenReadyOne(name: string) {
    return new Promise(resolve => {
      if (!this.postInit(name, resolve)) resolve(this.pluginRegistry.get(name)!.impl);
    });
  }

  private whenReadyAll(names: string[]) {
    return Promise.all(names.map(name => this.whenReadyOne(String(name))));
  }

  public whenReady(param){
    if (typeof param == "string") return this.whenReadyOne(param);
    if (Array.isArray(param)) return this.whenReadyAll(param);
    throw new TypeError(`unexpected argument ${param}`);
  }
  public get(name: string){
    name = String(name).toLowerCase();
    if (!name.startsWith('rcp-')) name = 'rcp-' + name;
    return this.pluginRegistry.get(name)?.impl;
  }

  // [Symbol.iterator](){
  //   return this.pluginRegistry.entries();
  // }
}

export const rcp = new RCP();

Object.defineProperty(window, "rcp", {
  value: rcp,
  enumerable: false,
  configurable: false,
  writable: false
})