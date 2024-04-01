/* @refresh reload */
import { render } from 'solid-js/web';
import App from './App';
import './style.css';

import install from '@twind/with-web-components';
import config from '../../twind.config';
import { loadTranslation } from './lib/i18n';

const rootId = 'pengu-root';
const withTwind = install(config);

class PenguRoot extends withTwind(HTMLElement) {
  constructor() {
    super();
    const shadow = this.attachShadow({ mode: 'open' });
    render(() => <App />, shadow);
  }
}

async function mount() {
  await loadTranslation();

  let root = document.getElementById(rootId);
  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', rootId);
    document.body.appendChild(root);
  }

  await customElements.whenDefined(rootId);
  const twind = document.createElement(rootId);
  root.appendChild(twind);
}

customElements.define(rootId, PenguRoot);
// window.addEventListener('load', mount);
if (document.readyState !== "loading") mount();
else document.addEventListener("DOMContentLoaded", mount);