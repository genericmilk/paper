// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement, html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import {faviconUrl, LauncherProxy} from './browser_proxy.js';
import {LauncherMode, MatchKind} from './paper.mojom-webui.js';
import type {LauncherMatch} from './paper.mojom-webui.js';

// Rotated through on each open, so the empty box has a little personality.
const PROMPTS = [
  'Where to?',
  'Search or type a URL',
  'Find a tab, a site, anything',
  'Go somewhere nice',
  'What are we looking for?',
];

const SECTION_TITLES: Record<MatchKind, string> = {
  [MatchKind.kTab]: 'Open tabs',
  [MatchKind.kUrl]: 'Go to',
  [MatchKind.kHistory]: 'From your history',
  [MatchKind.kSearch]: 'Search',
};

// Stable hue per host, for the tile behind each favicon.
function hueFor(text: string): number {
  let hash = 0;
  for (let i = 0; i < text.length; i++) {
    hash = (hash * 31 + text.charCodeAt(i)) | 0;
  }
  return Math.abs(hash) % 360;
}

export class PaperLauncherAppElement extends CrLitElement {
  static get is() {
    return 'paper-launcher-app';
  }

  static override get properties() {
    return {
      mode_: {type: Number},
      query_: {type: String},
      matches_: {type: Array},
      selected_: {type: Number},
      prompt_: {type: String},
      opening_: {type: Boolean},
    };
  }

  protected accessor mode_: LauncherMode = LauncherMode.kNewTab;
  protected accessor query_: string = '';
  protected accessor matches_: LauncherMatch[] = [];
  protected accessor selected_: number = 0;
  protected accessor prompt_: string = PROMPTS[0]!;
  protected accessor opening_: boolean = false;
  private openCount_ = 0;

  private readonly proxy_ = LauncherProxy.get();
  // Enter pressed before results for the current text arrived.
  private enterPending_ = false;

  override createRenderRoot() {
    return this;
  }

  override connectedCallback() {
    super.connectedCallback();
    const router = this.proxy_.router;
    router.shown.addListener(
        (mode: LauncherMode, text: string) => this.onShown_(mode, text));
    router.resultsChanged.addListener(
        (query: string, matches: LauncherMatch[]) =>
            this.onResults_(query, matches));
  }

  override render() {
    return html`
      <div class="card" ?data-opening=${this.opening_}>
        <div class="input-row">
          <span class="spark" aria-hidden="true"></span>
          <input id="input" type="text" spellcheck="false" autocomplete="off"
              placeholder=${this.prompt_} .value=${this.query_}
              @input=${this.onInput_} @keydown=${this.onKeyDown_}>
          ${this.mode_ === LauncherMode.kEditCurrent ? html`
            <span class="mode-chip">this tab</span>` : nothing}
        </div>
        ${this.matches_.length ? html`
          <div class="results" role="listbox">
            ${this.renderResults_()}
          </div>` : nothing}
      </div>`;
  }

  private renderResults_() {
    let lastKind: MatchKind|null = null;
    return this.matches_.map((match, index) => {
      const header = match.kind !== lastKind ?
          html`<div class="section">${SECTION_TITLES[match.kind]}</div>` :
          nothing;
      lastKind = match.kind;
      const selected = index === this.selected_;
      return html`
        ${header}
        <div class="result" role="option" aria-selected=${selected}
            ?data-selected=${selected} data-kind=${match.kind}
            style="--hue: ${hueFor(match.subtitle || match.title)};
                   --i: ${index}"
            @pointermove=${() => this.selected_ = index}
            @click=${() => this.open_(index)}>
          <span class="icon">${this.renderIcon_(match)}</span>
          <span class="text">
            <span class="title">${match.title}</span>
            ${match.subtitle ? html`
              <span class="subtitle">${match.subtitle}</span>` : nothing}
          </span>
          ${match.kind === MatchKind.kTab ? html`
            <span class="hint switch">Switch</span>` : html`
            <span class="hint enter">↩</span>`}
        </div>`;
    });
  }

  private renderIcon_(match: LauncherMatch) {
    if (match.kind === MatchKind.kSearch) {
      return html`<svg viewBox="0 0 20 20"><circle cx="8.5" cy="8.5" r="5"/>
          <path d="M12.5 12.5L16 16"/></svg>`;
    }
    return html`<img src=${faviconUrl(match.url.url, 32)} alt=""
        draggable="false">`;
  }

  private onShown_(mode: LauncherMode, text: string) {
    this.mode_ = mode;
    this.query_ = text;
    this.selected_ = 0;
    this.opening_ = false;
    this.enterPending_ = false;
    this.openCount_++;
    this.prompt_ = PROMPTS[this.openCount_ % PROMPTS.length]!;
    this.proxy_.handler.query(text);
    this.updateComplete.then(() => {
      const input = this.querySelector<HTMLInputElement>('#input')!;
      input.focus();
      input.select();
      // Springy entrance, replayed on every open.
      this.querySelector('.card')?.animate(
          [
            {transform: 'translateY(-12px) scale(0.95)', opacity: 0},
            {transform: 'none', opacity: 1},
          ],
          {duration: 420, easing: 'cubic-bezier(0.2, 1.4, 0.35, 1)'});
    });
  }

  private onResults_(query: string, matches: LauncherMatch[]) {
    // Results for text the user has since changed are stale.
    if (query !== this.query_) {
      return;
    }
    this.matches_ = matches;
    this.selected_ = Math.min(this.selected_, Math.max(0, matches.length - 1));
    if (this.enterPending_ && matches.length) {
      this.enterPending_ = false;
      this.open_(0);
    }
  }

  private onInput_ = (e: Event) => {
    this.query_ = (e.target as HTMLInputElement).value;
    this.selected_ = 0;
    this.proxy_.handler.query(this.query_);
  };

  private onKeyDown_ = (e: KeyboardEvent) => {
    const count = this.matches_.length;
    switch (e.key) {
      case 'ArrowDown':
        e.preventDefault();
        if (count) {
          this.selected_ = (this.selected_ + 1) % count;
        }
        break;
      case 'ArrowUp':
        e.preventDefault();
        if (count) {
          this.selected_ = (this.selected_ - 1 + count) % count;
        }
        break;
      case 'Enter':
        e.preventDefault();
        if (count) {
          this.open_(this.selected_);
        } else if (this.query_.trim()) {
          this.enterPending_ = true;
        }
        break;
    }
  };

  private open_(index: number) {
    // A quick squish before the native side hides the window.
    this.opening_ = true;
    setTimeout(() => this.proxy_.handler.openMatch(index), 90);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'paper-launcher-app': PaperLauncherAppElement;
  }
}

customElements.define(PaperLauncherAppElement.is, PaperLauncherAppElement);
