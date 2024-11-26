/* @refresh reload */
import { render } from 'solid-js/web';
import App from './App';
import './style.css';

import { twind, cssom, observe } from '@twind/core';
import config from '../../twind.config';
import { loadTranslation } from './lib/i18n';

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

  const sheet = cssom(new CSSStyleSheet());
  const tw = twind(config, sheet);

  const shadow = root.attachShadow({ mode: 'open' });

  shadow!.adoptedStyleSheets = [sheet.target];
  observe(tw, shadow);

  render(() => <App />, shadow);
}

window.addEventListener('load', mount);