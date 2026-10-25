// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html, nothing} from '//resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

import {displayHost, faviconUrl, haptic, SidebarProxy} from './browser_proxy.js';
import {DragController} from './drag.js';
import type {DragHost, DragSource, DropTarget} from './drag.js';
import {Haptic} from './paper.mojom-webui.js';
import type {BookmarkNode, SidebarPageHandlerRemote, SidebarState} from './paper.mojom-webui.js';

interface MenuItem {
  label: string;
  action: () => void;
  danger?: boolean;
}

interface Menu {
  x: number;
  y: number;
  items: MenuItem[];
}

const EXPANDED_KEY = 'paper.expandedFolders';
// Staggered sweep before Clear actually closes tabs, so you see them go.
const SWEEP_MS = 240;

function loadExpanded(): Set<string> {
  try {
    return new Set(JSON.parse(localStorage.getItem(EXPANDED_KEY) ?? '[]'));
  } catch {
    return new Set();
  }
}

function saveExpanded(ids: Set<string>) {
  try {
    localStorage.setItem(EXPANDED_KEY, JSON.stringify([...ids]));
  } catch {
    // Storage is a convenience; folders just start collapsed.
  }
}

function isFolder(node: BookmarkNode): boolean {
  return !node.url;
}

export class PaperSidebarAppElement extends CrLitElement implements DragHost {
  static get is() {
    return 'paper-sidebar-app';
  }

  static override get properties() {
    return {
      state_: {type: Object},
      menu_: {type: Object},
      target_: {type: Object},
      renamingId_: {type: String},
      creatingIn_: {type: String},
      sweeping_: {type: Boolean},
      expanded_: {type: Object},
    };
  }

  protected accessor state_: SidebarState|null = null;
  protected accessor menu_: Menu|null = null;
  protected accessor target_: DropTarget|null = null;
  protected accessor renamingId_: string|null = null;
  // Folder id the "new folder" input is shown in ('' is the bookmark bar).
  protected accessor creatingIn_: string|null = null;
  protected accessor sweeping_: boolean = false;
  protected accessor expanded_: Set<string> = loadExpanded();

  private readonly proxy_ = SidebarProxy.get();
  private readonly drag_ = new DragController(this);

  private get handler_(): SidebarPageHandlerRemote {
    return this.proxy_.handler;
  }

  // Light DOM, so the drag ghost (cloned onto <body>) keeps its styles.
  override createRenderRoot() {
    return this;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.proxy_.router.stateChanged.addListener(
        (state: SidebarState) => this.state_ = state);
    this.handler_.getState().then(({state}) => this.state_ = state);
    window.addEventListener('pointerdown', this.onWindowPointerDown_);
    window.addEventListener('keydown', this.onKeyDown_);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    window.removeEventListener('pointerdown', this.onWindowPointerDown_);
    window.removeEventListener('keydown', this.onKeyDown_);
  }

  // Rendering -----------------------------------------------------------------

