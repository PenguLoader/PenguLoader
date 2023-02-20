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
      <img src ="https://img.shields.io/badge/-Join%20Discord-5c5fff.svg?&style=for-the-badge&logo=Discord&logoColor=white"/>
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
  |__demo.js       ; will be executed
  |__my-plugin/
    |__index.js    ; will be executed
```

We recommend to use modern JavaScript editors like **Visual Studio Code** or **WebStorm** to develop your plugins, they support intellisense, linter and autocomplete. Remember that League Client is a web browser based, your should use front-end web technology only.

For plugin which contains resources e.g images, video, fonts, etc, we recommend to put these into subfolder.

```
plugins/
  |__awesome-plugin/
    |__assets/
      |__background.png
      |__avatar.gif
      ..
    |__index.js   <-- entry
    ..
````

### ES Modules

Top-level await:
```js
let data = await fetch('https://...').then(res => res.text()); 
```

Import a ESM library from CDN:
```js
import axios from 'https://esm.run/axios';
axios.get('https://...');
```

Assets import:

```js
import './theme.css';
// auto-inject CSS

import data from './data.json';
// parsed JSON data

import bgImage from './assets/background.png';
// path to asset
```

Explicit import:
```js
import QuantifyFontUrl from './assets/Quantify.ttf?url';
// https://plugins/your-plugin/assets/Quantify.ttf

import rawData from './my-data.txt?raw';
// content of my-data.txt in string
```

> With `?raw`, your text files must be saved with UTF8 encoding.

### Theme the League Client

Inject custom CSS to override default League's style.

From your plugin entry, use import to add:

```js
import './theme.css';
```

This line will inject CSS code from `theme.css` next to your `index.js`.

To use remote theme, e.g from `https://example.com/theme.css`:

```js
function addCssLink(url) {
  const link = document.createElement('link');
  link.href = url;
  link.type = 'text/css';
  link.rel = 'stylesheet';
  document.head.appendChild(link);
}

window.addEventListener('load', () => {
  addCssLink('https://example.com/theme.css');
});
```

### LCU API requests

Just use `fetch` to make LCU requests:
```js
async function acceptMatchFound() {
  await fetch('/lol-matchmaking/v1/ready-check/accept', {
    method: 'POST'  
  });
}
```

### LCU WebSocket

When the websocket ready, this link tag will appear:
```html
<link rel="riot:plugins:websocket" href="wss://riot:abcDEF0123XYZ@127.0.0.1:12345/">
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
