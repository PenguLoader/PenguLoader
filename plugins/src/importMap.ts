import { builtinNames } from "./builtins.js"

document.addEventListener("DOMContentLoaded", function () {
    const script = document.createElement("script");
    script.type = "importmap";
    const importmap: { imports: Record<string, string> } = { imports: {} };
    for (const pluginRoot in window.Pengu.__plugins) {
        const pkg = JSON.parse(window.Pengu.__plugins[pluginRoot]);
        importmap.imports[pluginRoot] = `https://plugins/${pluginRoot}/${pkg.main}`;
    }
    for (const [key, value] of builtinNames) {
        importmap.imports[key] = value;
    }
    script.append(document.createTextNode(JSON.stringify(importmap, null, 2)));
    document.head.append(script);
    document.dispatchEvent(new CustomEvent("Pengu.importmap.ready"));
})