  override render() {
    const state = this.state_;
    if (!state) {
      return nothing;
    }
    return html`
      <section id="essentials" class="essentials"
          ?data-empty=${state.essentials.length === 0}>
        ${state.essentials.map((e, i) => html`
          <button class="essential" data-key="essential:${e.id}"
              data-index=${i} title=${e.title || displayHost(e.url.url)}
              ?data-active=${e.active} ?data-open=${e.open}
              @pointerdown=${(ev: PointerEvent) =>
                  this.beginDrag_(ev, 'essential', e.id)}
              @click=${() => this.handler_.openEssential(e.id)}
              @contextmenu=${(ev: MouseEvent) => this.openMenu_(ev, [
                {label: 'Open', action: () => this.handler_.openEssential(e.id)},
                {
                  label: 'Remove from Essentials',
                  action: () => this.handler_.removeEssential(e.id),
                  danger: true,
                },
              ])}>
            <img src=${faviconUrl(e.url.url, 32)} alt="" draggable="false">
          </button>`)}
        ${state.essentials.length === 0 ? html`
          <div class="essentials-hint">Drag a tab here to keep it close</div>` :
          nothing}
      </section>

      <section id="bookmarks" class="bookmarks"
          @contextmenu=${(ev: MouseEvent) => this.openMenu_(ev, [
            {label: 'New folder', action: () => this.startCreateFolder_('')},
          ])}>
        ${this.creatingIn_ === '' ? this.renderNameInput_('', 0) : nothing}
        ${this.renderBookmarks_(state.bookmarks, '', 0)}
      </section>

      <div class="divider">
        <div class="divider-line"></div>
        <button class="clear" ?data-armed=${state.clearArmed}
            ?hidden=${state.tabs.length === 0}
            @click=${this.onClear_}>
          ${state.clearArmed ? 'Clear all' : 'Clear'}
        </button>
      </div>

      <section id="tabs" class="tabs">
        ${state.tabs.map((t, i) => html`
          <div class="row tab" data-key="tab:${t.id}" data-index=${i}
              ?data-active=${t.active} ?data-loading=${t.loading}
              ?data-sweep=${this.sweeping_ && (!t.active || state.clearArmed)}
              style="--sweep-delay: ${i * 18}ms"
              @pointerdown=${(ev: PointerEvent) =>
                  this.beginDrag_(ev, 'tab', String(t.id))}
              @click=${() => this.handler_.activateTab(t.id)}
              @auxclick=${(ev: MouseEvent) => {
                if (ev.button === 1) this.handler_.closeTab(t.id);
              }}
              @contextmenu=${(ev: MouseEvent) => this.openMenu_(ev, [
                {
                  label: 'Add to Essentials',
                  action: () => this.handler_.addEssentialFromTab(
                      t.id, state.essentials.length),
                },
                {
                  label: 'Bookmark',
                  action: () => this.handler_.bookmarkTab(
                      t.id, 0n, state.bookmarks.length),
                },
                {
                  label: 'Close',
                  action: () => this.handler_.closeTab(t.id),
                  danger: true,
                },
              ])}>
            <img class="favicon" src=${faviconUrl(t.url.url, 16)} alt=""
                draggable="false">
            <span class="title">${t.title || displayHost(t.url.url)}</span>
            <button class="close" title="Close"
                @pointerdown=${(ev: PointerEvent) => ev.stopPropagation()}
                @click=${(ev: MouseEvent) => {
                  ev.stopPropagation();
                  this.handler_.closeTab(t.id);
                }}>
              <svg viewBox="0 0 12 12"><path d="M3 3l6 6M9 3l-6 6"/></svg>
            </button>
          </div>`)}
        <div class="row new-tab" @click=${() => this.handler_.newTab()}>
          <span class="plus">
            <svg viewBox="0 0 16 16"><path d="M8 3v10M3 8h10"/></svg>
          </span>
          <span class="title">New Tab</span>
        </div>
      </section>

      ${this.renderIndicator_()}
      ${this.renderMenu_()}
    `;
  }

