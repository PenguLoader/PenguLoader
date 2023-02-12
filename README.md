<div align="center">
  <a href="https://leagueloader.app">
    <img src="https://user-images.githubusercontent.com/38210249/218244483-4f6fe136-fa7b-4732-9822-f41493c2f68a.png" width="144"/>
  </a>
  <h1 align="center">League Loader</h1>
  <p align="center">
    <strong>JavaScript plugin loader</strong> for League of Legends Client
  </p>
  <p>
    <a href="https://chat.leagueloader.app">
      <img src ="https://img.shields.io/badge/-Join%20Discord-7289da.svg?&style=for-the-badge&logo=Discord&logoColor=white"/>
    </a>
    <a href="https://github.com/nomi-san/league-loader">
      <img src="https://img.shields.io/github/stars/nomi-san/league-loader.svg?style=for-the-badge" />
    </a>
    <a href="./LICENSE">
      <img src ="https://img.shields.io/github/license/nomi-san/league-loader.svg?style=for-the-badge"/>
    </a>
  </p>
</div>

## About

League Loader is a plugin loader designed specifically for the League of Legends Client.

The League Client is actually an embedded Chromium web browser, and its interface is based on web technology. With League Loader, users can load JavaScript plugins into the Client as dependencies, allowing them to customize/theme the UI/UX, automate task and build a smarter Client.

League Loader was created to solve the problem caused by the big LoL patch in 2021, revived plugins from the death of Mecha. And now, Mecha has since returned as a debugger, but League Loader continues to thrive as the primary way for players looking to enhance their League Client with custom content, smarter functionality, and a personalized look and feel.

## Features
- Customize League Client with plugins
- Theme/personalize your Client
- Support modern JavaScript features
- Support built-in and remote DevTools
- Working with LCU APIs be easier

<br>

## Getting started

### Installation

1. Download the [latest release](https://github.com/nomi-san/league-loader/releases) and extract it
2. Run **League Loader.exe**
3. Select League Client path
4. Click **INSTALL**
5. Launch League Client

> To try preview features, you have to [build this project](#build-from-source) or download the latest auto-build in [Actions](https://github.com/nomi-san/league-loader/actions).

### Usage

After League Client ready, just click the settings button to ensure the default plugin loaded.

<br>
<p align="center">
  <img src="https://i.imgur.com/aNsUIQ8.png">
</p>
<br>

Now you'll see like above this, press:
- <kbd>F12</kbd> or <kbd>Ctrl Shift I</kbd> to open DevTools
- <kbd>Ctrl Shift R</kbd> to reload the client instantly

<br>

## JavaScript plugins

To add a plugin, just create a `.js` file in the plugins folder.

```js
// hello.js
console.log('Hello, League Client!')
```

All .js files (except filename starts with underscore or dot) in root of plugins folder and index.js in top-level subfolder will be executed after League ready.

```
plugins/
  |__ _util.js      
  |__ demo.js       ; will be executed
  |__ my-plugin/
    |__ index.js    ; will be executed
```

We recommend to use modern JavaScript editors like **Visual Studio Code** or **WebStorm** to develop your plugins, they support intellisense, linter and autocomplete. Remember that League Client is a web browser based, your should use front-end web technology only.

### CommonJS modules

We provided a simple implementation to support CommonJS modules. Each plugin file is a module, `require`, `global` and `module` are predefined.

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

Check out [API_DOCS](./API_DOCS.md) to get more APIs and [league-loader.js](/bin/plugins/league-loader.js) for example plugin.

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

window.addEventListener('load', () => {
  addCss('https://webdevtestbutch.000webhostapp.com/assets/Noxius.css')
})
```

To use local CSS file (see [Access local resources](#access-local-resources)):

```js
addCss('//assets/theme.css')
```

You can also use CSS code by requiring it from plugins folder:

```js
function insertCss(css) {
  const style = document.createElement('style')
  style.textContent = css
  document.body.append(style)
}

window.addEventListener('load', () => {
  insertCss(require('./theme.css'))
})
```

### LCU API requests

Just use `fetch` to make a LCU request:
```js
async function acceptMatchFound() {
  await fetch('/lol-matchmaking/v1/ready-check/accept', {
    method: 'POST'  
  })
}
```

### LCU WebSocket

When the websocket ready, this link tag will appear:
```html
<link rel="riot:plugins:websocket" href="wss://riot:hq5DDz5c8uLLc-dMRC1HGQ@127.0.0.1:50302/">
```

Call this function to subscribe API event:
```js
function subscribe() {
  const uri = document.querySelector('link[rel="riot:plugins:websocket"]').href
  const ws = new WebSocket(uri, 'wamp')
  
  ws.onopen = () => ws.send(JSON.stringify([5, 'OnJsonApiEvent']))
  ws.onmessage = async message => {
    const data = JSON.parse(message.data)
    console.log(data)
    // ...
  }
}
```

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
- https://www.jsdelivr.com/esm
- https://www.skypack.dev/
- https://esm.sh/
- https://unpkg.com/

### Faster UI development

We recommend to use tagged template literals to build large UI components. JSX flavor is the best choice with these libraries:
- [Nano JSX](https://nanojsx.io/)
- [Preact + htm](https://preactjs.com/guide/v10/getting-started/#alternatives-to-jsx)

Check out [league-loader.js](./bin/plugins/league-loader.js) to learn more.

<br>

## Access local resources

You can access the resources in **assets** folder by using this domain:

```
//assets/
```

It is also equivalent to `https://assets/` (HTTPS based scheme).

For example with custom theme.

```
league-loader/
  |__assets/
    |__background.png     ; custom background
    |__theme.css          ; overrides default background-url: url(//assets/background.png)
  |__plugins/
    |__theme.js           ; injects link tag with href="//assets/theme.css"
```

<br>

## Development

### Knowledges

- Project development requires high experience working with CEF.
- This project aims to League Client UI/UX. If you want to debug the Client, check out [Mecha](https://github.com/x00bence/Mecha) now.

### Build from source

This project requires Visual Studio 2017 with these components:
- Desktop development with C++
- .NET desktop development
- Windows 10 SDK, [version 1809](https://developer.microsoft.com/en-us/windows/downloads/sdk-archive/#windows-10)

You can also use VS2015+ and different SDK version.

Build steps:
  1. Open **league-loader.sln** in VS
  2. Right click on the solution -> **Restore Nuget Packages**
  3. Set arch to **Any CPU** or **x86**
  4. Right click on each project -> **Build**

### CEF notes
  
This project started development under CEF's low-level CAPI. You can convert to C++ OOP using `libcef_dll_wrapper`.
```cpp
#define WRAPPING_CEF_SHARED
#include "libcef_dll/ctocpp/browser_ctocpp.h"

void test_oop(cef_browser_t *cbrowser) {
  auto browser = CefBrowserCToCpp::Wrap(cbowser);
  auto host = browser->GetHost();
  host->GetMainFrame();
}
```

### How it works?

See [HOW_IT_WORKS](/HOW_IT_WORKS.md) for details.
