if (!Object.hasOwn) {
  Object.hasOwn = function (obj, prop) {
    return Object.prototype.hasOwnProperty.call(obj, prop);
  };
}

if (!Array.prototype.at) {
  Array.prototype.at = function (idx) {
    idx = Number(idx) || 0;
    if (idx < 0) {
      idx = this.length + idx;
      if (idx < 0) {
        return undefined;
      }
    }
    if (idx >= this.length) {
      return undefined;
    }
    return this[idx];
  };
}

export { }