  private renderBookmarks_(nodes: BookmarkNode[], parentId: string,
                           depth: number): TemplateResult[] {
    return nodes.map((node, index) => {
      const id = String(node.id);
      const folder = isFolder(node);
      const expanded = folder && this.expanded_.has(id);
      const row = html`
        <div class="row bm-row" data-key="bookmark:${id}" data-id=${id}
            data-parent=${parentId} data-index=${index}
            data-depth=${depth} ?data-folder=${folder}
            ?data-expanded=${expanded}
            style="--depth: ${depth}"
            @pointerdown=${(ev: PointerEvent) =>
                this.beginDrag_(ev, folder ? 'folder' : 'bookmark', id)}
            @click=${() => folder ? this.toggleFolder_(id) :
                                    this.handler_.openBookmark(node.id)}
            @contextmenu=${(ev: MouseEvent) => this.openMenu_(ev, [
              ...(folder ? [{
                label: 'New folder inside',
                action: () => this.startCreateFolder_(id),
              }] : [{
                label: 'Open',
                action: () => this.handler_.openBookmark(node.id),
              }]),
              {label: 'Rename', action: () => this.renamingId_ = id},
              {
                label: 'Delete',
                action: () => this.handler_.removeBookmark(node.id),
                danger: true,
              },
            ])}>
          ${folder ? html`
            <span class="folder-icon">
              <svg viewBox="0 0 18 18">
                <path d="M2.5 5.5a2 2 0 0 1 2-2h3l1.6 1.8h4.4a2 2 0 0 1 2 2v5.7a2 2 0 0 1-2 2h-9a2 2 0 0 1-2-2z"/>
              </svg>
            </span>` : html`
            <img class="favicon" src=${faviconUrl(node.url!.url, 16)} alt=""
                draggable="false">`}
          ${this.renamingId_ === id ?
              this.renderRenameInput_(node) :
              html`<span class="title">${
                  node.title || displayHost(node.url?.url ?? '')}</span>`}
          ${folder ? html`
            <span class="chevron">
              <svg viewBox="0 0 12 12"><path d="M3.5 4.5L6 7l2.5-2.5"/></svg>
            </span>` : nothing}
        </div>`;
      if (!folder) {
        return row;
      }
      return html`
        ${row}
        <div class="bm-children" ?data-expanded=${expanded}>
          <div class="bm-children-inner">
            ${this.creatingIn_ === id ?
                this.renderNameInput_(id, depth + 1) : nothing}
            ${expanded ? this.renderBookmarks_(node.children, id, depth + 1) :
                         nothing}
          </div>
        </div>`;
    });
  }

  private renderRenameInput_(node: BookmarkNode) {
    return html`
      <input class="inline-input" .value=${node.title}
          @pointerdown=${(ev: PointerEvent) => ev.stopPropagation()}
          @click=${(ev: MouseEvent) => ev.stopPropagation()}
          @keydown=${(ev: KeyboardEvent) => {
            const input = ev.target as HTMLInputElement;
            if (ev.key === 'Enter') {
              this.handler_.renameBookmark(node.id, input.value.trim() ||
                                                        node.title);
              this.renamingId_ = null;
            } else if (ev.key === 'Escape') {
              this.renamingId_ = null;
            }
          }}
          @blur=${() => this.renamingId_ = null}>`;
  }

  private renderNameInput_(parentId: string, depth: number) {
    return html`
      <div class="row bm-row creating" style="--depth: ${depth}">
        <span class="folder-icon">
          <svg viewBox="0 0 18 18">
            <path d="M2.5 5.5a2 2 0 0 1 2-2h3l1.6 1.8h4.4a2 2 0 0 1 2 2v5.7a2 2 0 0 1-2 2h-9a2 2 0 0 1-2-2z"/>
          </svg>
        </span>
        <input class="inline-input" placeholder="Folder name"
            @keydown=${(ev: KeyboardEvent) => {
              const input = ev.target as HTMLInputElement;
              if (ev.key === 'Enter' && input.value.trim()) {
                this.handler_.addBookmarkFolder(
                    parentId ? BigInt(parentId) : 0n, input.value.trim());
                this.creatingIn_ = null;
              } else if (ev.key === 'Escape') {
                this.creatingIn_ = null;
              }
            }}
            @blur=${() => this.creatingIn_ = null}>
      </div>`;
  }

  private renderIndicator_() {
    const line = this.target_?.line;
    if (!line) {
      return nothing;
    }
    return html`<div class="drop-line" style="transform: translate(${
        line.left}px, ${line.top}px); width: ${line.width}px; height: ${
        line.height}px"></div>`;
  }

  private renderMenu_() {
    const menu = this.menu_;
    if (!menu) {
      return nothing;
    }
    return html`
      <div class="menu" style="left: ${menu.x}px; top: ${menu.y}px"
          @pointerdown=${(ev: PointerEvent) => ev.stopPropagation()}>
        ${menu.items.map(item => html`
          <button class="menu-item" ?data-danger=${item.danger}
              @click=${() => {
                this.menu_ = null;
                item.action();
              }}>${item.label}</button>`)}
      </div>`;
  }

