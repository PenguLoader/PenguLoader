<br>

<div align="center">
  <a href="https://leagueloader.app">
    <img src="https://user-images.githubusercontent.com/38210249/221445985-ef4591a8-c53e-4590-97bc-6bd4c50f3c1f.png" width="144"/>
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
<a href="https://app.fossa.com/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader?ref=badge_shield" alt="FOSSA Status"><img src="https://app.fossa.com/api/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader.svg?type=shield"/></a>
    <a href="./LICENSE">
      <img src ="https://img.shields.io/github/license/nomi-san/league-loader.svg?style=for-the-badge"/>
    </a>
  </p>
</div>


[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader?ref=badge_large)

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

1. Download the [latest release](https://github.com/nomi-san/league-loader/releases) and extract it
2. Run **League Loader**
4. Click **ACTIVATE**
5. Launch **League Client** and enjoy

<p align=center>
  <img src="https://user-images.githubusercontent.com/38210249/221457186-94c0fc0d-f062-42fc-bc78-fb1b5b43e9e2.png" />
</p>

> To try preview features, you have to [build this project](#build-from-source) or download the newest auto-build in [Actions](https://github.com/nomi-san/league-loader/actions).

## JavaScript plugins

To add your first plugin, just create a folder e.g `your-plugin` in `plugins` folder. Then create an `index.js` in your plugin folder.

```
plugins/
  |__your-plugin/
    |__index.js
```

This `index.js` is an entry point of your plugin and will be executed when League Client ready. Put this line to your index, you will see the log in console.

```js
console.log('Hello, League Client!')
```

#### Please check out these [basic plugin templates](./plugins) to start.

> We recommend you to use modern JavaScript editors like **Visual Studio Code** or **WebStorm** to develop your plugins, it supports intellisense, linter and autocomplete.

### ES Modules

To load other scripts, just use `import`:

```
plugins/
  |__your-plugin/
    |__index.js
    |__utils.js
```

```js
// utils.js
export default {
  greet: () => console.log('Hello!')
}

// index.js
import utils from './utils'
utils.greet();
```

> To use `import` and `export` properly, please refer this [MDN docs](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import).

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

### NodeJS & npm compatibility

We highly recommend you to use npm project for building plugins. With TypeScript or other languages require transpilation, you need a build tool to build them,
Webpack, Rollup or Vite is the best choice.

You can also use any font-end library to build custom UI, e.g React, Preact, Vue, Svelte, SolidJS, etc. With front-end tooling, its hot-reload/HMR will help you to do faster.

Example plugins:
- [./plugins/@default](./plugins/@default): Vite with SolidJS + SASS + TypeScript
- [douugdev/league-a-better-client](https://github.com/douugdev/league-a-better-client): Webpack with Preact + SASS + TypeScript

> Please note that all packages that are designed to run in NodeJS only could not be used for League Loader plugin.

> With build tool, your bundled assets output may have incorrect path. Please refer to next section to make them right.

### Accessing local resources

You can access local resources in **assets** and **plugins** folder by using these domain:

```
//assets/
//plugins/
```

They are also equivalent to `https://assets/` and `https://plugins` (HTTPS based scheme).

```
root/
  |__assets/
    |__background.png      ->  //assets/background.png
  |__plugins/
    |__your-plugin/
      |__assets/
        |__some-image.jpg  ->  //plugins/your-plugin/assets/some-image.jpg
```

- **assets** folder contains common resources.
- While assets in **plugins** folder are used for plugin itself.

## Documentation

- [API docs](./API_DOCS.md)
- [Migration to v1](./MIGRATION_TO_V1.md)
- [Insecure options](./INSECURE_OPTIONS.md)

## Contributing

Follow these steps to contribute to the project:
1. Fork it (https://github.com/nomi-san/league-loader/fork)
2. Create your feature branch `feat/<branch-name>`
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request

### Ways you can contribute

- **documentation and website** - the documentation always needs some work, if you discover that something is not documented or can be improved you can create a PR for it, check out [LeagueLoader org](https://github.com/LeagueLoader)
- **more base/starter plugins** - push your plugin with detailed guide to help beginner with ease
- **core features** - make sure you have a much experience with CEF and low-level programming skill
- **javascript features** - need too much webdev knowledge

### Project structure

- **LeagueLoader** - main loader menu UI, written in C# and WPF XAML
- **d3d9** - core module (DLL), it hooks libCEF to make everything magical
- **plugins** - templates for plugin dev beginner

### Build from source

This project requires Visual Studio 2017 with these components:
- Desktop development with C++
- .NET desktop development
- Windows 8.1 SDK

You can also use VS 2019+ and different SDK version.

Build steps:
  1. Open **league-loader.sln**
  2. Right click on the solution -> **Restore Nuget Packages**
  3. Set arch to **x86**
  4. Right click on each project -> **Build**

The @default plugin requires:
- NodeJS 16+
- pnpm

Then build it:

```
pnpm install
pnpm build
```