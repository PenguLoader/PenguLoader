const GLOBAL_STYLE = `
*:not(.store-loading):not(.spinner):not([animated]):not(.lol-loading-screen-spinner):not(.lol-uikit-vignette-celebration-layer *), *:before, *:after {
  transition: none !important;
  transition-property: none !important;
  /* animation: none !important; */
}`;

const SHADOW_STYLE = `
*:not(.spinner):not([animated]), *:before, *:after {
  transition: none !important;
  transition-property: none !important;
  /* animation: none !important; */
}`;

function load() {
  const style = document.createElement('style');
  style.textContent = GLOBAL_STYLE;
  document.body.appendChild(style);

  const createElement = document.createElement;
  document.createElement = function (name, options?) {
    const elm = createElement.call(this, name, options);

    if (elm.shadowRoot) {
      const style = document.createElement('style');
      style.textContent = SHADOW_STYLE;
      elm.shadowRoot.appendChild(style);
    }

    return elm;
  };

  fetch('/lol-settings/v1/local/lol-user-experience', {
    method: 'PATCH',
    headers: {
      'content-type': 'application/json'
    },
    body: JSON.stringify({
      schemaVersion: 3,
      data: { potatoModeEnabled: true }
    })
  });
}

if (window.Pengu.superPotato) {
  window.addEventListener('load', load);
}

export { }