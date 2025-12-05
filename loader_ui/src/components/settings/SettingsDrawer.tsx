import { Component, JSXElement, VoidComponent } from 'solid-js'
import { Portal } from 'solid-js/web'

type SettingsDrawerProps = {
  title: string
  icon: JSXElement
}

export const SettingsDrawer: Component<SettingsDrawerProps> = (props) => {
  return (
    <Portal mount={document.getElementById('pengu-settings-drawer')}>
      <>
        <div class="settings-header-title">
          <div class="settings-header-title-icon">
            <img class="settings-header-title-icon-image" src="data:image/svg+xml;base64,PHN2ZyB3aWR0aD0iNDgiIGhlaWdodD0iNDgiIHZpZXdCb3g9IjAgMCA0OCA0OCIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTI0IDQ4QzM3LjI1NDggNDggNDggMzcuMjU0OCA0OCAyNEM0OCAxMC43NDUyIDM3LjI1NDggMCAyNCAwQzEwLjc0NTIgMCAwIDEwLjc0NTIgMCAyNEMwIDM3LjI1NDggMTAuNzQ1MiA0OCAyNCA0OFoiIGZpbGw9IiNFQjAwMjkiLz4KPHBhdGggZD0iTTI0LjAxODcgMTJMMTAgMTguNDkyTDEzLjQ5MyAzMS43ODU0TDE2LjE1MTQgMzEuNDU5TDE1LjQyMDQgMjMuMTAwNUwxNi4yOTMzIDIyLjcxMTZMMTcuODAwNiAzMS4yNTZMMjIuMzQzOSAzMC42OTgyTDIxLjUzNjMgMjEuNDcyNkwyMi40MDA2IDIxLjA4NzlMMjQuMDU4NCAzMC40ODgxTDI4LjY1NDIgMjkuOTIzMkwyNy43NyAxOS44MTA1TDI4LjY0NDMgMTkuNDIxN0wzMC40NTY3IDI5LjcwMThMMzUgMjkuMTQ0VjE0Ljc1MDdMMjQuMDE4NyAxMloiIGZpbGw9IiNGMUY1RjEiLz4KPHBhdGggZD0iTTI0LjM0OCAzMi4xNjE1TDI0LjU3OTMgMzMuNDcwMUwzNSAzNS4yMDc0VjMwLjg1MjlMMjQuMzUzNiAzMi4xNjAxTDI0LjM0OCAzMi4xNjE1WiIgZmlsbD0iI0YxRjVGMSIvPgo8L3N2Zz4K" />
          </div>
          <div class="settings-header-title-text">
            <span class="text formatted-message" data-family="riot-sans" data-bold="false" data-scale="HeadlineL" data-testid="text">{props.title}</span>
          </div>
        </div>
        <div>
          <span class="text formatted-message social-settings-section-title" data-family="sans" data-bold="false" data-scale="LabelXS" data-testid="text">
            <span class="formatted-message">CÀI ĐẶT CHAT</span>
          </span>
          <div class="social-settings-chat-settings">
            <div class="social-settings-chat-settings-text">
              <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="LabelM" data-testid="text">
                <span class="formatted-message">Bật tính năng lọc ngôn từ</span>
              </span>
              <span class="text formatted-message social-settings-chat-settings-subtitle" data-family="sans" data-bold="false" data-scale="BodyS" data-testid="text">
                <span class="formatted-message">Ẩn từ ngữ thô tục trong tin nhắn chat.</span>
              </span>
            </div>
            <label class="toggle" data-testid="social-language-filter-toggle" data-checked="false">
              <input type="checkbox" class="toggle-input" data-testid="toggle-input" id="social-language-filter" checked="" />
              <div class="toggle-slide-background"></div><div class="toggle-slide"><svg width="16" height="16" viewBox="0 0 16 16" fill="currentColor" xmlns="http://www.w3.org/2000/svg" role="img" class="icon display-block toggle-icon" data-testid="icon" data-icon="close">
                <path d="M12.473 2.193h-.004a.665.665 0 00-.938 0h-.004L8 5.72 4.473 2.193H4.47a.665.665 0 00-.938 0h-.004L2.193 3.527v.004a.664.664 0 000 .938v.004L5.72 8l-3.527 3.527v.004a.665.665 0 000 .938v.004l1.334 1.334h.004a.665.665 0 00.938 0h.004L8 10.28l3.527 3.527h.004a.665.665 0 00.938 0h.004l1.334-1.334v-.004a.664.664 0 000-.938v-.004L10.28 8l3.527-3.527V4.47a.664.664 0 000-.938v-.004l-1.334-1.334z"></path>
              </svg>
              </div>
            </label>
          </div>
          <span class="text formatted-message social-settings-section-title" data-family="sans" data-bold="false" data-scale="LabelXS" data-testid="text">
            <span class="formatted-message">NGƯỜI CHƠI ĐÃ CHẶN</span>
          </span>
          <div class="social-settings-block-list-empty-state">
            <svg width="32" height="32" viewBox="0 0 32 32" fill="none" xmlns="http://www.w3.org/2000/svg" role="img" class="icon display-block" data-testid="icon" data-icon="blocked-user">
              <path d="M22 7c0-4-.04-6-6.09-6-4.88 0-5.75 1.35-5.91 4.04l7.91 7.87C22 12.48 22 10.51 22 7zM2 1l-.05 4.02L12 15H4L2 25s6 4 14 4c3.28 0 6.92-.67 9.25-1.46L26.71 29H30L2 1zM28 15h-8.71L30 25l-2-10z" fill="currentColor"></path>
            </svg>
            <span class="text formatted-message" data-family="sans" data-bold="false" data-scale="LabelM" data-testid="text">
              <span class="formatted-message">Không có người chơi bị chặn nào ở đây.</span>
            </span>
          </div>
        </div>
      </>
    </Portal>
  )
}