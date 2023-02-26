import { render } from 'preact';
import { App } from './app';
import { getSummonerName } from './lib/api';
import './app.scss';

async function init() {

  if (window.__llver) {
    // Wait for manager layer
    while (!document.getElementById('lol-uikit-layer-manager-wrapper')) {
      await new Promise(r => setTimeout(r, 300));
    }
    // Wait for logged
    while (!await getSummonerName()) {
      await new Promise(r => setTimeout(r, 300));
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