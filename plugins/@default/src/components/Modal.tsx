import { onMount, Show } from 'solid-js';
import { nanoid } from 'nanoid';
import MicroModal from 'micromodal';

interface Props {
  onClose?: () => void;
  children: any;
}

export default function Modal(props: Props) {
  const id = nanoid();
  
  onMount(() => {
    MicroModal.show(id, {
      openTrigger: 'data-custom-open',
      onClose: () => props.onClose?.call(null),
      openClass: 'is-open',
      disableFocus: true,
      awaitOpenAnimation: true,
      awaitCloseAnimation: true,
      debugMode: import.meta.env.DEV
    });
  });

  return (
    <div class="micromodal-slide modal" id={id}>
      <div class="modal__overlay" tabIndex={-1}>
        <div class="modal__container" role="dialog" aria-modal="true">
          {props.children}
        </div>
      </div>
    </div>
  )
}

export function ModalHeader(props: any) {
  return (
    <header class="modal__header">
      {props.children}
    </header>
  )
}

export function ModalContent(props: any) {
  return (
    <div class="modal__content">
      {props.children}
    </div>
  )
}

export function ModalFooter(props: any) {
  return (
    <footer class="modal__footer">
      {props.children}
    </footer>
  )
}