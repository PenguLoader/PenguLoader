function load() {
  const loadingDiv = document.querySelector("div");
  loadingDiv?.classList.add("silent-mode");
}

if (window.Pengu.silentMode) {
  window.addEventListener('load', load);
}

export { }