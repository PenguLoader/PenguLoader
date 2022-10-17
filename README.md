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

## Getting started

1. Download the [latest release](https://github.com/nomi-san/league-loader/releases) and extract it (or [build from source](#build-from-source))).
2. Run **League Loader.exe**
3. Select League Client path
4. Click **Install**
5. Launch League Client

<p align="center">
  <img src="https://i.imgur.com/mDihNl7.png">
</p>

After League is ready, just click "**Show DevTools**" in settings panel to open **League Client DevTools**.

<p align="center">
  <img src="https://user-images.githubusercontent.com/38210249/196092793-54e6e16b-7f02-41b3-84fe-42181acc1c96.png">
</p>

We also support **insecure options** for plugins, do not use it if you do not want to be banned.

## Plugins

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

### CommonJS

You can use ES6 and CommonJS modules in your plugins.

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

We have an example which modifies the settings UI to preppend a DevTools opener, see [dev-tools.js](/bin/plugins/dev-tools.js).

### Development notes

You should use Visual Studio Code to develop your plugins,
it supports intellisense, linter and autocomplete for modules.

League Client uses CEF 91/Chromium 91.
The JS runtime is V8 engine in browser, that means you are writing JS for web browser.

When interacting with the DOM, you should add your code to `onload` or `DOMContentLoaded` event of `window`.

## Build from source

This project requires Visual Studio 2017 with these components:
- Desktop development with C++
- .NET desktop development
- Windows 8.1 SDK & .NET Framework 4.5

You can also use VS2015+ and different SDK version.

Build steps
  1. Open **league-loader.sln** in VS
  2. Right click on the solution -> **Restore Nuget Packages**
  3. Set arch to **Any CPU** or **x86**
  4. Right click on each project -> **Build**

## How it works?

See [HOW_IT_WORKS](/HOW_IT_WORKS.md) for details.

### CEF notes

Current CEF version: **v91.1.22**

This project use CEF CAPI in C++. If you need sweet C++ OOP, just use libcef wrapper.

Let's download our [pre-built here](https://github.com/nomi-san/league-loader/releases/tag/0.1a) then add the following code.

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
