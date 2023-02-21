import { resolve, join } from 'node:path';
import { existsSync } from 'node:fs';
import { unlink, readFile, writeFile, cp, mkdir, rm } from 'node:fs/promises';
import { defineConfig } from 'vite';
import preact from '@preact/preset-vite';
import mkcert from 'vite-plugin-mkcert';
import cssInjectedByJsPlugin from 'vite-plugin-css-injected-by-js';

// @ts-ignore
import pkg from './package.json';
const PLUGIN_NAME = pkg.config.pluginName;

const getIndexCode = (port) => (
  `window.addEventListener('load', async () => {
  await import('https://localhost:${port}/@vite/client');
  await import('https://localhost:${port}/src/main.tsx');
});`
);

let port: number;
const outDir = resolve(__dirname, 'dist');
const pluginsDir = resolve(__dirname, pkg.config.leagueLoaderPath, 'plugins', PLUGIN_NAME);

async function emptyDir(path) {
  if (existsSync(path)) {
    await rm(path, { recursive: true });
  }
  await mkdir(path, { recursive: true });
}

export default defineConfig({
  build: {
    rollupOptions: {
      output: {
        manualChunks: undefined,
        entryFileNames: 'index.js'
      }
    }
  },
  server: { https: true },
  plugins: [
    preact(),
    mkcert(),
    cssInjectedByJsPlugin(),
    {
      name: 'll-plugin-1',
      configureServer(server) {
        server.httpServer.once('listening', async () => {
          port = server.httpServer.address()['port'];
          await emptyDir(pluginsDir);
          await writeFile(join(pluginsDir, 'index.js'), getIndexCode(port));
        });
      },
      async closeBundle() {
        const index = join(outDir, 'index.js');
        await unlink(join(outDir, 'index.html'));
        const code = (await readFile(index, 'utf-8'))
          .replace(/url\((\/[^\n"?:*<>|]+\.[A-z0-9]+)\)/g, `url(//plugins/${PLUGIN_NAME}$1)`)
          .replace(/^/, 'window.addEventListener("load", function () {\n')
          .replace(/$/, '});');
        await writeFile(index, code);
        await emptyDir(pluginsDir);
        await cp(outDir, pluginsDir, {
          recursive: true,
        });
      }
    },
    {
      name: 'll-plugin-2',
      enforce: 'post',
      apply: 'serve',
      transform: (code, id) => {
        if (/\.(ts|tsx)$/i.test(id)) return;
        return code.replace(/\/src\//g, `https://localhost:${port}/src/`)
      },
    }
  ]
});