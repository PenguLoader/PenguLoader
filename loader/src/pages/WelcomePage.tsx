import { Component, createSignal } from 'solid-js'
import { Button, ComboBox, Checkbox } from '../components/ui'
import { useI18n } from '../lib/i18n'
import { useConfig } from '../lib/config'

export const WelcomePage: Component<{
  onDone: () => void
}> = (props) => {

  const i18n = useI18n()
  const config = useConfig()
  const [accepted, setAccepted] = createSignal(false)

  const selectLang = async (id: string) => {
    i18n.switchTo(id)
    await config.app.language(id)
  }

  return (
    <div class="flex flex-col justify-center items-center my-auto">
      <div class="mb-8">
        <h2 class="text-4xl font-semibold text-center">{i18n.t('welcome')}</h2>
      </div>
      <div class="flex flex-col gap-8 max-w-60">
        <div class="gap-2">
          <p class="text-sm my-2 pl-2">{i18n.t('choose_lang')}</p>
          <ComboBox
            items={i18n.languages}
            selected={config.app.language()}
            onSelect={selectLang}
          />
        </div>
        <label class="flex space-x-2">
          <Checkbox checked={accepted()} onClick={() => setAccepted(v => !v)} />
          <div class="grid gap-1.5 leading-none">
            <h2 class="text-sm font-medium leading-none">{i18n.t('accept_tos')}</h2>
            <p class="text-sm text-muted-foreground">{i18n.t('tos_content')}</p>
          </div>
        </label>
        <div class="mt-4">
          <Button class="w-full" onClick={props.onDone} disabled={!accepted()}>
            {i18n.t('get_started')}
          </Button>
        </div>
      </div>
    </div>
  )
}