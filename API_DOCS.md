# JavaScript API docs

These APIs are designed to use inside League Client with Pengu Loader plugin runtime.

<br>

## `openDevTools()` [function]

Call it to open the built-in DevTools window of the League Client.

Example:
```js
window.openDevTools();
```

## `openAssetsFolder()` [function]

Call it to open the assets folder in a new File Explorer window.

Example:
```js
window.openAssetsFolder();
```

## `openPluginsFolder()` [function]

Call it to open the plugins folder in a new File Explorer window.

Example:
```js
window.openPluginsFolder();
```

<br>

## `AuthCallback` [namespace]

This namespace helps you create a callback URL and read its response from an auth flow.

### `AuthCallback.createURL()` [function]

Create a unique URL that can be used for auth callback/redirect from external web browsers.

### `AuthCallback.readResponse(url, timeout?)` [function]

This function waits for a response from a given callback URL. It returns a Promise for an async context, containing a string if successful or null when timed out.

- `url`: Callback URL created by `createURL()`.
- `timeout`: Optional timeout in milliseconds, default is 180s.

Callback URL supports **GET** request only, the response of this function could be search params fulfilled by auth flow. 

Example:
```js
async function requestUserAuth() {
  const callbackURL = AuthCallback.createURL();
  const requestAuth = 'https://.../?redirect_uri=' + encodeURIComponent(callbackURL);
  
  // Open the authentication request URL in a new browser window.
  window.open(callbackURL);

  const response = await AuthCallback.readResponse(callbackURL);
  if (response === null) {
    console.log('timeout/fail');
  } else {
    console.log(response);
  }
  
  // Notify the Riot client to bring the focus back to the application window.
  fetch('/riotclient/ux-show', { method: 'POST' });
}
```

See the [spotify-gateway](https://github.com/LeagueLoader/spotify-gateway) example to learn more.

<br>

## `DataStore` [namespace]

League Client does not save user data to disk, as same as incognito mode in web browsers. This namespace helps you to store user data, as a persistent storage.

### `DataStore.set(key, value)` [function]

Call this function to store your data with a given key.
- `key` [required] Keys should be a string or number.
- `value` [require] Value may be a string, number, boolean, null, or a collection like array and object. It will be stored as JSON format, so any value like function and runtime object are ignored.

Example:
```js
let my_num = 10
let my_str = 'hello'
DataStore.set('my_num', my_num)
DataStore.set('my_str', my_str)
```

### `DataStore.get(key)` [function]

Retrieve your stored data with a given key. If the key does not exist, it will return `undefined`.

Example:
```js
console.log(DataStore.get('my_str'))
console.log(DataStore.get('key-does-not-exist'))
```

### `DataStore.has(key)` [function]

This function returns a boolean indicating whether data with the specified key exists or not.

```js
console.log(DataStore.has('my_num'))
console.log(DataStore.has('key-does-not-exist'))
```

### `DataStore.remove(key)` [function]

This function removes the specified data from storage by key, returns true if the existing key-value pair has been removed.

Example:
```js
DataStore.remove('some-key')
DataStore.has('some-key')     // -> false
```

> You should use unique names for keys, do not use common names, e.g `access_token`, `is_logged`, etc. Other plugins can override your data, you can add prefix to your keys. 

<br>

## `Effect` [namespace]

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

- `name` [required] These effect names above to be applied, in a string.

- `options` [optional] Additional options for the effect, `acrylic`, `unified` and `blurbehind` could have tint color, but `mica` will ignore this options.

This function returns `false` if the effect could not be applied, see the notes below.

Example:
```js
// Apply the acrylic effect to the current window.
Effect.apply('acrylic');

// Apply the unified effect with a white tinting color to the current window.
Effect.apply('unified', { color: '#FFFFFF' });

// Apply the mica effect to the current window.
Effect.apply('mica')
```

#### System compatibility
- These effects are currently supported on Windows only.
- On Windows 7, only `blurbehind` is supported.
- On Windows 10, it requires build 1703 or higher to use `acrylic`.
  - With any build after 2020, enabling it with transparency effects (on Personalize -> Color settings) will cause lagging.
- `mica` and `unified` are only supported on Windows 11, but `unified` can be enabled on Windows 10 without difference from `acrylic`. 

### `Effect.clear()` [function]

A function that clears any currently applied effect, then the Client background will be black.<br>
Use `Effect.current` after cleared will give you `undefined`.

Example:
```js
// just clear the applied effect, even if nothing is applied
Effect.clear();
```

### `Effect.on(event, callback)` [function]

Add a listener which will be triggered when the effect changes.

```js
Effect.on('apply', ({ old, name, options }) => {
  // do something
});

Effect.on('clear', () => {
  // do something
});
```

### `Effect.off(event, callback)` [function]

Remove your added listener.

<br>

## `__llver` (property)

This property contains the version of Pengu Loader in a string.

Example:
```js
console.log(window.__llver); // e.g 0.7.0
console.log('You are using Pengu Loader v' + window.__llver);
```

## TypeScript declaration

```ts
namespace globalThis {
  function openAssetsFolder(): void;
  function openPluginsFolder(): void;
  function openDevTools(remote?: boolean): void;
  
  namespace AuthCallback {
    function createURL(): string;
    function readResponse(url: string, timeout: number): Promise<string | null> 
  }

  namespace DataStore {
    function has(key: string): boolean;
    function get(key: string): any;
    function set(key: string, value: any): void;
    function remove(key: string): boolean;
  }

  namespace Effect {
    type EffectName = 'mica' | 'acrylic' | 'unified' | 'blurbehind';
    const current: EffectName | null;
    function apply(name: EffectName): boolean;
    function apply(name: Exclude<EffectName, 'mica'>, options: { color: string }): boolean;
    function clear(): void;
    function on(event: 'apply', listener: (name: string, options?: object) => any): void;
    function on(event: 'clear', listener: () => any): void;
    function off(event: 'apply', listener): void;
    function off(event: 'clear', listener): void;
  }
  
  var __llver: string;
}
```
