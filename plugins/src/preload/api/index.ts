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

window.openSpecifiedPluginFodler = function (pluginName:string, relativePath:string) {
  if(arguments.length != 2) return false;
  if(typeof pluginName !== "string" || typeof relativePath !== "string") return false;

  if(pluginName.includes("..") || relativePath.includes("..")) return false;
  if(pluginName==="") pluginName = "."
  if(relativePath==="") relativePath = "."

  return native.OpenSpecifiedPluginFodler(pluginName,relativePath);
}

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