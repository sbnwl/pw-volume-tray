/* pw-tray
 * Copyright (C) 2026 Surendra Beniwal
 * Author Email: surendra_beniwal@yahoo.co.in
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * osd.c — optional on-screen volume/mute notifications, via notify-send.
 *
 * Entirely opt-in and best-effort: if no notification daemon is running
 * (nothing providing org.freedesktop.Notifications on the session bus —
 * e.g. no dunst/xfce4-notifyd), notify-send simply has no effect, and
 * run_async_silent() lets that fail without raising pw-tray's own
 * warning icon. This feature has no business complaining about
 * something the user never asked to enable.
 *
 * Every call passes the same fixed OSD_NOTIFICATION_ID as notify-send's
 * -r (replace-id), so every OSD call updates one notification in place
 * instead of stacking a new one each time. That's a genuine no-round-trip
 * trick, not a workaround: replaces_id is a required parameter of the
 * core Notify() D-Bus method itself, not an optional hint a daemon can
 * ignore — the spec says a server replaces the notification with that ID
 * if one is showing, or creates one with that ID if not. No need to read
 * an ID back from a previous call, which would mean a synchronous
 * notify-send call — exactly the kind of blocking-under-a-scroll-burst
 * problem trayicon.c's on_scroll() is built to avoid.
 *
 * Because replaces_id is spec-level rather than daemon-specific, this
 * works identically on dunst, xfce4-notifyd, and any other compliant
 * daemon — unlike a hint such as x-dunst-stack-tag, which only dunst
 * understands.
 */

#include "pw-tray.h"

/* Arbitrary, fixed, chosen once — see the file comment above for why a
 * constant (rather than an ID read back from a previous call) is
 * exactly the point, not a shortcut. */
#define OSD_NOTIFICATION_ID 424547

void osd_show(gdouble volume, gboolean muted)
{
    gint pct = volume_percent(volume);
    const gchar *icon    = volume_icon_name(muted, volume);
    const gchar *summary = muted ? "Muted" : "Volume";

    run_async_silent(
        "notify-send -r %d -i %s -h int:value:%d -t 1500 \"%s\"",
        OSD_NOTIFICATION_ID, icon, muted ? 0 : pct, summary);
}
