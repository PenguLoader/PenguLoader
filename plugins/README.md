## What's it?

This folder is the source code of the preload scripts and built-in plugins.

### Tech stacks

![](https://skillicons.dev/icons?i=vite,ts,solidjs,tailwind)

- **Vite** - dev server, build tool and bundler.
- **TypeScript** - typesafe!
- **SolidJS** - high performance JSX embedded views, no vDOM!
- **Tailwind** - no more CSS!

## Developing

We have two folders in `src`

- preload: pofyfill, loader and hook scripts
- views: SolidJS app - a successor of the previous @default plugin

First, you need to build the core module in `Debug` mode.

Second, install dependencies:

```
pnpm install
```

Then run the dev server:

```
pnpm dev
```

Now start your **League Client** to get the result. Additionally, you can open
https://localhost:3001 in a web browser to test the views.

Any changes of views will perform hot-replacement in DOM. With preload script
changes, it will reload entire client.

If you need to modify the `preload` only, run dev build and reload the client
manually.

```
pnpm build-dev
```

> Don't touch vite.config.ts without knowing it.

## Building production

Run build

```
pnpm build
```

To embed the scripts into the core module, you should build it in `Release`
mode.
