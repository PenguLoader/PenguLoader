import 'uno.css';
import { render } from 'solid-js/web';
import 'solid-devtools';

import App from './App';
import './global.css';

const root = document.createElement('div');
root.id = 'pengu-root';

document.body.appendChild(root);
render(() => <App />, root!);