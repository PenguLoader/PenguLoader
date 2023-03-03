## What's this?

This is a simple theme with Vite + TypeScript + SASS, full HMR support.

## Getting started

### Setup

You need **Node 16+** and **pnpm** installed to run and build it.

Before you start, let's edit the `package.json`, there are two fields in `config`:
- `pluginName`: your theme name, default is `vite-theme`.
- `loaderPath`: path to your League Loader folder, the default value `../../bin/` points to the directory which is used for binary built output.

```json
  "config": {
    "pluginName": "vite-theme",
    "loaderPath": "../../bin/"
  },
```

### Development

Project structure:

```
src/
  |__assets/
    |...            <- assets
  |__index.ts       <- entry
  |__theme.scss     <- theme
```

Let's install Node modules and start the dev server:
```
pnpm i
pnpm dev
```

After starting the dev server, you will see a new folder in the League Loader plugins folder. The `index.js` inside is your theme entry.

```
plugins/
  |__your-theme/
    |__index.js
```

Now you can open League Client and enjoy coding.

#### Local assets

You should put all your assets in the `src/assets/` folder. Then use relative `url()` in `theme.scss`.

```css
/* theme.scss  */

.some-div {
  background-image: url(./assets/my-image.png);
}
```

### Build

To build the theme for production, please run:
```
pnpm build
```

You can get its output from your specified League Loader folder.