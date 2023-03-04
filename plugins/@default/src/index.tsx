/* @refresh reload */
import { render } from 'solid-js/web';
import App from './App';
import { getSummonerName } from './lib/api';
import './index.scss';

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
  let root = document.getElementById('pengu-loader-plugin');
  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', 'pengu-loader-plugin');
    document.body.appendChild(root);
  }

  // Render app
  render(() => <App />, root);
}

init();