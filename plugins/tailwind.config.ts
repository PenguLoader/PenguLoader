import type { Config } from 'tailwindcss';

const config: Config = {
  content: [
    './index.html',
    './src/**/*.{ts,tsx,css,html,scss}',
  ],
  // prefix: 'tw-',
  important: '#pengu',
  corePlugins: {
    preflight: false,
  },
  // darkMode: 'class',
  theme: {
    extend: {
      colors: {
        blue: {
          50: '#CAF0F8',
          100: '#ADE8F4',
          200: '#90E0EF',
          300: '#48CAE4',
          400: '#00B4D8',
          500: '#0096C7',
          600: '#0077B6',
          700: '#023E8A',
          800: '#03045E',
          900: '#0000FF'
        },
        green: {
          50: '#d6ffee',
          100: '#acffdd',
          200: '#83ffcc',
          300: '#30ffaa',
          400: '#00dc82',
          500: '#00bd6f',
          600: '#009d5d',
          700: '#007e4a',
          800: '#005e38',
          900: '#003f25'
        },
      }
    },
  },
  plugins: [],
};

export default config;