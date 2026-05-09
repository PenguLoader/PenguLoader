import { defineConfig } from 'vite'
import { resolve } from 'node:path'
import solid from 'vite-plugin-solid'
import autoprefixer from 'autoprefixer'
import tailwindcss from 'tailwindcss'
import rootPkg from '../../package.json' with { type: 'json' }

// https://vitejs.dev/config/
export default defineConfig({
  css: {
    postcss: {
      plugins: [
        autoprefixer as any,
        tailwindcss as any,
      ]
    }
  },
  define: {
    '__VERSION__': JSON.stringify(rootPkg.version),
    '__PLATFORM__': JSON.stringify(process.platform),
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

  // Dev server: the .NET host (Pengu.Windows) launches with
  // `--dev=http://localhost:1420` and navigates straight here, bypassing the
  // packed `app.dat` flow. Fixed port so the launchSettings profile can hard-code it.
  clearScreen: false,
  server: {
    port: 1420,
    strictPort: true,
  },
})
