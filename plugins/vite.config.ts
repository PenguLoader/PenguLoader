import path from 'node:path';
import fs from 'node:fs/promises';
import { defineConfig } from 'vite';
import autoprefixer from 'autoprefixer';
import tailwindcss from 'tailwindcss';
import solidPlugin from 'vite-plugin-solid';
import bundleCssInJs from 'vite-plugin-css-injected-by-js';

const root = (...args: string[]) => path.join(__dirname, ...args);

export default defineConfig(({ command, mode }) => {

  const dev = command === 'serve'
    || mode === 'development';

  return {
    publicDir: false,
    css: {
      postcss: {
        plugins: [
          autoprefixer,
          tailwindcss,
        ]
      }
    },
    esbuild: {
      legalComments: 'none',
    },
    build: {
      assetsInlineLimit: 1024 * 64,
      minify: !dev,
      modulePreload: false,
      sourcemap: dev ? 'inline' : false,
      lib: {
        name: 'preload',
        entry: 'src/index.ts',
        formats: ['iife']
      },
      rollupOptions: {
        output: {
          format: 'iife',
          // sourcemap: dev ? 'inline' : false,
          entryFileNames: 'preload.js'
        }
      }
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
      {
        name: 'pengu-build',
        apply: 'build',
        enforce: 'post',
        async closeBundle() {
          const code = await fs.readFile(root('dist/preload.js'), 'utf-8');
          const header = generateHeader(code, 'preload_script');
          await fs.writeFile(root('dist/preload.g.h'), header, 'utf-8');
        }
      }
    ]
  }
});

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