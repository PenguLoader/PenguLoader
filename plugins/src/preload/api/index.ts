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

window.openPluginsFolder = function () {
  native.OpenPluginsFolder();
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