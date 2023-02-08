# API docs

These APIs are designed to use inside League Client only.

<br>

## `require(name)` [function]

You must have knowledge about CommonJS module to use this function.<br>
If you've ever developed software with NodeJS, it shouldn't be hard.

Please refer to [README](./README.md) for examples and [Node docs](https://nodejs.org/api/modules.html) to learn more.

<br>

## `openDevTools()` [function]

Call it to open built-in DevTools window of League Client.

Example:
```js
window.openDevTools();
```

<br>

## `openPluginsFolder()` [function]

Call it to open the plugins folder in new File Explorer window.

Example:
```js
window.openPluginsFolder();
```

<br>

## `Effect` (object)

This namespace supports changing window effects.<br>

There are available effects: `mica`, `acrylic`, `unified` and `blurbehind`.

### `Effect.current` [property]

A read-only property that returns the currently applied effect or `undefined` if no effect has been applied.

Example:
```js
// prints applied effect
console.log(Effect.current)
```

### `Effect.apply(name, options?)` [function]

A function that takes the name of the desired effect name and an optional object.<br>
It returns a boolean indicating whether the effect was successfully applied or not.

- `name` [required] These effect names above to be applied, in string.

- `options` [optional] Additional options for the effect, `acrylic`, `unified` and `blurbehind` could have tint color, but `mica` will ignore this options.

This function returns `false` if the effect could not be applied, see the notes below.

Example:
```js
// enable acrylic on Windows 10
Effect.apply('acrylic')

// with tinting color
Effect.apply('unified', { color: '#4446' })

// mica on windows 11, no options needed
Effect.apply('mica')
```

### `Effect.clear()` [function]

A function that clears any currently applied effect, then the Client background will be black.<br>
Use `Effect.current` after cleared will give you `undefined`.

Example:
```js
// just clear applied effect, even if nothing applied
Effect.clear();
```

### Notes
- These effects are currently supported on Windows only.
- On Windows 7, only the `blurbehind` is supported.
- On Windows 10, requires build 1703 or higher to use `acrylic`.
  - With any build after 2020, enabling it with transparency effects (on Personalize -> Color settings) will cause lagging.
- `mica` and `unified` are only supported on Windows 11, but `unified` can be enabled on Windows 10 without different from `acrylic`. 

<br>

## `__llver` (property)

This property contains version of League Loader in string.

Example:
```js
console.log(window.__llver); // e.g 0.7.0
console.log('You are using League Loader v' + window.__llver);
```
