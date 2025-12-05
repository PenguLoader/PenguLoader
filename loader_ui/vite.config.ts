import { defineConfig } from 'vite';
import solidPlugin from 'vite-plugin-solid';
import devtools from 'solid-devtools/vite';
import UnocssPlugin from '@unocss/vite';
import mkcert from 'vite-plugin-mkcert'

export default defineConfig({
  plugins: [
    solidPlugin(),
    UnocssPlugin({
      // your config or in uno.config.ts
    }),
    // mkcert(),
  ],
  server: {
    port: 3000,
    // https: true,
  },
  build: {
    target: 'esnext',
  },
});
