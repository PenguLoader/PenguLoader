
var openDevTools = function (remote) {
    native function OpenDevTools();
    OpenDevTools(Boolean(remote));
};

var openAssetsFolder = function () {
    native function OpenAssetsFolder();
    OpenAssetsFolder();
};

var openPluginsFolder = function () {
    native function OpenPluginsFolder();
    OpenPluginsFolder();
};

var reloadClient = function () {
    native function ReloadClient();
    ReloadClient();
};

var restartClient = function () {
    fetch('/riotclient/kill-and-restart-ux', {
        method: 'POST'
    });
};

var DataStore = new function () {
    native function LoadData();
    native function SaveData();

    let _data;
    function data() {
        if (!(_data instanceof Map)) {
            try {
                var object = JSON.parse(LoadData());
                _data = new Map(Object.entries(object));
            } catch {
                _data = new Map();
            }
        }
        return _data;
    }

    function commitData() {
        var object = Object.fromEntries(_data);
        SaveData(JSON.stringify(object));
    }

    return {
        [Symbol.toStringTag]: 'DataStore',
        has(key) {
            return data().has(String(key));
        },
        get(key, fallback) {
            key = String(key);
            if (data().has(key)) {
                return data().get(key);
            }
            return fallback;
        },
        set(key, value) {
            if (typeof key === 'function' || typeof key === 'object') {
                return false;
            } else {
                data().set(String(key), value);
                commitData();
                return true;
            }
        },
        remove(key) {
            var result = data().delete(String(key));
            commitData();
            return result;
        }
    };
};

var Effect = new function () {
    native function GetEffect();
    native function ApplyEffect();
    native function ClearEffect();

    let listeners = {
        apply: [],
        clear: [],
    };

    function triggerCallbacks(name, ...args) {
        var callbacks = listeners[name];
        if (Array.isArray(callbacks)) {
            for (var callback of callbacks) {
                callback?.apply(null, args);
            }
        }
    }

    return {
        [Symbol.toStringTag]: 'Effect',
        get current() {
            return GetEffect() || null;
        },
        apply(name, options) {
            var old = GetEffect();
            var success = ApplyEffect(name, options);
            if (success) {
                triggerCallbacks('apply', { old, name, options });
            }
            return success;
        },
        clear() {
            ClearEffect();
            triggerCallbacks('clear');
        },
        on(event, callback) {
            var callbacks = listeners[event];
            if (Array.isArray(callbacks)) {
                var idx = callbacks.indexOf(callback);
                if (idx < 0) {
                    callbacks.push(callback);
                }
            }
        },
        off(event, callback) {
            var callbacks = listeners[event];
            if (Array.isArray(callbacks)) {
                var idx = callbacks.indexOf(callback);
                if (idx >= 0) {
                    callbacks.splice(idx, 1);
                }
            }
        }
    };
};

var requireFile = function (path) {
    native function RequireFile();
    return RequireFile(path);
};

var AuthCallback = new function () {
    native function CreateAuthCallbackURL();
    native function AddAuthCallback();
    native function RemoveAuthCallback();

    return {
        [Symbol.toStringTag]: 'AuthCallback',
        createURL() {
            return CreateAuthCallbackURL();
        },
        readResponse(url, timeout) {
            if (typeof timeout !== 'number' || timeout <= 0) {
                timeout = 180000;
            }
            return new Promise(resolve => {
                let fired = false;
                AddAuthCallback(url, response => {
                    fired = true;
                    resolve(response);
                });
                setTimeout(() => {
                    RemoveAuthCallback(url);
                    if (!fired) {
                        resolve(null);
                    }
                }, timeout);
            });
        }
    };
};

var __hookEvents = function () {
    var windowLoaded = false;
    var windowDOMLoaded = false;
    var documentDOMLoaded = false;

    window.addEventListener('load', function () {
        windowLoaded = true;
    });

    window.addEventListener('DOMContentLoaded', function () {
        windowDOMLoaded = true;
    });

    document.addEventListener('DOMContentLoaded', function () {
        documentDOMLoaded = true;
    });

    function trigger(listener) {
        try {
            listener.call(window);
        } catch (err) {
            console.error(err);
        }
    }

    var windowAddEventListener = window.addEventListener;
    window.addEventListener = function (type, listener, options) {
        if (typeof listener === 'function') {
            if (windowLoaded && type === 'load') {
                trigger(listener);
            } else if (windowDOMLoaded && type === 'DOMContentLoaded') {
                trigger(listener);
            }
        }
        windowAddEventListener.call(window, type, listener, options);
    };

    var documentAddEventListener = document.addEventListener;
    document.addEventListener = function (type, listener, options) {
        if (typeof listener === 'function') {
            if (documentDOMLoaded && type === 'DOMContentLoaded') {
                trigger(listener);
            }
        }
        documentAddEventListener.call(document, type, listener, options);
    };

    delete window['__hookEvents'];
};

var __initSuperPotatoMode = function () {
    const GLOBAL_STYLE = `
  *:not(.store-loading):not(.spinner), *:before, *:after {
    transition: none !important;
    transition-property: none !important;
    animation: none !important;
  }
`;
    const SHADOW_STYLE = `
  *:not(.spinner), *:before, *:after {
    transition: none !important;
    transition-property: none !important;
    animation: none !important;
  }
  .section-glow {
    transform: none !important;
  }
`;
    function main() {
        const style = document.createElement('style');
        style.textContent = GLOBAL_STYLE;
        document.body.appendChild(style);

        const createElement = document.createElement;
        document.createElement = function (name) {
            const elm = createElement.apply(this, arguments);

            if (elm.shadowRoot && elm.shadowRoot.children.length > 0) {
                const style = elm.shadowRoot.children[0];
                if (style instanceof HTMLStyleElement) {
                    style.textContent += SHADOW_STYLE;
                }
            }

            return elm;
        };

        fetch('/lol-settings/v1/local/lol-user-experience', {
            method: 'PATCH',
            headers: {
                "content-type": "application/json"
            },
            body: JSON.stringify({
                schemaVersion: 3,
                data: { potatoModeEnabled: true }
            })
        });
    }

    window.addEventListener('load', main);

    delete window['__initSuperPotatoMode'];
};