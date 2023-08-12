
export interface Action {
  id?: string
  name: string
  legend?: string
  tags?: string[]
  icon?: string
  group?: string
  hidden?: boolean
  perform?: (id?: string) => any
}

export enum VisualState {
  Hidden = 0,
  AnimatingIn,
  Showing,
  AnimatingOut,
}