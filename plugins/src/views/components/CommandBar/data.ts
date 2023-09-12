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
      name: 'Visit homepage',
      legend: 'pengu.lol',
      perform: () => window.open('https://pengu.lol', '_blank')
    },
    {
      name: 'Open DevTools',
      legend: 'F12',
      tags: ['dev', 'console'],
      perform: () => window.openDevTools?.()
    },
    {
      name: 'Open plugins folder',
      tags: ['dev'],
      perform: () => window.openPluginsFolder?.()
    },
    {
      name: 'Reload Client',
      legend: 'Ctrl Shift R',
      hidden: true,
      perform: () => window.reloadClient?.()
    },
    {
      name: 'Restart Client',
      legend: 'Ctrl Shift Enter',
      hidden: true,
      perform: () => window.restartClient?.()
    }
  ],
  lobby: [
    {
      name: 'Create ARAM lobby',
      perform: () => createLobby(QueueId.ARAM)
    },
    {
      name: 'Create 5v5 SR lobby',
      perform: () => createLobby(QueueId.BlindPick)
    },
    {
      name: 'Create Practice Tool',
      perform: () => createLobby(QueueId.PracticeTool)
    },
  ],
  uncategorized: [
    {
      name: 'Quit PvP champ-select',
      hidden: true,
      perform: () => quitPvPChampSelect()
    }
  ]
};

export const DEFAULT_ACTIONS: Action[] = Object.keys(ACTIONS)
  .flatMap(k => ACTIONS[k].map(v => ({ ...v, group: k })));