import { t } from '../lib/i18n';
import { getUpdate } from '../lib/updater';
import Modal, { ModalContent, ModalFooter, ModalHeader } from './Modal';

export default function ModalUpdate() {

  const update = getUpdate()!;

  return (
    <Modal>
      <ModalHeader>
        <h2 class="modal__title">League Loader update</h2>
      </ModalHeader>
      <ModalContent>
        <p>
          <span innerText={t('has_new_ver')} /> 👉 <strong>{update.version}</strong>
        </p>
        <div class="scroll-view">
          <div innerHTML={update.changelog} />
        </div>
      </ModalContent>
      <ModalFooter>
        <button class="modal__btn modal__btn-primary" data-micromodal-close>OKAY</button>
        <span style="max-width: 50%">
          <small innerText={t('update_tip')} />
        </span>
      </ModalFooter>
    </Modal>
  )
}