import { native } from './native';

import './DataStore';
import './Effect';
import './json';   // installs window.__pwj before any plugin runs

window.openDevTools = function () {
  native.OpenDevTools();
};

window.openPluginsFolder = function (path?: string) {
  if (typeof path === 'string' && path) {
    // Reject Windows absolute paths (drive letter + colon, e.g. "C:\...").
    if (!path.startsWith('..') && !/[\\\/]\.\.[\\\/]/.test(path) && !/^[a-zA-Z]:/.test(path)) {
      if (/^[\\/]/.test(path))
        path = path.substring(1);
      return native.OpenPluginsFolder(path);
    }
  }
  return native.OpenPluginsFolder();
};

window.reloadClient = function () {
  native.ReloadClient();
};

window.restartClient = function () {
  fetch('/riotclient/kill-and-restart-ux', {
    method: 'POST'
  });
};

window.getScriptPath = function () {
  const error = new Error();
  const stack = error.stack;
  return stack?.match(/(?:http|https):\/\/[^\s]+\.js/g)?.[0];
};

export { }