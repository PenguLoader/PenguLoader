import { Component } from 'solid-js'
import { Button } from '../ui/Button'
import { DiscordIcon, GitHubIcon, LinkIcon } from '../Icons'
import { shell } from '@tauri-apps/api'

const links = {
  home: 'https://pengu.lol',
  github: 'https://github.com/PenguLoader/PenguLoader/',
  discord: 'https://chat.pengu.lol/'
}

export const TabAbout: Component = () => {
  return (
    <div>
      <div class="flex flex-col space-y-4">
        <p class="text-base font-semibold leading-none text-neutral-300">Pengu Loader v1.2.0</p>
        <div class="flex items-center space-x-4 text-neutral-200">
          <Button variant="outline" size="sm" class="flex items-center gap-x-2" onClick={() => shell.open(links.home)}><LinkIcon size={16} />pengu.lol</Button>
          <Button variant="outline" size="sm" class="flex items-center gap-x-2" onClick={() => shell.open(links.discord)}><DiscordIcon size={16} /> Discord</Button>
          <Button variant="outline" size="sm" class="flex items-center gap-x-2" onClick={() => shell.open(links.github)}><GitHubIcon size={16} /> GitHub</Button>
        </div>
      </div>
    </div>
  )
}