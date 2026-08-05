/* @refresh reload */
import { render } from 'solid-js/web';
import App from './App';
import './style.css';

import { rcp } from './shared';
import { loadTranslation } from './lib/i18n';

// Light DOM. Our CSS uses `pengu-` prefixed classes (see styles/_tokens.scss
// and per-component .scss files) to avoid colliding with LCUX's own styles —
// no shadow root required.

const rootId = 'pengu-root';

// The `rcp.preInit('rcp-fe-lol-shared-components')` pre-warm that used to live
// here now runs in the core (`preload/index.ts`). Push-style RCP requires the
// subscription to be in place before LCUX announces, and this chunk is loaded
// asynchronously — by the time it evaluates, the announce may already have
// happened. mount() still awaits fulfillment below, which is what actually
// gates the render.

async function mount() {
  // rcp-fe-lol-shared-components does `document.body.innerHTML += ...` during
  // its init. Wait until it's fulfilled so its body manipulation is done
  // before we attach — otherwise our root gets destroyed and re-parsed,
  // remounting <App /> as a duplicate tree.
  await rcp.whenReady('rcp-fe-lol-shared-components').catch(() => {});

  await loadTranslation();

  let root = document.getElementById(rootId);
  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', rootId);
    document.body.appendChild(root);
  }

  render(() => <App />, root);
}

window.addEventListener('load', mount);
