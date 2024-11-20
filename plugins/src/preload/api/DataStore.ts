import { native } from './native';

let data_ = new Map<string, any>();

async function initDataStore() {
  try {
    const json = await native.LoadDataStore();
    const object = JSON.parse(json);
    data_ = new Map(Object.entries(object));
  } catch {
    console.warn('Failed to parse DataStore, empty data will be used.');
  }
}

async function commit() {
  const object = Object.fromEntries(data_);
  const json = JSON.stringify(object);
  await native.SaveDataStore(json);
}

window.DataStore = {

  has(key) {
    return data_.has(String(key));
  },

  get(key, fallback) {
    if (typeof key !== 'string') {
      return undefined;
    } else if (data_.has(key)) {
      return data_.get(key);
    }
    return fallback;
  },

  async set(key, value) {
    if (typeof key !== 'string') {
      return false;
    }
    data_.set(String(key), value);
    await commit();
    return true;
  },

  async remove(key) {
    let result = data_.delete(String(key));
    await commit();
    return result;
  }
}

export {
  initDataStore
}