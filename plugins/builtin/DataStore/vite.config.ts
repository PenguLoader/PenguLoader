import { defineConfig } from 'vite';

export default defineConfig({
    build: {
        assetsInlineLimit: 1024 * 64,
        minify: false,
        modulePreload: false,
        sourcemap: "inline",
        lib: {
          name: 'pengufs',
          entry: 'src/index.ts',
          fileName: "pengufs.js",
          formats: ["es"],
        },
        rollupOptions: {
          output: {
            format: 'es',
            entryFileNames: 'pengufs.js'
          }
        }
    }
})