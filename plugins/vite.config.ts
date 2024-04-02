import { defineConfig } from 'vite';

export default defineConfig({
  build: {
    target: ['es2020'],
    assetsInlineLimit: 1024 * 64,
    minify: false,
    modulePreload: false,
    sourcemap: "inline",
    lib: {
      name: 'preload',
      entry: 'src/index.ts',
      fileName: "preload.js",
      formats: ["iife"],
    },
    rollupOptions: {
      output: {
        format: 'iife',
        entryFileNames: 'preload.js'
      }
    }
  }
})