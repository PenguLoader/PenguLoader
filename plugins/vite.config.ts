import path from 'node:path';
import fs from 'node:fs/promises';
import { defineConfig } from 'vite';
import { build } from 'esbuild';

// Vite plugins
import mkcert from 'vite-plugin-mkcert';
import solidPlugin from 'vite-plugin-solid';
import viteRestart from 'vite-plugin-restart';

const port = 3001;
const root = (...args: string[]) => path.join(__dirname, ...args);

export default defineConfig(({ command, mode }) => {

  const dev = command === 'serve'
    || mode === 'development';

  return {
    publicDir: false,
    server: {
      https: true,
      port: port
    },
    esbuild: {
      legalComments: 'none',
    },
    build: {
      minify: !dev,
      modulePreload: false,
      lib: {
        entry: {
          npm: 'src/npm/index.ts',
          preload: 'src/preload/index.ts',
          views: 'src/views/index.tsx',
        },
        formats: ['es'],
      },
      rollupOptions: {
        output: {
          format: 'es',
          sourcemap: dev ? 'inline' : false,
        }
      }
    },
    plugins: [
      mkcert(),
      solidPlugin(),
      viteRestart({
        restart: 'src/preload/**/*.ts'
      }),
      {
        name: 'pengu-serve',
        apply: 'serve',
        enforce: 'post',
        transform(code, id) {
          if (/\.(ts|tsx)$/i.test(id)) return;
          return code.replace(/\/src\//g, `https://localhost:${port}/src/`)
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
          const header = await generateHeader(root('dist'), 'pengu_assets');
          await fs.writeFile(root('dist/pengu.g.h'), header, 'utf-8');
        }
      }
    ]
  }
});

function generateDevLoader(port: number) {
  const template = function (port) {
    document.addEventListener('DOMContentLoaded', async () => {
      // @ts-ignore
      await import(`https://localhost:${port}/@vite/client`);
      // @ts-ignore
      await import(`https://localhost:${port}/src/views/index.tsx`);
    });
  }
  return `!(${template.toString()})(${port});`;
}

async function generateBytes(file: string) {
  const bytes = [...await fs.readFile(file)]
    .map(c => '0x' + c.toString(16).padStart(2, '0'));

  const formatted = Array<string>();
  for (let i = 0; i < bytes.length; i += 12) {
    const line = bytes.slice(i, i + 12).join(', ');
    formatted.push(line);
  }

  return formatted.join(',\n  ');
}

async function generateHeader(dir: string, name: string) {
  const files = await readDir(dir);
  const hashes = files.map(p => fnv1a32('/@pengu/' + p));
  const chunks = await Promise.all(files.map(p => generateBytes(path.join(dir, p))));

  return `#ifndef _${name.toUpperCase()}_H_
#define _${name.toUpperCase()}_H_

#include <utility>
#include <unordered_map>

${hashes.map((h, i) => `static const unsigned char __h${h}_[] = {
  ${chunks[i]}
};
`).join('\n')
}

static const std::unordered_map<unsigned int, std::pair<const void *, size_t>> __${name} {
${hashes.map((h, i) => `  { 0x${h}, std::make_pair<const void *, size_t>(__h${h}_, sizeof(__h${h}_)) },`).join('\n')}
};

#endif`
}

async function readDir(dir: string) {
  const files = Array<string>();

  async function readDirectory(dir: string) {
    const entries = await fs.readdir(dir, { withFileTypes: true });

    for (const entry of entries) {
      const fullPath = path.join(dir, entry.name);
      if (entry.isDirectory()) {
        await readDirectory(fullPath);
      } else {
        files.push(fullPath);
      }
    }
  }

  await readDirectory(dir);
  return files.map(p => p.replace(dir, '').replace(/\\/g, '/').substring(1));
}

function fnv1a32(str: string) {
  let hash = 0x811c9dc5n;
  for (let i = 0; i < str.length; i++) {
    hash ^= BigInt(str.charCodeAt(i));
    hash = BigInt.asUintN(32, hash * 0x01000193n);
  }
  return hash.toString(16);
}