/* @refresh reload */
import { render } from 'solid-js/web';
import App from './App.tsx';
import './style.css';

function mount() {
  const rootId = 'pengu';
  let root = document.getElementById(rootId);

  if (!root) {
    root = document.createElement('div');
    root.setAttribute('id', rootId);
    document.body.appendChild(root);
  }

  render(() => <App />, root!);
}

window.addEventListener('load', mount);