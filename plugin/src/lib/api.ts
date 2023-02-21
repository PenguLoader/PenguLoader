import SemVer from 'semver';
import { GITHUB_REPO } from './constants';

export async function getSummonerName() {
  const res = await fetch('/lol-summoner/v1/current-summoner');
  return (await res.json())['displayName'];
}

export async function fetchUpdate() {
  const currentVersion = window['__llver'];
  if (typeof currentVersion !== 'string') {
    try {
      const res = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases/latest`);
      const release = await res.json();
      const latestVersion: string = release['tag_name'];
      if (SemVer.gt(latestVersion, currentVersion)) {
        return latestVersion;
      }
    } catch (err) {
      console.warn('Failed to fetch update', err);
    }
  }
  return null;
}