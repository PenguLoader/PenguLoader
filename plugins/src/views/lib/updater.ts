import snarkdown from 'snarkdown';

const GITHUB_REPO = 'PenguLoader/PenguLoader';

function parseVersion(version: string) {
  const match = /v?(\d+(?:\.\d+){2,3})/i.exec(version);
  if (!match) return 0;
  const nums = match[1].split('.').map(Number);
  return (nums[0] * 10000 ** 2)
    + (nums[1] * 10000) + (nums[2])
    + ((nums[3] ?? 0) / 10000);
}

export async function fetchUpdate() {
  const currentVersion = window.Pengu?.version || window.__llver || 'v0.0.0';
  try {
    const res = await fetch(`https://api.github.com/repos/${GITHUB_REPO}/releases/latest`);
    const release = await res.json();
    const latestVersion: string = release['tag_name'];
    if (parseVersion(latestVersion) > parseVersion(currentVersion)) {
      return {
        old: currentVersion,
        version: latestVersion,
        changelog: snarkdown(release['body'])
      };
    }
  } catch (err) {
    console.warn('Pengu failed to fetch update.', err);
  }
  return false;
}