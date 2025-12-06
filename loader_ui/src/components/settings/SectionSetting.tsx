import { VoidComponent } from "solid-js"

type SectionSettingProps = {
  name: string
}

export const SectionSettings: VoidComponent<SectionSettingProps> = (props) => {
  return (
    <div class="general-settings">
      <span class="text formatted-message close-window-title" data-family="sans" data-bold="false" data-scale="LabelXS" data-testid="text">
        <span class="formatted-message case-upper">{props.name}</span>
      </span>
    </div>
  )
}