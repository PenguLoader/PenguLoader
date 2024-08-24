import './api';
import './polyfills';
import './super-potato';
import './load-hooks';
import './loader';
import { version } from '../../package.json'

window.Pengu.version = version
Object.freeze(window.Pengu);