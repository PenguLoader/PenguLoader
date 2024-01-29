import * as yaml from 'js-yaml';
import * as toml from 'smol-toml';

export const YAML = {
  parse: yaml.load,
  stringify: yaml.dump,
};

export const TOML = {
  parse: toml.parse,
  stringify: toml.stringify
};