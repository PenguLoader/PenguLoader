import { fallback, translations } from './trans.json';

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

export function loadTranslation() {
  const locale = document.body.dataset['locale'] as string;
  _T = (locale && findTranslation(locale)) || findTranslation(fallback);
}

export function _t(key: TranslationKey) {
  return _T[key] || `{{${key}}}`;
}