let windowLoaded = false;

window.addEventListener('load', function () {
  windowLoaded = true;
})

const windowAddEventListener = window.addEventListener;
const documentAddEventListener = document.addEventListener;

// Make sure the late window's load & DOMContentLoaded listeners are called
window.addEventListener = function (type, listener, options) {
  if (type === 'load' && windowLoaded) {
    setTimeout(listener, 1);
  } else if (type === 'DOMContentLoaded' && document.readyState === 'complete') {
    setTimeout(listener, 1);
  } else {
    windowAddEventListener.call(this, type, listener, options);
  }
};

// Make sure the late document's DOMContentLoaded listeners are called
document.addEventListener = function (type, listener, options) {
  if (type === 'DOMContentLoaded'
    && (document.readyState === 'interactive'
      || document.readyState === 'complete')) {
    setTimeout(listener, 1);
  } else {
    documentAddEventListener.call(this, type, listener, options);
  }
};

export { }