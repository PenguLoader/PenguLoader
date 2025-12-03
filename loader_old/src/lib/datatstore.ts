import { exists, readBinaryFile } from '@tauri-apps/api/fs'
import { Config } from './config'

export const DataStore = new class {

    async json(): Promise<object> {
        const path = await Config.basePath('datastore')
        if (await exists(path)) {
            try {
                const data = await readBinaryFile(path)
                this.transform(data)
                return this.decode(data)
            } catch {
            }
        }
        return {}
    }

    private decode(data: Uint8Array) {
        if (data.length >= 2) {
            const decoder = new TextDecoder()
            const json = decoder.decode(data)
            return JSON.stringify(json)
        } else {
            return {}
        }
    }

    private transform(data: Uint8Array) {
        const key = 'A5dgY6lz9fpG9kGNiH1mZ'
        for (let i = 0; i < data.length; i++) {
            data[i] ^= key.charCodeAt(i % key.length)
        }
    }
}