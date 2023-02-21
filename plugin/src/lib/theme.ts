
export function isDarkMode() {
    return window.matchMedia
    && window.matchMedia('(prefers-color-scheme: dark)').matches;
}