import { resolve, join } from 'node:path';
import { existsSync } from 'node:fs';
import { unlink, readFile, writeFile, cp, mkdir, rm } from 'node:fs/promises';
import { defineConfig } from 'vite';
import mkcert from 'vite-plugin-mkcert';

import pkg from './package.json';
const PLUGIN_NAME = pkg.config.pluginName;

const getIndexCode = (port: number) => (
  `window.addEventListener('load', async () => {
  await import('https://localhost:${port}/@vite/client');
  await import('https://localhost:${port}/src/index.ts');
});`
);

let port: number;
const outDir = resolve(__dirname, 'dist');
const pluginsDir = resolve(__dirname, pkg.config.loaderPath, 'plugins', PLUGIN_NAME);

async function emptyDir(path: string) {
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
        entryFileNames: 'index.js',
        assetFileNames(name) {
          if (name.name === 'index.css')
            return name.name;
          return 'assets/[name]-[hash][extname]';
        }
      }
    }
  },
  server: {
    https: true,
    // port: 3000
  },
  publicDir: false,
  plugins: [
    mkcert(),
    {
      name: 'll-serve',
      apply: 'serve',
      enforce: 'post',
      configureServer(server) {
        server.httpServer!.once('listening', async () => {
          // @ts-ignore
          port = server.httpServer.address()['port'];
          await emptyDir(pluginsDir);
          await writeFile(join(pluginsDir, 'index.js'), getIndexCode(port));
        });
      },
      transform: (code, id) => {
        if (/\.(ts|tsx|js|jsx)$/i.test(id)) return;
        return code.replace(/\/src\//g, `https://localhost:${port}/src/`)
      },
    },
    {
      name: 'll-build',
      apply: 'build',
      enforce: 'post',
      async closeBundle() {
        const indexJs = join(outDir, 'index.js');
        const indexCss = join(outDir, 'index.css');

        const jsCode = (await readFile(indexJs, 'utf-8'))
          // Patch asset URLs
          .replace(/\"\/assets\//g, `"//plugins/${PLUGIN_NAME}/assets/`)
          // Wrap code inside window load
          .replace(/^/, 'window.addEventListener("load", function () {\n')
          .replace(/$/, '});')
          // Import CSS module
          .replace(/^/, 'import "./index.css";\n');
        await writeFile(indexJs, jsCode);

        const cssCode = (await readFile(indexCss, 'utf-8'))
          // Patch asset URLs
          .replace(/url\(\/assets\//g, `url(./assets/`);
        await writeFile(indexCss, cssCode);

        // Remove index.html
        await unlink(join(outDir, 'index.html'));

        // Copy output
        await emptyDir(pluginsDir);
        await cp(outDir, pluginsDir, {
          recursive: true,
        });
      }
    }
  ]
});