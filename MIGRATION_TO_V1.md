## Migration from CommonJS

In v1.0, we have been removed CommonJS support and switched to ESM. That means all scripts those are using `require` and `module.exports` will not run anymore when switching to new version. 

## Migration guide

### Update your `require()`:

```js
// before
const utils = require('./utils')

// after
import utils from './utils'
```

```js
// before
const { hello } = require('./greeting')

// after
import { hello } from './greeting'
```

> Read more at [MDN import statement](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/import). Please note that you no need to add the `.js` extension to import path.

### Also update your `module.exports`:

```js
// before
module.exports = {
  greet: () => 'Hello'
};

// after
export default {
  greet: () => 'Hello'
};
```

```js
// before
module.exports = to_export;

// after
export default to_export;
```

Just `exports.<id>`:

```js
// before
exports.data = some_value

// after
export let data = some_value
// or with const if data is immutable
export const data = some_value
```

This also works with named functions:

```js
// before
exports.todo = function () { ... }
exports.todoAsync = async function () { ... }

// after
export function todo() { ... }
export async function todoAsync() { ... }
```

 >Read more at [MDN export statement](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/export).

### Dynamic import

`import` statement does not allow you to use it in non-top-level, in functions or some scopes, likes require() does.

```js
// before
function load() {
  const utils = require('./utils')
}
```

Do it with async context:

```js
// after
async function load() {
  const utils = await import('./utils')
}
```

Or with Promise's `.then` callback:

```js
// after
function load() {
  import('./utils').then(module => {
    const utils = module.default;
    // ...
  })
}
```

In this case above, `import` becomes an async function like. You can also add a `.catch` chain to catch module errors.

> Read more at [MDN import() operator](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/import). Please note that you no need to add the `.js` extension to import path.

### New plugin project structure

Since v0.5, we introduced `//assets/` domain which supported accessing resources from **assets** folder. And now, this folder is used for large-common-unique resources only. Now each plugin will have its assets and you can access them via `//plugins/`.

Stop putting your plugin JS files directly into **plugins** folder. You have to create new folder for your plugin.

```
plugins/
  |__your-plugin
    |__assets/
      |...         <- resources
    |__index.js    <- plugin entry
    |...           <- other utils
```

For example with plugin entry:

```js
// my-plugin/index.js
import { hello } from './greeting'
console.log(hello)
```

```js
// my-plugin/gretting.js
export const hello = 'WORLD'
```

When your plugin loads, the entry will acquires the `greeting.js` first and prints the `hello`.

## Magic imports

### Assets import

For updating HTML elements from JS, you can use `import` to get the assets path without knowing its full path, e.g `//plugins/your-plugin/assets/name`.

```js
import background from './assets/my-background.jpg'

function changeBackground() {
  const div = document.querySelector('some-div')
  div.style.backgroundImage = `url(${background})`

  // or with <img>'s src attribute
  myImg.src = background
}
```

You can log the background to see its value:

```js
console.log(background)
```

```
plugins/
  |__your-plugin/
    |__assets/
      |__my-background.jpg
    |__index.js
```

With the structure above, you got `//plugins/your-plugin/assets/my-background.jpg`.

### CSS import

```js
import './theme.css'
```

This line will inject a `link` tag into `body` to load your `theme.css`. It's equivalent to the legacy way:

```js
const link = document.createElement('link')
link.rel = 'stylesheet'
link.href = '//plugins/your-plugin/theme.css'
document.body.appendChild(link)
```

In your CSS module, you can import plugin assets using relative path:
```css
.some-div {
  background-image: url(./assets/image.png);
  /* resolve to //plugins/your-plugin/assets/image.png */ 
}
```

### JSON import

Assume your `config.json` like this:

```json
{
  "hideMyName": true
}
```

Let's import it:

```js
import config from './config.json'
console.log(config.hideMyName)  // true
```

You can also change its value, but no change to the JSON file.

```js
config.hideMyName = false

// in other modules that require it
console.log(config.hideMyName)  // false
```

### Explicit import

You can specific a suffix value as query param to the `import` URL.

```js
import content from './my-data.txt?raw'
// read a text file

import path from './some-resource?url'
// get path to resource
```