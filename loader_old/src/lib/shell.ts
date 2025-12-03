import { invoke, shell } from '@tauri-apps/api'

export const Shell = {
    /**
     * Expand a folder in file explorer.
     * @param path Absolute path to folder.
     */
    async expandFolder(path: string) {
        await invoke('plugin:shell|expand_folder', {
            path: path
        })
    },

    /**
     * Reveal a file in file explorer.
     * @param path Absolute path to file.
     */
    async revealFile(path: string) {
        await invoke('plugin:shell|reveal_file', {
            path: path
        })
    },

    /**
     * Open an external link.
     * @param url URL.
     */
    async openLink(url: string) {
        if (typeof url === 'string' && url.startsWith('https://')) {
            await shell.open(url)
        }
    },
}