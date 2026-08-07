import './api';
import './super-potato';
import './load-hooks';
import './loader';

import { native } from './api/native';

// @ts-ignore
window.Pengu.version = __VERSION__;

// The database backing the per-plugin store. Surfaced here for the same reason
// as `version` — it is the sort of thing a bug report should be able to state
// without a debugger, and the hub can show it. See docs/plugin-storage.md.
// @ts-ignore
window.Pengu.storageVersion = native.StorageVersion();

Object.freeze(window.Pengu);