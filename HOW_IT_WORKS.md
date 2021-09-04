The original idea from [Mecha](https://github.com/x00bence/Mecha).

They used **Image File Execution** to inject the payload into **LeagueClientUx.exe**,

Their injector used MS Detours to start Ux process with flag DEBUG_ONLY_THIS_PROCESS, turning debug mode may affect performance.

> That's against Riot Privacy Policy, so you might get banned.

> Since the patch 11.17, League Client is updated from CEF 74 -> 91, so any previous built of Mecha is not working.

### Hooking

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

window.require = (name) => {
    const dir = paths[paths.length - 1]
    const path = join(dir, name)
    const mod = modules[path]
    
    if (mod) return mod.exports
    else {
        const source = Require(path)
        
        if (source === null) {
            paths.pop()
            // module not found
        } else {
            const _M = { source, exports: {} }
            try {
                paths.push(join(path, '..'))
                Function('module', 'exports', 'global', source)
                    (_M, _M.exports, global)
            } catch (err) {  
                // error
            } finally {
                paths.pop()
            }
            
            modules[path] = _M
            return _M.exports
        }
    }
}
```

If we resolve the module name is a folder which contains index.js,
we will map the exported for both `/module_name` and `/module_name/index.js`.

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

You can run the above code in your browser and click on the output stack trace (at `123    demo.js`).

See the full code of our built-in extension [here](/d3d9/src/ext_code.h).
