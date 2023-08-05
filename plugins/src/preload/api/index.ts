import native from './native';

import './AuthCallback';
import './DataStore';
import './Effect';

window.openDevTools = function (remote?: boolean) {
  native.OpenDevTools(Boolean(remote));
};

window.openAssetsFolder = function () {
  native.OpenAssetsFolder();
};

window.openPluginsFolder = function (path?: string) {
  if(arguments.length > 1) return false;
  if (path === undefined) return native.OpenPluginsFolder();
  if(typeof path !== 'string') return false;

  if(path.includes('..')) return false;
  if(path === "") path = ".";

  return native.OpenPluginsFolder(path);
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