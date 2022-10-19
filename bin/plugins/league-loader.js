// Common UI components
const UI = {
    Row: (childs) => {
        const row = document.createElement('div')
        row.classList.add('lol-settings-general-row')
        if (Array.isArray(childs))
            childs.forEach(el => row.appendChild(el))
        return row
    },
    Label: (text) => {
        const label = document.createElement('p')
        label.classList.add('lol-settings-window-size-text')
        label.innerText = text
        return label
    },
    Link: (text, href, onClick) => {
        const link = document.createElement('p')
        link.classList.add('lol-settings-code-of-conduct-link')
        link.classList.add('lol-settings-window-size-text')

        const a = document.createElement('a')
        a.innerText = text
        a.target = '_blank'
        a.href = href
        a.onclick = onClick || null

        link.append(a)
        return link
    },
    Button: (text, onClick) => {
        const btn = document.createElement('lol-uikit-flat-button-secondary')
        btn.innerText = text
        btn.onclick = onClick
        btn.style.display = 'inline-block'
        return btn
    }
}

// Add controls to settings panel
const injectSettings = (panel) => {
    panel.prepend(
        UI.Row([
            UI.Label('League Loader'),
            UI.Button('Show DevTools (F12)', () => window.openDevTools()),
            UI.Link('Open in browser', `#${Math.random()}`, () => (window.openDevTools(true), false)),
            UI.Button('Reload Client (Ctrl Shift R)', () => window.location.reload())
        ])
    )
}

window.addEventListener('load', () => {
    // Wait for manager layer
    const interval = setInterval(() => {
        const manager = document.getElementById('lol-uikit-layer-manager-wrapper')
        if (manager) {
            clearInterval(interval)
            // Observe settings panel
            new MutationObserver(mutations => {
                const panel = document.querySelector('div.lol-settings-options > lol-uikit-scrollable')
                if (panel && mutations.some(record => Array.from(record.addedNodes).includes(panel))) {
                    // Inject settings
                    injectSettings(panel)
                }
            }).observe(manager, {
                childList: true,
                subtree: true
            })
        }
    }, 500)
})

// Set hotkey, an added iframe can steal focus
window.addEventListener('keydown', (e) => {
    if ((e.ctrlKey && e.shiftKey && e.code === 'KeyI') || e.code === 'F12') {
        window.openDevTools()
    } else if (e.ctrlKey && e.shiftKey && e.code === 'KeyR') {
        window.location.reload()
    }
})

exports.UI = UI