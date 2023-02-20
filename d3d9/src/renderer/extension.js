
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
        async set(key, value) {
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

//var __require;

//(function () {
//    var join = function (a, b) {
//        var parts = a.split("/").concat(b.split("/"));
//        var newParts = [];

//        for (var i = 0, l = parts.length; i < l; i++) {
//            var part = parts[i];
//            if (!part || part === ".") continue;
//            if (part === "..") newParts.pop();
//            else newParts.push(part);
//        }

//        if (parts[0] === "") newParts.unshift("");
//        return newParts.join("/") || (newParts.length ? "/" : ".");
//    };

//    var global = {};    // Global object.
//    var modules = {};   // Modules map.
//    var paths = [''];   // Path stack.

//    __require = function (name) {
//        native function Require();

//        // Check invalid name.
//        if (typeof name !== "string" || name === "") {
//            throw Error("Module name is required.")
//        }

//        // Get current dir.
//        var dir = paths[paths.length - 1];
//        var path = join(dir, name);
//        var mod = modules[path];

//        if (typeof mod === "object") {
//            // Found exported.
//            return mod.exports;
//        } else {
//            // Require path.
//            var data = Require(path);

//            if (data === null) {
//                throw Error("Cannot find module '" + name + "'");
//            } else {
//                var source = data[0];
//                var type = data[1];

//                var _M = {};

//                // Textual
//                if (type === 2) {
//                    _M.source = source;
//                    // JSON
//                    if (/\.json$/i.test(name)) {
//                        _M.exports = JSON.parse(source);
//                    } else {
//                        _M.exports = source;
//                    }
//                } else {
//                    // Default exports.
//                    _M.exports = {};
//                    // Fake source map URL.
//                    _M.source = source + "\n//# sourceURL=@plugins" + path +
//                        (type === 1 ? "/index" : ".js");

//                    try {
//                        // Create function.
//                        var code = "\"use strict\";eval(module.source);";
//                        var exec = Function("module", "exports", "require", "global", code);

//                        // Push current dir.
//                        paths.push(join(path, ".."));
//                        // Execute.
//                        exec(_M, _M.exports, __require, global);
//                    } catch (err) {
//                        throw err;
//                    } finally {
//                        paths.pop();
//                    }

//                    if (type === 1) {
//                        modules[path + "/index"] = _M;
//                    }
//                }

//                modules[path] = _M;
//                return _M.exports;
//            }
//        }
//    };
//})();