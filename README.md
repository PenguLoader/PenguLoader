<img align="left" src="https://i.imgur.com/uM5xPEa.png" width="120px">

# Susan
A small **plugin loader** for **League Client**, supports **CommonJS** modules.

<br>
<br>

> WORKING IN PROGRESS

## Features
// TODO

## Getting started

// TODO

### Plugins

To add your plugin, just create a `.js` file in the plugins folder.
All plugin files in top of folder will be executed, except file name starts with underscore.

```
plugins/
  _util.js      
  demo.js       ; will be executed after League starts.
```

Now you can use ES6 and CommonJS modules in your plugin.

```js
// _util.js
module.exports = {
    greet: () => console.log('Hello, world')
}

// demo.js
const util = require('./_util')
util.greet()
```

To store data globally, you can use `window` object or `global`.
```js
window.my_str = 'ABC'
global.my_num = 100
```

To open DevTools, you just call:
```js
window.openDevTools()
```

We have an example which modifies the settings UI to preppend a DevTools opener, see [dev-tools.js](/plugins/dev-tools.js).

#### Development notes

You should use Visual Studio Code to develop your plugins, it supports intellisense, linter and autocomplete for modules.
The JS runtime is V8 engine in browser, that means you are writing JS for web browser.

Plugins start loading after JS context created, to interact with the DOM, you should add your code to `window`'s `onload` or `DOMContentLoaded` event.

## How it works?

The original idea from [Mecha](https://github.com/x00bence/Mecha).

They used **Image File Execution** to debug **LeagueClientUx.exe**,
and their debugger (**mecha_injector.exe**) was an injector, which is used to inject **mecha_payload.dll** into Ux process.

The debugger used MS Detours to start Ux process with flag DEBUG_ONLY_THIS_PROCESS, turning debug mode may affect about performance.
That works fine, but against Riot security, so you might get banned.

> Since the patch 11.17, League Client is updated from CEF 74 -> 91, so any previous built of Mecha is not working.

###  Hooking

We use "passive injection" technique and provide d3d9.dll, called D3D9 injection.
We don't know about the origin of this technique, but you can see it from [ENBSeries](http://enbdev.com/).

Original d3d9.dll is a Direct3D 9 Runtime – a dependency of libcef.dll.
Our dll is a proxy to load needed D3D9 APIs and hooks back libcef.dll.

Here is our hooking strategies:

```
d3d9.dll
> LeagueClientUx.exe
    + Load CEF functions
    + Hook cef_initialize(), cef_browser_host_create_browser()
    + Modify command line/settings
    + Provide DevTools
> LeagueClientUxRender.exe (only arg --type=renderer)
    + Load CEF functions
    + Hook cef_execute_process()
    + Register extension
    + Load plugins
```

LeagueClientUx.exe is the browser process for graphics rendering, UI, IO...
And LeagueClientUxRender.exe is the renderer process for V8 and Blink.

### DevTools
We support two types of DevTools: `built-in` and `remote`.

The remote DevTools URL can be retrieved by `localhost:REMOTE_DEBUGGING_PORT/json` if `--remote-debugging-port` is enabled in CEF.

To open built-in DevTools by JavaScript, we invoke the built-in method in browser process through CreateRemoteThread.

### CommonJS

We handle modules in JavaScript, in renderer processs only.

The concept of creating CommonJS:

```js
const global = {}
const modules = {}
const paths = ['']

function require(name) {
    const dir = paths[paths.length - 1]
    const paht = join(dir, name)
    const mod = paths[path]
    
    if (mod) return mod.exports
    else {
        paths.push(join(path, '..'))
        const source = Require(path)
        
        if (source === null) {
            paths.pop()
            // module not found
        } else {
            const _M = { exports: {} }
            const exec = Function('module', 'exports', 'global', source)
            exec(_M, _M.exports, global)
            
            modules[path] = {
                source,
                exports: _M.exports
            }
            
            paths.pop()
            return _M.exports
        }
    }
}
```

If we resolve the module name is a folder which contains index.js,
we will map the exported for both `/module_name` and `/module_name/index.js`.

So the native `Require()` looks like this:
```js
function Require(path) {
    var plugin = PLUGINS[path + '.js']
    if (plugin.isFile()) {
        return [plugin.text(), false]
    } else if (PLUGINS[path].isDir()) {
        plugin = PLUGINS[path + '/index.js']
        if (plugin.isFile()) {
            return [plugin.text(), true]
        }
    }
    return null
}
```

In Chromium browser based, the `Error` has no `fileName` and `lineNumber`, these are not the standard.
To make the error output extractly in DevTools, we just add the sourceURL to it.

```js
Function(source + '\n//# sourceURL=filename.js')
```

And here is why we use `eval()` inside `Function()`:

```js
var src = `console.log(123)`
Function('module', src + '\n//# sourceURL=demo.js')({})
Function('module',  `"use strict";eval(module.src);`)
    ({ src: src + '\n//# sourceURL=demo2.js' })

// demo.js
// 1. (function anonymous(module
// 2. ) {
// 3. console.log(123)
// 4. //# sourceURL=demo.js
// 5. })

// demo2.js
// 1. console.log(123)
```

You can run the above code in your browser and click on the output stack trace (at `123`).

See the full code of our built-in extension [here](/d3d9/src/ext_code.h).
