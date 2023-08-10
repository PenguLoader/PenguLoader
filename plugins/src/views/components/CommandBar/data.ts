import type { Action } from './types';

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
    },
    {
      name: 'Create 5v5 SR lobby',
    },
  ],
  uncategorized: [
    {
      name: 'Test 1',
    },
    {
      name: 'Test 2',
    },
    {
      name: 'Test 3',
    },
  ]
};

export const DEFAULT_ACTIONS: Action[] = Object.keys(ACTIONS)
  .flatMap(k => ACTIONS[k].map(v => ({ ...v, group: k })));