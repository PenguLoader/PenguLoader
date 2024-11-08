import { createRoot } from 'solid-js'
import { createStore } from 'solid-js/store'
import translations from '../../translations.json'
import { useConfig } from './config'

const EN = translations.languages[0]
type TranslationKey = keyof typeof EN.translations
type TranslationMap = Record<TranslationKey, string>

const _i18n = createRoot(() => {
  const [current, set] = createStore<TranslationMap>({ ...EN.translations })

  const languages = translations.languages.map((x) => ({
    id: x.id,
    name: x.name,
  }))

  const switchTo = (id: string) => {
    for (const lang of translations.languages) {
      if (lang.id === id) {
        set({ ...lang.translations })
      }
    }
  }

  const text = (key: TranslationKey): string => {
    if (key in current) {
      return current[key]
    }
    return `{{${key}}}`
  }

  return {
    languages,
    switchTo,
    t: text,
  }
})

export const useI18n = () => {
  _i18n.switchTo(useConfig().app.language())
  return _i18n
}