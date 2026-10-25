// Copyright 2026 The Paper Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Haptic, LauncherPageCallbackRouter, LauncherPageHandlerFactory, LauncherPageHandlerRemote, SidebarPageCallbackRouter, SidebarPageHandlerFactory, SidebarPageHandlerRemote} from './paper.mojom-webui.js';

export class SidebarProxy {
  readonly handler = new SidebarPageHandlerRemote();
  readonly router = new SidebarPageCallbackRouter();

  private constructor() {
    SidebarPageHandlerFactory.getRemote().createPageHandler(
        this.router.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  private static instance_: SidebarProxy|null = null;
  static get(): SidebarProxy {
    return SidebarProxy.instance_ ??= new SidebarProxy();
  }
}

export class LauncherProxy {
  readonly handler = new LauncherPageHandlerRemote();
  readonly router = new LauncherPageCallbackRouter();

  private constructor() {
    LauncherPageHandlerFactory.getRemote().createPageHandler(
        this.router.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  private static instance_: LauncherProxy|null = null;
  static get(): LauncherProxy {
    return LauncherProxy.instance_ ??= new LauncherProxy();
  }
}

// Haptics only make sense while a finger is on the trackpad, so callers fire
// them from pointer handlers. Rapid repeats (a fast drag sweeping past many
// slots) are collapsed so the trackpad ticks rather than buzzes.
const MIN_HAPTIC_INTERVAL_MS = 45;
let lastHapticAt = 0;

export function haptic(
    pattern: Haptic, send: (pattern: Haptic) => void): void {
  const now = performance.now();
  if (pattern === Haptic.kAlignment &&
      now - lastHapticAt < MIN_HAPTIC_INTERVAL_MS) {
    return;
  }
  lastHapticAt = now;
  send(pattern);
}

export function faviconUrl(pageUrl: string, size = 32): string {
  const params = new URLSearchParams({
    size: String(size),
    scaleFactor: `${Math.ceil(window.devicePixelRatio)}x`,
    pageUrl,
    allowGoogleServerFallback: '0',
  });
  return `chrome://favicon2/?${params.toString()}`;
}

export function displayHost(url: string): string {
  try {
    return new URL(url).hostname.replace(/^www\./, '');
  } catch {
    return url;
  }
}
