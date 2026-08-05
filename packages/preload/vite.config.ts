import path from 'node:path';
import fs from 'node:fs/promises';
import { defineConfig } from 'vite';
import { build } from 'esbuild';
import rootPkg from '../../package.json' with { type: 'json' }

// Vite plugins
import solidPlugin from 'vite-plugin-solid';
import bundleCssInJs from 'vite-plugin-css-injected-by-js';
import viteRestart from 'vite-plugin-restart';

const port = 3001;
const root = (...args: string[]) => path.join(__dirname, ...args);

export default defineConfig(({ command, mode }) => {

  const dev = command === 'serve'
    || mode === 'development';

  return {
    publicDir: false,
    server: {
      port: port
    },
    esbuild: {
      legalComments: 'none',
    },
    // Two artifacts, deliberately built by two tools:
    //
    //   preload.js  core, self-contained IIFE, ~10 kB. Embedded and eval'd
    //               synchronously in OnContextCreated, so it must have no
    //               `import` statements at all — a chunk fetch would make it
    //               async and lose the ordering guarantee the RCP wrap needs.
    //               Built by esbuild in closeBundle below; pure .ts, no JSX.
    //
    //   views.js    Pengu's own UI, ~100 kB of SolidJS. An ES module, pulled
    //               in by loader.ts via dynamic import before plugins load.
    //               Built by vite here because it needs the Solid transform.
    //
    // Rollup can't emit both formats from one lib build, and a shared chunk
    // between them would reintroduce the import into the core — hence the
    // split toolchain and the `__pshared` handoff for the stateful rcp module.
    build: {
      assetsInlineLimit: 1024 * 64,
      minify: !dev,
      modulePreload: false,
      lib: {
        entry: 'src/views/index.tsx',
        formats: ['es']
      },
      rollupOptions: {
        output: {
          format: 'es',
          sourcemap: dev ? 'inline' : false,
          entryFileNames: 'views.js',
          // One file, no code-splitting. The built-in asset map serves whole
          // files by name, so a hashed chunk would have nothing to resolve it.
          // views has no dynamic imports today; this keeps a future one from
          // silently emitting a second file that never loads.
          codeSplitting: false,
        }
      }
    },
    define: {
      '__VERSION__': JSON.stringify(rootPkg.version),
      '__PLATFORM__': JSON.stringify(process.platform),
    },
    plugins: [
      solidPlugin(),
      bundleCssInJs({
        topExecutionPriority: false,
        injectCodeFunction: function (css) {
          document.addEventListener('DOMContentLoaded', function () {
            const style = document.createElement('style');
            style.appendChild(document.createTextNode(css));
            document.head.appendChild(style);
          });
        }
      }),
      viteRestart({
        restart: 'src/preload/**/*.ts'
      }),
      {
        name: 'pengu-serve',
        apply: 'serve',
        enforce: 'post',
        transform(code, id) {
          if (/\.(ts|tsx)$/i.test(id)) return;
          return code.replace(/\/src\//g, `http://localhost:${port}/src/`)
        },
        async configResolved() {
          await build({
            entryPoints: [root('src/preload/index.ts')],
            outfile: root('dist/preload.js'),
            bundle: true,
            format: 'iife',
            sourcemap: 'inline',
            footer: {
              'js': generateDevLoader(port)
            }
          });
        },
      },
      {
        name: 'pengu-build',
        apply: 'build',
        enforce: 'post',
        async closeBundle() {
          // Vite has just written dist/views.js. Now build the core, which
          // vite can't produce in the same pass (different format, and it must
          // stay import-free).
          await build({
            entryPoints: [root('src/preload/index.ts')],
            outfile: root('dist/preload.js'),
            bundle: true,
            format: 'iife',
            minify: !dev,
            legalComments: 'none',
            define: {
              '__VERSION__': JSON.stringify(rootPkg.version),
              '__PLATFORM__': JSON.stringify(process.platform),
            },
          });

          const preload = await fs.readFile(root('dist/preload.js'), 'utf-8');
          const views = await fs.readFile(root('dist/views.js'), 'utf-8');

          await fs.writeFile(root('dist/preload.g.h'),
            generateHeader(preload, 'preload_script'), 'utf-8');
          await fs.writeFile(root('dist/views.g.h'),
            generateHeader(views, 'views_script'), 'utf-8');
        }
      }
    ]
  }
});

function generateDevLoader(port: number) {
  const template = function (port: number) {
    document.addEventListener('DOMContentLoaded', async () => {
      // @ts-ignore
      await import(`http://localhost:${port}/@vite/client`);
      // @ts-ignore
      await import(`http://localhost:${port}/src/views/index.tsx`);
    });
  }
  return `!(${template.toString()})(${port});`;
}

function generateHeader(code: string, name: string, lineLength = 12) {
  const bytes = [...Buffer.from(code, 'utf-8')]
    .map(c => '0x' + c.toString(16).padStart(2, '0'));

  const formatted = Array<string>();
  for (let i = 0; i < bytes.length; i += lineLength) {
    const line = bytes.slice(i, i + lineLength).join(', ');
    formatted.push(line);
  }

  return `#ifndef _${name.toUpperCase()}_H_
#define _${name.toUpperCase()}_H_

static const unsigned int _${name}_size = ${bytes.length};

static const unsigned char _${name}[${bytes.length + 1}] = {
  ${formatted.join(',\n  ')}
};

#endif`
}