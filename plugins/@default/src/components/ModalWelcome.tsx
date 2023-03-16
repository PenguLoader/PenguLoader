import { nanoid } from 'nanoid';
import { createSignal, onMount } from 'solid-js';
import { DISCORD_URL, GITHUB_REPO_URL } from '../lib/constants';
import Modal, { ModalContent, ModalHeader, ModalFooter } from './Modal';
import { t } from '../lib/i18n';
import { getSummonerName } from '../lib/api';
import PenguImg from '../assets/PenguLoader.jpg';

interface Props {
  onClose(): void;
}

export default function ModalWelcome(props: Props) {

  const checkboxId = nanoid();
  const version = window.__llver || '0.0.0';

  const visible = window.DataStore?.get('ll-welcome') ?? true;
  const [summoner, setSummoner] = createSignal('Summoner');
  const [showAgain, setShowAgain] = createSignal<boolean>(visible);

  const onNotShowClick = () => {
    const value = !showAgain();
    setShowAgain(value);
    window.DataStore?.set('ll-welcome', value);
  };

  onMount(() => {
    getSummonerName().then(setSummoner);
  });

  return (
    <Modal onClose={props.onClose}>
      <ModalHeader>
        <img src={PenguImg} width="48" style="border-radius: 4px" />
        <h2 class="modal__title">Pengu Loader</h2>
        <span class="version">v{version}</span>
      </ModalHeader>
      <ModalContent>
        <p innerHTML={t('welcome', { name: `<strong>${summoner()}</strong>` })} />
        <p innerHTML={t('checkout_links', {
          discord: `<a href=${DISCORD_URL} target="_blank">Discord</a>`,
          github: `<a href=${GITHUB_REPO_URL} target="_blank">GitHub</a>`
        })} />
        <p>
          <small innerHTML={t('devtools_tip', { key: '<code>Ctrl Shift I</code>' })} />
        </p>
      </ModalContent>
      <ModalFooter>
        <button class="modal__btn modal__btn-primary" data-micromodal-close>OKAY</button>
        <label for={checkboxId} style="margin-left: .4rem">
          <input type="checkbox" checked={!showAgain()} id={checkboxId}
            onClick={onNotShowClick} /><span innerHTML={t('donotshowagain')} />
        </label>
      </ModalFooter>
    </Modal>
  )
}