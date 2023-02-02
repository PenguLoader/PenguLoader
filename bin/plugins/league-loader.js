// Create UI using nano-jsx
const addSettingsUI = async (root) => {
    const { Component, jsx, render } = await import('https://cdn.jsdelivr.net/npm/nano-jsx/+esm')

    class Settings extends Component {
        visible = false
        frame = null
        opener = null
        didMount() {
            this.opener = document.querySelector('div[action=settings]')
            this.opener.addEventListener('click', e => {
                if (!this.visible) {
                    e.stopImmediatePropagation()
                    this.show(true)
                }
            })
        }
        show(on) {
            this.visible = on
            this.update()
            if (this.visible) {
                this.frame.shadowRoot.querySelector('lol-uikit-close-button')
                    ?.addEventListener('click', () => this.show(false))
            }
        }
        showDefaultSettings() {
            this.opener.click()
            this.show(false)
        }
        render() {
            return jsx/*html*/`
                <div class="modal" style="position: absolute; inset: 0px; z-index: 8500" hidden=${!this.visible || undefined}>
                    <lol-uikit-full-page-backdrop class="backdrop" style="display: flex; align-items: center; justify-content: center; position: absolute; inset: 0px" />
                    <div class="dialog-confirm" style="display: flex; align-items: center; justify-content: center; position: absolute; inset: 0px">
                        <lol-uikit-dialog-frame ref=${el => (this.frame = el)} class="dialog-frame" orientation="bottom" close-button="false">
                            <div class="dialog-content">
                                <lol-uikit-content-block class="app-controls-exit-dialog" type="dialog-medium" style="position: relative; overflow: hidden">
                                    <div style="position: absolute; top: 60px">
                                        <video autoplay loop muted
                                            src="https://assets.contentstack.io/v3/assets/blt2ac872571a60ee02/blt7a72b1686eb3219a/618d75137ae6ce6fab413b1f/background-video-d-02.mp4"
                                            style="object-fit: cover; object-position: center center; height: 100%; width: 100%; transform-origin: center center; transform: scale(2.5)">
                                        </video>
                                    </div>
                                    <div style="position: relative">
                                        <div style="margin-bottom: 24px">
                                            <h4 style="padding: 6px 0">League Loader</h4>
                                            <p>v0.6.1</p>
                                        </div>
                                        <hr class="heading-spacer" />
                                        <div style="display: flex; flex-direction: column; align-items: center; gap: 12px">
                                            <lol-uikit-flat-button-secondary style="display:inline-block; width: 180px" onClick=${() => window.openDevTools()}>
                                                Open DevTools (F12)
                                            </lol-uikit-flat-button-secondary>
                                            <lol-uikit-flat-button-secondary style="display:inline-block; width: 180px" onClick=${() => window.location.reload()}>
                                                Reload Client (Ctrl Shift R)
                                            </lol-uikit-flat-button-secondary>
                                            <lol-uikit-flat-button-secondary style="display:inline-block; width: 180px" onClick=${() => window.openPluginsFolder()}>
                                                Open plugins folder
                                            </lol-uikit-flat-button-secondary>
                                        </div>
                                        <hr class="heading-spacer" />
                                        <p style="padding: 20px 0" class="lol-settings-code-of-conduct-link lol-settings-window-size-text">
                                            <a href="https://leagueloader.app" target="_blank">leagueloader.app</a>
                                        </p>
                                    </div>
                                </lol-uikit-content-block>
                            </div>
                            <lol-uikit-flat-button-group type="dialog-frame">
                                <lol-uikit-flat-button tabindex="1" class="button-accept" onClick=${() => this.showDefaultSettings()}>Open Settings</lol-uikit-flat-button>
                                <lol-uikit-flat-button tabindex="2" class="button-decline" onClick=${() => this.show(false)}>CLOSE</lol-uikit-flat-button>
                            </lol-uikit-flat-button-group>
                        </lol-uikit-dialog-frame>
                    </div>
                </div>
            `
        }
    }

    render(jsx`<${Settings} />`, root)
}

window.addEventListener('load', async () => {
    // Wait for manager layer
    const manager = () => document.getElementById('lol-uikit-layer-manager-wrapper')
    while (!manager()) await new Promise(r => setTimeout(r, 200))
    // Create UI and mount
    const root = document.createElement('div')
    await addSettingsUI(root)
    manager().prepend(root)
})

// Set hotkey, an added iframe can steal focus
window.addEventListener('keydown', (e) => {
    if ((e.ctrlKey && e.shiftKey && e.code === 'KeyI') || e.code === 'F12') {
        window.openDevTools()
    } else if (e.ctrlKey && e.shiftKey && e.code === 'KeyR') {
        window.location.reload()
    }
})