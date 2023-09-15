import { penguI18n } from "../../../i18n/i18n";

enum QueueId {
  DraftPick = 400,
  SoloDuo = 420,
  BlindPick = 430,
  Flex = 440,
  ARAM = 450,
  Clash = 700,
  IntroBots = 830,
  BeginnerBots = 840,
  IntermediateBots = 850,
  ARURF = 900,
  TFTNormal = 1090,
  TFTRanked = 1100,
  TFTTutorial = 1110,
  TFTHyperRoll = 1130,
  TFTDoubleUp = 1160,
  NexusBlitz = 1300,
  Tutorial1 = 2000,
  Tutorial2 = 2010,
  Tutorial3 = 2020,

  PracticeTool = 0xFFFF,
}

async function createLobby(queueId: QueueId) {
  let body: any = { queueId };
  if (queueId === QueueId.PracticeTool) {
    body = {
      customGameLobby: {
        configuration: {
          gameMode: 'PRACTICETOOL',
          gameMutator: '',
          gameServerRegion: '',
          mapId: 11,
          mutators: { id: 1 },
          spectatorPolicy: 'AllAllowed',
          teamSize: 5
        },
        lobbyName: 'Game ' + Math.floor(Math.random() * 0xFFFFFFFF).toString(36),
        lobbyPassword: null
      },
      isCustom: true
    }
  }

  await fetch('/lol-lobby/v2/lobby', {
    method: 'POST',
    body: JSON.stringify(body),
    headers: {
      'Content-Type': 'application/json'
    }
  });
}

async function quitPvPChampSelect() {
  const params = new URLSearchParams({
    destination: 'lcdsServiceProxy',
    method: 'call',
    args: JSON.stringify(['', 'teambuilder-draft', 'quitV2', '']),
  });
  const url = '/lol-login/v1/session/invoke?' + params.toString();
  await fetch(url, { method: 'POST' });
}

const ACTIONS: Record<string, Action[]> = {
  pengu: [
    {
      name: penguI18n("commandbar.builtin_commands.pengu_visit_homepage"),
      legend: 'pengu.lol',
      perform: () => window.open('https://pengu.lol', '_blank')
    },
    {
      name: penguI18n("commandbar.builtin_commands.pengu_open_devtools"),
      legend: 'F12',
      tags: ['dev', 'console'],
      perform: () => window.openDevTools?.()
    },
    {
      name: penguI18n("commandbar.builtin_commands.pengu_open_plugins_folder"),
      tags: ['dev'],
      perform: () => window.openPluginsFolder?.()
    },
    {
      name: penguI18n("commandbar.builtin_commands.pengu_reload_client"),
      legend: 'Ctrl Shift R',
      hidden: true,
      perform: () => window.reloadClient?.()
    },
    {
      name: penguI18n("commandbar.builtin_commands.pengu_restart_client"),
      legend: 'Ctrl Shift Enter',
      hidden: true,
      perform: () => window.restartClient?.()
    }
  ],
  lobby: [
    {
      name: penguI18n("commandbar.builtin_commands.lobby_create_aram"),
      perform: () => createLobby(QueueId.ARAM)
    },
    {
      name: penguI18n("commandbar.builtin_commands.lobby_create_5v5_sr"),
      perform: () => createLobby(QueueId.BlindPick)
    },
    {
      name: penguI18n("commandbar.builtin_commands.lobby_create_practive_tool"),
      perform: () => createLobby(QueueId.PracticeTool)
    },
  ],
  uncategorized: [
    {
      name: penguI18n("commandbar.builtin_commands.quit_pvp_champ_select"),
      hidden: true,
      perform: () => quitPvPChampSelect()
    }
  ]
};

export const DEFAULT_ACTIONS: Action[] = Object.keys(ACTIONS)
  .flatMap(k => ACTIONS[k].map(v => ({ ...v, group: k })));