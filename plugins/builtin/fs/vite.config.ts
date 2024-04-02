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
}));