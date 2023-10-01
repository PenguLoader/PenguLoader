import { fallback, translations } from '../trans.json';

type Translation = Record<string, string>;
type TranslationKey = keyof typeof translations[0];

let _T: Translation;

function findTranslation(locale: string) {
  locale = locale.toLowerCase();
  for (const trans of translations) {
    if (trans._locales.some(l => l.toLowerCase() === locale)) {
      return trans as any;
    }
  }
}

export async function loadTranslation() {
  let locale = fallback;
  try {
    // cant use body's dataset before rcp get loads
    const data = await fetch('/riotclient/region-locale')
      .then(r => r.json());
    locale = data.locale.replace('_', '-');
  } catch {
    // fallback
  }
  _T = (locale && findTranslation(locale)) || findTranslation(fallback);
}

export function _t(key: TranslationKey) {
  return _T[key] || `{{${key}}}`;
}