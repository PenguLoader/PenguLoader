/* @refresh reload */
import { render } from 'solid-js/web';
import { loadTranslation } from './lib/i18n';
import App from './App';

import './styles/global.css';
import styles from './styles/scoped.css?inline';

async function layerManager() {
  // browser env
  if (!navigator.userAgent.includes('League')) {
    return document.body;
  }
  while (true) {
    let elm = document.getElementById('lol-uikit-layer-manager');
    if (elm != null) return elm;
    await new Promise(r => setTimeout(r, 100));
  }
}

async function mount() {

  await loadTranslation();
  const manager = await layerManager();

  const rootId = 'pengu-root';
  let root = document.getElementById(rootId);

  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', rootId);
    manager.appendChild(root);
  }

  const sheet = new CSSStyleSheet();
  sheet.replaceSync(styles);

  const shadow = root.attachShadow({ mode: 'open' });
  shadow.adoptedStyleSheets.push(sheet);

  render(() => <App />, shadow);
}

window.addEventListener('load', mount);