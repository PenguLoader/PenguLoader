
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

var DataStore = new function () {
    native function LoadData();
    native function SaveData();

    let data;
    try {
        var object = JSON.parse(LoadData());
        data = new Map(Object.entries(object));
    } catch {
        data = new Map();
    }

    function commitData() {
        var object = Object.fromEntries(data);
        SaveData(JSON.stringify(object));
    }

    return {
        [Symbol.toStringTag]: 'DataStore',
        has(key) {
            return data.has(key);
        },
        get(key) {
            return data.get(key);
        },
        set(key, value) {
            data.set(key, value);
            commitData();
        },
        remove(key) {
            var result = data.delete(key);
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