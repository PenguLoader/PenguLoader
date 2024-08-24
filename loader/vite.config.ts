import { defineConfig } from 'vite'
import { resolve } from 'node:path'
import solid from 'vite-plugin-solid'
import autoprefixer from 'autoprefixer'
import tailwindcss from 'tailwindcss'

import pkg from './package.json'

// https://vitejs.dev/config/
export default defineConfig({
  css: {
    postcss: {
      plugins: [
        autoprefixer,
        tailwindcss
      ]
    }
  },
  define: {
    '__VERSION__': JSON.stringify(pkg.version),
    '__PLATFORM__':  JSON.stringify(process.platform),
  },
  publicDir: false,
  plugins: [
    solid(),
  ],
  resolve: {
    alias: {
      '~': resolve(__dirname, './src')
    }
  },

  // Vite options tailored for Tauri development and only applied in `tauri dev` or `tauri build`
  //
  // 1. prevent vite from obscuring rust errors
  clearScreen: false,
  // 2. tauri expects a fixed port, fail if that port is not available
  server: {
    port: 1420,
    strictPort: true,
    watch: {
      // 3. tell vite to ignore watching `src-tauri`
      ignored: ['**/src-tauri/**'],
    },
  },
})