  override updated() {
    // Folder rows and essentials light up when they're the drop target.
    for (const el of this.querySelectorAll('[data-drop-hover]')) {
      el.removeAttribute('data-drop-hover');
    }
    const key = this.target_?.highlightKey;
    if (key) {
      this.querySelector(`[data-key="${CSS.escape(key)}"]`)
          ?.setAttribute('data-drop-hover', '');
    }
    this.querySelector<HTMLInputElement>('.inline-input')?.focus();
  }

  // Interaction ---------------------------------------------------------------

  private onClear_ = () => {
    // Let the rows sweep out, then close for real.
    this.sweeping_ = true;
    setTimeout(() => {
      this.sweeping_ = false;
      this.handler_.clearTabs();
    }, SWEEP_MS);
  };

  private toggleFolder_(id: string) {
    const expanded = new Set(this.expanded_);
    if (expanded.has(id)) {
      expanded.delete(id);
    } else {
      expanded.add(id);
    }
    this.expanded_ = expanded;
    saveExpanded(expanded);
  }

  private startCreateFolder_(parentId: string) {
    if (parentId && !this.expanded_.has(parentId)) {
      this.toggleFolder_(parentId);
    }
    this.creatingIn_ = parentId;
  }

  private openMenu_(ev: MouseEvent, items: MenuItem[]) {
    ev.preventDefault();
    ev.stopPropagation();
    // Keep the menu inside the sidebar.
    const x = Math.min(ev.clientX, window.innerWidth - 196);
    const y = Math.min(ev.clientY, window.innerHeight - items.length * 32 - 16);
    this.menu_ = {x, y, items};
  }

  private onWindowPointerDown_ = () => {
    this.menu_ = null;
  };

  private onKeyDown_ = (ev: KeyboardEvent) => {
    if (ev.key === 'Escape') {
      this.menu_ = null;
    }
  };

  private beginDrag_(ev: PointerEvent, kind: DragSource['kind'], id: string) {
    if (this.renamingId_ || ev.button !== 0) {
      return;
    }
    this.drag_.begin(ev, {kind, id, el: ev.currentTarget as HTMLElement});
  }

  // DragHost ------------------------------------------------------------------

  haptic(pattern: Haptic) {
    haptic(pattern, p => this.handler_.performHaptic(p));
  }

  showIndicator(target: DropTarget|null) {
    this.target_ = target;
  }

  resolveTarget(source: DragSource, x: number, y: number): DropTarget|null {
    const within = (el: Element|null, slop = 0) => {
      if (!el) {
        return false;
      }
      const r = el.getBoundingClientRect();
      return x >= r.left - slop && x <= r.right + slop && y >= r.top - slop &&
          y <= r.bottom + slop;
    };

    const essentials = this.querySelector('#essentials');
    if ((source.kind === 'tab' || source.kind === 'essential') &&
        within(essentials, 10)) {
      return this.resolveEssentials_(source, x, y);
    }

    const bookmarks = this.querySelector('#bookmarks');
    if (source.kind !== 'essential' && within(bookmarks, 4)) {
      return this.resolveBookmarks_(source, y);
    }

    const tabs = this.querySelector('#tabs');
    if ((source.kind === 'tab' || source.kind === 'essential') &&
        within(tabs, 12)) {
      return this.resolveTabs_(source, y);
    }
    return null;
  }

  private resolveEssentials_(source: DragSource, x: number, y: number):
      DropTarget {
    const tiles = [...this.querySelectorAll<HTMLElement>('.essential')].filter(
        el => el !== source.el);
    // Reading-order insertion point.
    let index = 0;
    for (const tile of tiles) {
      const r = tile.getBoundingClientRect();
      if (y > r.bottom || (y >= r.top && x > r.left + r.width / 2)) {
        index++;
      }
    }
    let line;
    const anchor = tiles[index] ?? tiles[tiles.length - 1];
    if (anchor) {
      const r = anchor.getBoundingClientRect();
      const before = index < tiles.length;
      line = {
        left: (before ? r.left : r.right) - 2,
        top: r.top + 6,
        width: 3,
        height: r.height - 12,
      };
    }
    return {zone: 'essentials', index, line};
  }

