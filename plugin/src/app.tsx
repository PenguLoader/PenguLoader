import { useEffect, useState } from 'preact/hooks';
import MicroModal from 'micromodal';
import { nanoid } from 'nanoid';
import { DISCORD_URL, GITHUB_REPO_URL } from './lib/constants';
import { getSummonerName } from './lib/api';
import { t } from './lib/i18n';
import { isDarkMode } from './lib/theme';
import logo from './assets/logo.png';

export function App() {

  const modalId = nanoid();
  const checkboxId = nanoid();
  const version = window.__llver || '0.0.0';

  const [summoner, setSummoner] = useState('Summoner');
  const [showAgain, setShowAgain] = useState(window.DataStore?.get('ll-welcome') ?? true);

  const onNotShowClick = () => {
    const value = !showAgain;
    setShowAgain(value);
    window.DataStore?.set('ll-welcome', value);
  };

  useEffect(() => {
    getSummonerName().then(setSummoner);
    if (showAgain) {
      MicroModal.show(modalId, {
        openTrigger: 'data-custom-open',
        openClass: 'is-open',
        disableFocus: true,
        awaitOpenAnimation: true,
        awaitCloseAnimation: true,
        debugMode: import.meta.env.DEV
      });
    }
  }, []);

  return (
    <div class="micromodal-slide modal dark" dark-mode={isDarkMode()} id={modalId}>
      <div class="modal__overlay" tabIndex={-1}>
        <div class="modal__container" role="dialog" aria-modal="true">
          <header class="modal__header">
            <img src={logo} width="28" />
            <h2 class="modal__title">League Loader</h2>
            <span class="version">v{version}</span>
          </header>
          <div class="modal__content">
            <p dangerouslySetInnerHTML={{ __html: t('welcome', { summoner: `<strong>${summoner}</strong>` }) }}></p>
            <p dangerouslySetInnerHTML={{
              __html: t('checkout_links', {
                discord: `<a href=${DISCORD_URL} target="_blank">Discord</a>`,
                github: `<a href=${GITHUB_REPO_URL} target="_blank">GitHub</a>`
              })
            }}>
            </p>
            <p>
              <small dangerouslySetInnerHTML={{ __html: t('devtools_tip', { key: '<code>F12</code>' }) }} ></small>
            </p>
          </div>
          <footer class="modal__footer">
            <label for={checkboxId} style="margin-left: .4rem">
              <input type="checkbox" checked={!showAgain} id={checkboxId}
                onClick={onNotShowClick} /><span dangerouslySetInnerHTML={{ __html: t('donotshowagain') }} ></span>
            </label>
            <button class="modal__btn modal__btn-primary" data-micromodal-close>OKAY</button>
          </footer>
        </div>
      </div>
    </div >
  )
}