import { defineConfig, Plugin } from 'vite'
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

  },
  publicDir: false,
  plugins: [
    solid(),
    penguEnv(),
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

function penguEnv(): Plugin {
  const pluginName = 'pengu-env'
  const virtualModuleId = 'virtual:' + pluginName
  const resolvedVirtualModuleId = '\0' + virtualModuleId

  return {
    name: pluginName,
    resolveId(id) {
      if (id === virtualModuleId) {
        return resolvedVirtualModuleId
      }
    },
    async load(id) {
      if (id === resolvedVirtualModuleId) {
        const env = {
          isMac: process.platform === 'darwin',
          version: pkg.version
        }
        return `export default ${JSON.stringify(env)};`
      }
    },
  }
}