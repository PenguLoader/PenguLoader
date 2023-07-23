import native from './native';

let data_: Map<string, any>;

function data() {
  if (data_ === undefined) {
    try {
      var object = JSON.parse(native.LoadDataStore());
      data_ = new Map(Object.entries(object));
    } catch {
      data_ = new Map();
    }
  }
  return data_;
}

function commit() {
  var object = Object.fromEntries(data_);
  native.SaveDataStore(JSON.stringify(object));
}

window.DataStore = {

  has(key) {
    return data().has(String(key));
  },

  get(key, fallback) {
    if (typeof key !== 'string') {
      return undefined;
    } else if (data().has(key)) {
      return data().get(key);
    }
    return fallback;
  },

  set(key, value) {
    if (typeof key !== 'string') {
      return false;
    }
    data().set(String(key), value);
    commit();
    return true;
  },

  remove(key) {
    var result = data().delete(String(key));
    commit();
    return result;
  }
}