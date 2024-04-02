import { builtinUrls } from "./builtins.js"

const primordials: Primordials = (window as any).__native;
delete (window as any).__native;
// (window as any).__native = undefined;
console.log(primordials);

Object.defineProperty(Pengu, "primordials", {
    configurable: false,
    enumerable: false,
    get() {
        const stack = new Error().stack!;
        const rawLine = stack.split("\n")[2].trim().substring(3);
        let s: number = (rawLine.indexOf("(") + 1), e: number;
        if ((e = rawLine.lastIndexOf(")")) == -1) e = rawLine.length;
        const line = rawLine.substring(s, e);
        const caller = line.substring(0, line.lastIndexOf(":", line.lastIndexOf(":") - 1));
        if (builtinUrls.has(caller)) return primordials;
    }
})