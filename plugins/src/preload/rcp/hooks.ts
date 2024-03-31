type CallbackType = "before" | "after";

interface Callback {
  (...args: any): void | Promise<void>;
}

type CallbackContainer = {
  [length]: number;
} & {
  [k in CallbackType]?: Callback[];
}

const length = Symbol("length");

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

  private readonly pluginRegistry = new Map<string, any>();
  private readonly callbacks = new Map<string, CallbackContainer>();

  private onPluginAnnounce(event: RcpAnnouceEvent) {
    const self = this;
    const name = event.type.substring(RCP.PREF_LEN);
    
    const { registrationHandler } = event;

    function registrationHandlerWrap(this: any, registrar: Parameters<typeof registrationHandler>[0]): ReturnType<typeof registrationHandler> {
      return registrationHandler.call(this, async function(provider) {
        await self.invokeCallbacks("before", name, provider);
        const api = await registrar(provider);
        self.pluginRegistry.set(name, api);
        await self.invokeCallbacks("after", name, api);
        return api;
      });
    }
  
    Object.defineProperty(event, "registrationHandler", {
      value: registrationHandlerWrap,
    })
  }

  private invokeCallbacks(type: CallbackType, name: string, ...args: any[]) {
    const container = this.callbacks.get(name);
    if (container == undefined) return;
    const callbacks = container[type];
    if (callbacks == undefined) return;
    if ((container[length] -= callbacks.length) == 0) this.callbacks.delete(name);
    const tasks: any[] = [];
    for (const callback of callbacks) tasks.push(callback(...args));
    callbacks.length = 0;
    return Promise.allSettled(tasks);
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
    if (this.pluginRegistry.has(name)) return false;
    this.addCallback("before", name, callback);
    return true;
  }

  public postInit(name: string, callback: (api: any) => any, blocking: boolean = false){
    name = String(name);
    if (typeof callback !== "function") throw new TypeError(`${callback} is not a function`);
    if (this.pluginRegistry.has(name)) return false;
    this.addCallback("after", name, callback);
    return true;
  }

  private whenReadyOne(name: string) {
    const plugin = this.pluginRegistry.get(name);
    if (plugin !== undefined) return Promise.resolve(plugin);
    return new Promise<any>(resolve => {
      this.postInit(name, resolve);
    });
  }
  private whenReadyAll(names: string[]) {
    const tasks: any[] = [];
    for (const name of names) tasks.push(this.whenReadyOne(name));
    return Promise.all(tasks);
  }

  public whenReady(param){
    if (typeof param == "string") return this.whenReadyOne(param);
    if (Array.isArray(param)) return this.whenReadyAll(param);
    throw new TypeError(`unexpected argument ${param}`);
  }
  public get(name: string){
    name = String(name).toLowerCase();
    if (!name.startsWith('rcp-')) name = 'rcp-' + name;
    return this.pluginRegistry.get(name);
  }

  [Symbol.iterator](){
    return this.pluginRegistry.entries();
  }
}

export const rcp = new RCP();

Object.defineProperty(window, "rcp", {
  value: rcp,
  enumerable: false,
  configurable: false,
  writable: false
})