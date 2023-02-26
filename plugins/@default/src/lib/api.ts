/**
 * Get current summoner name.
 */
export async function getSummonerName() {
  const res = await fetch('/lol-summoner/v1/current-summoner');
  return (await res.json())['displayName'] as string;
}