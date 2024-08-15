/// <reference types="vite/client" />

declare module 'virtual:pengu-env' {
  const env: {
    isMac: boolean
    version: string
  }
  export default env
}