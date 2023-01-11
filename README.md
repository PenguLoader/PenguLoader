<img align="left" src="https://i.imgur.com/ZhWCav3.png" width="140px">

# League Loader
A small **JavaScript plugin loader** for League Client, supports **CommonJS** modules.

<br>
<br>

## Features
- Customize League Client
- Unlock insecure options
- Support built-in and remote DevTools
- Support custom plugins
- Support CommonJS modules
- Interacting LCU APIs be easier

<br>

## Getting started

1. Download the [latest release](https://github.com/nomi-san/league-loader/releases) and extract it.
2. Run **League Loader.exe**
3. Select League Client path
4. Click **INSTALL**
5. Launch League Client

<p align="center">
  <img src="https://i.imgur.com/mDihNl7.png">
</p>

After League ready, just open the settings panel to ensure the default plugin loaded.

<br>
<p align="center">
  <img src="https://i.imgur.com/E8gnK5W.png">
</p>
<br>

Now you'll see like above this, just press:
- <kbd>F12</kbd> or <kbd>Ctrl Shift I</kbd> to open DevTools
- <kbd>Ctrl Shift R</kbd> to reload the client instantly

<br>

> Do not touch **insecure options** without knowledge of them, you might get banned (about 3%).

<br>

## JavaScript plugins

To add a plugin, just create a `.js` file in the plugins folder.

```js
// hello.js
console.log('Hello, League Client!')
```

All .js files in root of plugins folder will be executed after League ready, except file name starts with underscore.

```
plugins/
  _util.js      
  demo.js       ; will be executed after League starts.
```

### CommonJS modules

We provided a simple implementation to support CommonJS modules. Each plugin file is a module, `require`, `global` and `module` are available variables.

```js
// _util.js
module.exports = {
    greet: () => console.log('Hello, world')
}

// demo.js
const util = require('./_util')
util.greet()
```

We also support to require JSON and text files.
```js
require('data.json') // -> parsed JSON
require('data.raw')  // -> string
```

To store data globally, you can use `window` object or `global`.
```js
window.my_str = 'ABC'
global.my_num = 100
```

To open **DevTools**, just call:
```js
window.openDevTools()
```

To reload plugins, just reload the Client (or Ctrl + R in DevTools):
```js
window.location.reload()
```

We have an example which modifies the settings panel to add some controls, see [league-loader.js](/bin/plugins/league-loader.js).

### Theme the League Client

To change the default style, just add your CSS:

```js
function addCss(filename) {
  const style = document.createElement('link')
  style.href = filename
  style.type = 'text/css'
  style.rel = 'stylesheet'
  document.body.append(style)
}

window.addEventListener('DOMContentLoaded', () => {
  addCss('https://webdevtestbutch.000webhostapp.com/assets/Noxius.css')
})
```

To use a local CSS file, just require it:

```js
function insertCss(css) {
    const style = document.createElement('style')
    style.textContent = css
    document.body.append(style)
}

window.addEventListener('DOMContentLoaded', () => {
  insertCss(require('my.theme.css'))
})
```

### LCU API requests

Use `fetch` to make a LCU request:
```js
function acceptMatchFound() {
  fetch('/lol-matchmaking/v1/ready-check/accept', {
    method: 'POST'  
  })
}
```

### LCU Websocket

When the websocket ready, this link tag will appear:
```html
<link rel="riot:plugins:websocket" href="wss://riot:hq5DDz5c8uLLc-dMRC1HGQ@127.0.0.1:50302/">
```

Call this function to subscribe API event:
```js
function subscribe() {
  const uri = document.querySelector('link[rel="riot:plugins:websocket"]').href
  const ws = new WebSocket(uri, 'wamp')
  
  ws.onopen = () => ws.send(JSON.stringify([5, 'JsonApiEvent']))
  ws.onmessage = async message => {
    const data = JSON.parse(message.data)
    console.log(data)
    // ...
  }
}
```

### Development notes

- You should use **Visual Studio Code** to develop your plugins, it supports intellisense, linter and autocomplete for modules.
- League Client runs on **Chromium 91**, so you're writing JS for the web browser like **Chrome**.
- When interacting with the DOM, you should put the entry to `onload` or `DOMContentLoaded` event of `window`.

### Native ESM supports

Plugin scripts are loaded as classic module, so you cannot use top-level `import`.

Let's wrap your code:

```js
(() => import('https://your-cdn.com/plugin.js'))();
```

Then use ESM in your plugin module:
```js
// https://your-cdn.com/plugin.js
import axios from 'https://cdn.skypack.dev/axios'
axios.get(...)
```

To import Axios locally, just use `import` function:
```js
async function test() {
  const { default: axios } = await import('https://cdn.skypack.dev/axios')
  const { data } = await axios.get('/performance/v1/memory')
  console.log(data)
}
```

Recommended CDNs:
- https://www.skypack.dev/
- https://esm.sh/
- https://www.jsdelivr.com/esm
- https://unpkg.com/

## Build from source

This project requires Visual Studio 2017 with these components:
- Desktop development with C++
- .NET desktop development
- Windows 8.1 SDK & .NET Framework 4.5

You can also use VS2015+ and different SDK version.

Build steps:
  1. Open **league-loader.sln** in VS
  2. Right click on the solution -> **Restore Nuget Packages**
  3. Set arch to **Any CPU** or **x86**
  4. Right click on each project -> **Build**

## How it works?

See [HOW_IT_WORKS](/HOW_IT_WORKS.md) for details.

### CEF notes

Current CEF version: **v91.1.22**

This project use CEF CAPI in C++. If you need sweet C++ OOP, just use libcef wrapper.

Let's download our [pre-built here](https://github.com/nomi-san/league-loader/releases/tag/0.1a), then add the following code.

```cpp
#pragma comment(lib, "libcef.lib")
#pragma comment(lib, "libcef_dll_wrapper.lib")
#define WRAPPING_CEF_SHARED
#include "libcef_dll/ctocpp/browser_ctocpp.h"

void callback(cef_browser_t *cbrowser) {
  auto browser = CefBrowserCToCpp::Wrap(cbowser);
  aut host = browser->GetHost();
  host->GetMainFrame();
}
```
