import * as yaml from 'js-yaml';
import * as toml from 'smol-toml';

const pkg = {
  yaml: {
    parse: yaml.load,
    stringify: yaml.dump,
  },
  toml: {
    parse: toml.parse,
    stringify: toml.stringify
  },
};

// @ts-ignore
window.__p = function (name: string) {
  return pkg[name];
};