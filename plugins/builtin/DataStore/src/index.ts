const {
  LoadDataStore,
  SaveDataStore
} = Pengu.primordials;

// class DataStore {

// }

let data_: Map<string, any>;

function data() {
  if (data_ === undefined) {
    try {
      var object = JSON.parse(LoadDataStore());
      data_ = new Map(Object.entries(object));
    } catch {
      data_ = new Map();
    }
  }
  return data_;
}

function commit() {
  var object = Object.fromEntries(data_);
  SaveDataStore(JSON.stringify(object));
}

export function has(key: string) {
  return data().has(String(key));
}

export function get(key: string, fallback?: any) {
  if (typeof key !== 'string') {
    return undefined;
  } else if (data().has(key)) {
    return data().get(key);
  }
  return fallback;
}

export function set(key: string, value: any) {
  if (typeof key !== 'string') {
    return false;
  }
  data().set(String(key), value);
  commit();
  return true;
}

export function remove(key: string) {
  var result = data().delete(String(key));
  commit();
  return result;
}

export default {
  has,
  get,
  set,
  remove
}