  private resolveTabs_(source: DragSource, y: number): DropTarget {
    const rows = [...this.querySelectorAll<HTMLElement>('.tab')].filter(
        el => el !== source.el);
    let index = 0;
    for (const row of rows) {
      const r = row.getBoundingClientRect();
      if (y > r.top + r.height / 2) {
        index++;
      }
    }
    return {zone: 'tabs', index, line: this.lineBetween_(rows, index)};
  }

  private resolveBookmarks_(source: DragSource, y: number): DropTarget {
    // Can't drop a folder into itself or anything inside it.
    const rows = [...this.querySelectorAll<HTMLElement>('.bm-row[data-id]')]
                     .filter(el => {
                       if (el === source.el) {
                         return false;
                       }
                       if (source.kind !== 'folder') {
                         return true;
                       }
                       const children = source.el.nextElementSibling;
                       return !children?.contains(el);
                     });
    const row = rows.find(el => {
      const r = el.getBoundingClientRect();
      return y >= r.top && y <= r.bottom;
    });

    if (!row) {
      const count = this.state_?.bookmarks.length ?? 0;
      const last = rows.filter(el => el.dataset['depth'] === '0').pop();
      const r = last?.getBoundingClientRect();
      return {
        zone: 'bookmarks',
        parentId: '',
        index: count,
        line: r ? {left: r.left + 8, top: r.bottom - 1, width: r.width - 16,
                   height: 2} : undefined,
      };
    }

    const r = row.getBoundingClientRect();
    const relative = (y - r.top) / r.height;
    if (row.hasAttribute('data-folder') && relative > 0.25 &&
        relative < 0.75) {
      return {
        zone: 'bookmarks',
        parentId: row.dataset['id'],
        index: Number.MAX_SAFE_INTEGER,
        into: true,
        highlightKey: row.dataset['key'],
      };
    }
    const after = relative >= 0.5;
    const depth = Number(row.dataset['depth']);
    return {
      zone: 'bookmarks',
      parentId: row.dataset['parent'],
      index: Number(row.dataset['index']) + (after ? 1 : 0),
      line: {
        left: r.left + 10 + depth * 14,
        top: (after ? r.bottom : r.top) - 1,
        width: r.width - 20 - depth * 14,
        height: 2,
      },
    };
  }

  private lineBetween_(rows: HTMLElement[], index: number) {
    const anchor = rows[index] ?? rows[rows.length - 1];
    if (!anchor) {
      return undefined;
    }
    const r = anchor.getBoundingClientRect();
    const top = index < rows.length ? r.top : r.bottom;
    return {left: r.left + 10, top: top - 1, width: r.width - 20, height: 2};
  }

  drop(source: DragSource, target: DropTarget) {
    const h = this.handler_;
    const parent = target.parentId ? BigInt(target.parentId) : 0n;
    // Mojo clamps; MAX_SAFE_INTEGER means "append".
    const index = Math.min(target.index, 0x7fffffff);

    switch (target.zone) {
      case 'essentials':
        if (source.kind === 'tab') {
          h.addEssentialFromTab(Number(source.id), index);
        } else if (source.kind === 'essential') {
          h.moveEssential(source.id, index);
        }
        break;
      case 'tabs':
        if (source.kind === 'tab') {
          h.moveTab(Number(source.id), index);
        } else if (source.kind === 'essential') {
          h.removeEssential(source.id);
        }
        break;
      case 'bookmarks':
        if (source.kind === 'tab') {
          h.bookmarkTab(Number(source.id), parent, index);
        } else {
          h.moveBookmark(BigInt(source.id), parent, index);
        }
        if (target.into && target.parentId &&
            !this.expanded_.has(target.parentId)) {
          this.toggleFolder_(target.parentId);
        }
        break;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'paper-sidebar-app': PaperSidebarAppElement;
  }
}

customElements.define(PaperSidebarAppElement.is, PaperSidebarAppElement);
