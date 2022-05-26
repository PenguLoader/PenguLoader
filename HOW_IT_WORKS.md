The original idea from [Mecha](https://github.com/x00bence/Mecha).

They used **Image File Execution** to inject the payload into **LeagueClientUx.exe**,

Their injector used MS Detours to start Ux process with flag DEBUG_ONLY_THIS_PROCESS, turning debug mode may affect performance.

> Anything's against Riot Privacy Policy might lead to banned, we've seen a few people get banned from using Mecha.
> Since the patch 11.17, League Client is updated from CEF 74 -> 91, so any previous built of Mecha was not working.

### Hooking

We use different approach, "passive injection" via D3D9 proxying.
We don't know about the origin of this technique, but you can see it in [ENBSeries](http://enbdev.com/).

Original d3d9.dll is a Direct3D 9 Runtime – a dependency of libcef.dll.
Our dll is a proxy to load needed D3D9 APIs and hooks back libcef.dll.

Here is our hooking strategy:

```
d3d9.dll
> LeagueClientUx.exe (browser process)
    + Load CEF functions
    + Hook cef_initialize(), cef_browser_host_create_browser()
    + Modify command line/settings
    + Provide DevTools
> LeagueClientUxRender.exe (rerderer process, arg: --type=renderer)
    + Load CEF functions
    + Hook cef_execute_process()
    + Register extension
    + Load plugins
```

LeagueClientUx.exe is the browser process for graphics rendering, UI, IO...
And LeagueClientUxRender.exe is the renderer process for V8 and Blink.

### Configuration

We provide Winforms GUI for setting up the loader and controlling League Client remotely.

In the first version, we just copy our d3d9.dll to League Client folder and set the path of loader folder in env. This approach works too slow when you're opening too many programs. So the next version, we create symlink in League Client folder which points to the loader folder, and in d3d9.dll, we just reference to the final path to get settings and plugins.

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

#### Source traceability

In Chromium browser based, the `Error` has no `fileName` and `lineNumber`, these are not the standard.
To make the error output extractly in DevTools and traceable, we just add the sourceURL to it.

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
