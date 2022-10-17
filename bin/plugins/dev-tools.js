const observer = new MutationObserver(mutations => {
    const panel = document.querySelector('div.lol-settings-options > lol-uikit-scrollable')
    if (panel && mutations.find(record => Array.from(record.addedNodes).includes(panel))) {
        const row = document.createElement('div')
        row.classList.add('lol-settings-general-row')

        const label = document.createElement('p')
        label.classList.add('lol-settings-window-size-text')
        label.textContent = 'Developer tools'

        const btn = document.createElement('lol-uikit-flat-button-secondary')
        btn.style.display = 'flex'
        btn.textContent = 'Show DevTools (Ctrl Shift I)'
        btn.onclick = () => { window.openDevTools() }

        const link = document.createElement('p')
        link.classList.add('lol-settings-code-of-conduct-link')
        link.classList.add('lol-settings-window-size-text')

        const a = document.createElement('a')
        a.textContent = 'Open in browser'
        a.href = '#' + Math.random()
        a.target = '_blank'
        a.onclick = () => { window.openDevTools(true); return false }

        link.append(a)

        row.append(label)
        row.append(link)
        row.append(btn)

        panel.prepend(row)
    }
})

window.addEventListener('load', () => {
    const interval = setInterval(() => {
        const manager = document.getElementById('lol-uikit-layer-manager-wrapper')
        if (manager) {
            clearInterval(interval)
            observer.observe(manager, {
                attributes: true,
                childList: true,
                subtree: true,
                characterData: true
            })
        }
    }, 500)

    document.addEventListener('keydown', (e) => {
        if (e.ctrlKey && e.shiftKey && e.code === 'KeyI') {
            e.preventDefault()
            window.openDevTools()
            return false
        }
    })
})