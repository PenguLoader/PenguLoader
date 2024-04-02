import { defineConfig } from 'vite';

export default defineConfig(({ mode }) => ({
  build: {
    ...mode == "production" ? {
      minify: true,
      sourcemap: false,
    } : {
      minify: false,
      sourcemap: "inline",
    },

    target: ['es2020'],
    assetsInlineLimit: 1024 * 64,
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
}));