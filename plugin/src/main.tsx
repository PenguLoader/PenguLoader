import { render } from 'preact';
import { App } from './app';
import './app.scss';

async function init() {
  if (window.__llver) {
    // Wait for manager layer
    while (!document.getElementById('lol-uikit-layer-manager-wrapper')) {
      await new Promise(r => setTimeout(r, 200));
    }
  }

  // Get root
  let root = document.getElementById('league-loader-plugin');
  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', 'league-loader-plugin');
    document.body.appendChild(root);
  }

  // Render app
  render(<App />, root);
}

init();