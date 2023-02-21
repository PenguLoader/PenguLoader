## What's this?

This is a plugin built for League Loader itself with these goals:
- Show welcome page and update changelog
- Support i18n
- Using Preact + SASS + TypeScript
- Vite boilerplate template
- Fully HMR supports, it's just better than legacy hot-reload
- Auto-patching assets path, just put assets in dev folder only

This plugin replaced **league-loader.js** plugin as @default, you can find the legacy one in tag v0.6.0 and before.

## Getting started

### Setup

You need  **Node 16+** and **Yarn** installed to run and build this plugin. For developing new advanced plugin from this one, you should learn Vite to configure the project. Next, we will guide you basic editting.

Before you start, let's edit the `package.json`, there are two fields in `config`:
- `pluginName`: you should change it to avoid overriding the @default plugin.
- `leagueLoaderPath`: path to your League Loader folder, the default value `../bin/` points to the directory which is used for binary built output.

```json
  "config": {
    "pluginName": "@default",
    "leagueLoaderPath": "../bin/"
  },
```

### Development

Let's install Node modules and start the dev server:
```
yarn dev
```

After the dev server stared, you can see a new folder in League Loader's plugins. It's your plugin entry.

```
plugins/
  |__your-plugin/
    |__index.js
```

Now you can open League Client and enjoy coding.

<p align="center">
  <img src="https://i.imgur.com/RA8djUO.png" />
</p>

### Build

To build the plugin for production, please run:
```
yarn build
```

The bundled output will replaced the previous `index.js` file in Development section.