import { createSignal, onMount, Show } from 'solid-js';
import { fetchUpdate } from './lib/updater';
import ModalUpdate from './components/ModalUpdate';
import ModalWelcome from './components/ModalWelcome';

export default function App() {
  
  const [welcomeVisible, setWelcomeVisible] = createSignal<boolean>(window.DataStore?.get('ll-welcome') ?? true);
  const [updateVisible, setUpdateVisible] = createSignal(false);
  const [updateAvailable, setUpdateAvailable] =  createSignal(false);

  const showUpdate = () => {
    setWelcomeVisible(false);
    setUpdateVisible(true);
  };

  onMount(() => {
    fetchUpdate().then(setUpdateAvailable);
  });

  return (
    <>
      <Show when={welcomeVisible()}>
        <ModalWelcome onClose={showUpdate} />
      </Show>
      <Show when={updateVisible() && updateAvailable()}>
        <ModalUpdate />
      </Show>
    </>
  )
}