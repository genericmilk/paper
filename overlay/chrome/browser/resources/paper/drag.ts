// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pointer-driven drag and drop for the sidebar. HTML5 drag and drop can't do
// springy ghosts or per-slot haptics, so this tracks the pointer itself and
// asks the host where a drop would land on every move.

import {Haptic} from './paper.mojom-webui.js';

export type DragKind = 'tab'|'essential'|'bookmark'|'folder';
export type DropZone = 'essentials'|'tabs'|'bookmarks';

export interface DragSource {
  kind: DragKind;
  id: string;
  el: HTMLElement;
}

export interface DropTarget {
  zone: DropZone;
  // Insertion index within the zone (or within `parentId` for bookmarks).
  index: number;
  // Bookmark folder id, as a decimal string. Empty means the bookmark bar.
  parentId?: string;
  // Dropping onto a folder row rather than between rows.
  into?: boolean;
  // Where to draw the insertion line, in viewport coordinates.
  line?: {left: number, top: number, width: number, height: number};
  // Element (by data-key) to light up instead of drawing a line.
  highlightKey?: string;
}

export interface DragHost {
  resolveTarget(source: DragSource, x: number, y: number): DropTarget|null;
  showIndicator(target: DropTarget|null): void;
  drop(source: DragSource, target: DropTarget): void;
  haptic(pattern: Haptic): void;
}

const START_DISTANCE_PX = 4;
// How quickly the ghost catches up with the pointer, per frame. Below 1 gives
// it a little weight, so it trails and settles instead of being glued on.
const FOLLOW = 0.35;

function targetKey(t: DropTarget|null): string {
  return t ? `${t.zone}:${t.parentId ?? ''}:${t.index}:${t.into ? 1 : 0}` : '';
}

export class DragController {
  private source_: DragSource|null = null;
  private startX_ = 0;
  private startY_ = 0;
  private offsetX_ = 0;
  private offsetY_ = 0;
  private started_ = false;
  private ghost_: HTMLElement|null = null;
  private ghostX_ = 0;
  private ghostY_ = 0;
  private pointerX_ = 0;
  private pointerY_ = 0;
  private frame_ = 0;
  private target_: DropTarget|null = null;

  constructor(private readonly host_: DragHost) {}

  get dragging(): boolean {
    return this.started_;
  }

  // Call from pointerdown on a draggable item.
  begin(e: PointerEvent, source: DragSource) {
    if (e.button !== 0) {
      return;
    }
    this.source_ = source;
    this.startX_ = e.clientX;
    this.startY_ = e.clientY;
    const rect = source.el.getBoundingClientRect();
    this.offsetX_ = e.clientX - rect.left;
    this.offsetY_ = e.clientY - rect.top;
    window.addEventListener('pointermove', this.onMove_);
    window.addEventListener('pointerup', this.onUp_, {once: true});
    window.addEventListener('pointercancel', this.onCancel_, {once: true});
  }

  private onMove_ = (e: PointerEvent) => {
    this.pointerX_ = e.clientX;
    this.pointerY_ = e.clientY;
    if (!this.started_) {
      const moved = Math.hypot(e.clientX - this.startX_, e.clientY - this.startY_);
      if (moved < START_DISTANCE_PX) {
        return;
      }
      this.start_();
    }
    const target = this.host_.resolveTarget(this.source_!, e.clientX, e.clientY);
    if (targetKey(target) !== targetKey(this.target_)) {
      // Entering a folder is a bigger moment than shuffling a slot.
      this.host_.haptic(
          target?.into ? Haptic.kLevelChange : Haptic.kAlignment);
      this.target_ = target;
      this.host_.showIndicator(target);
    }
  };

  private onUp_ = () => {
    const source = this.source_;
    const target = this.target_;
    const started = this.started_;
    this.finish_();
    if (!started || !source) {
      return;
    }
    // The click that follows a drag would otherwise activate the item.
    window.addEventListener(
        'click', e => e.stopPropagation(), {capture: true, once: true});
    if (target) {
      this.host_.haptic(Haptic.kLevelChange);
      this.host_.drop(source, target);
    }
  };

  private onCancel_ = () => {
    this.finish_();
  };

  private start_() {
    const el = this.source_!.el;
    const rect = el.getBoundingClientRect();
    this.started_ = true;
    this.host_.haptic(Haptic.kGeneric);

    const ghost = el.cloneNode(true) as HTMLElement;
    ghost.classList.add('drag-ghost');
    ghost.style.width = `${rect.width}px`;
    ghost.style.height = `${rect.height}px`;
    document.body.appendChild(ghost);
    this.ghost_ = ghost;
    this.ghostX_ = rect.left;
    this.ghostY_ = rect.top;
    el.classList.add('drag-source');
    document.body.classList.add('dragging');
    this.frame_ = requestAnimationFrame(this.tick_);
  }

  private tick_ = () => {
    if (!this.ghost_) {
      return;
    }
    const goalX = this.pointerX_ - this.offsetX_;
    const goalY = this.pointerY_ - this.offsetY_;
    this.ghostX_ += (goalX - this.ghostX_) * FOLLOW;
    this.ghostY_ += (goalY - this.ghostY_) * FOLLOW;
    // Tilt with horizontal velocity, like a card being pulled.
    const tilt = Math.max(-6, Math.min(6, (goalX - this.ghostX_) * 0.4));
    this.ghost_.style.transform = `translate(${this.ghostX_}px, ${
        this.ghostY_}px) rotate(${tilt}deg) scale(1.04)`;
    this.frame_ = requestAnimationFrame(this.tick_);
  };

  private finish_() {
    window.removeEventListener('pointermove', this.onMove_);
    window.removeEventListener('pointerup', this.onUp_);
    window.removeEventListener('pointercancel', this.onCancel_);
    cancelAnimationFrame(this.frame_);
    if (this.ghost_) {
      const ghost = this.ghost_;
      ghost.classList.add('drag-ghost-out');
      ghost.addEventListener('transitionend', () => ghost.remove(), {once: true});
      setTimeout(() => ghost.remove(), 300);
    }
    this.source_?.el.classList.remove('drag-source');
    document.body.classList.remove('dragging');
    this.host_.showIndicator(null);
    this.source_ = null;
    this.ghost_ = null;
    this.target_ = null;
    this.started_ = false;
  }
}
