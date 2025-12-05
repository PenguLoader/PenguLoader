import { mergeProps, VoidComponent } from "solid-js"

type DirectorySettingProps = {
  path: string
  disabled?: boolean
  onClick?: () => void
}

export const DirectorySetting: VoidComponent<DirectorySettingProps> = (props) => {
  const merged = mergeProps({ disabled: false }, props)
  return (
    <button
      type="button"
      onClick={merged.onClick}
      class="text formatted-message folder-picker-input-field"
      data-placeholder="false"
      data-testid="folder-picker-input-field"
      title={merged.path}
      disabled={merged.disabled}
      data-family="sans"
      data-bold="false"
      data-scale="BodyM"
    >
      <div class="path-ellipsis folder-picker-input-path-ellipsis" data-testid="path-ellipsis" data-settled="true">{merged.path}</div>
      <div class="icon-folder"></div>
    </button>
  )
}