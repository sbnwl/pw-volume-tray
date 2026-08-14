/* pw-tray
 * Copyright (C) 2026 Surendra Beniwal
 * Author Email: surendra_beniwal@yahoo.co.in
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * model.c — Node/Snapshot and wpctl status parsing.
 *
 * One `wpctl status` call yields the whole picture. Sinks:/Sources: entries
 * look like:
 *     " │  *   50. Built-in Audio Analog Stereo   [vol: 0.65 MUTED]"
 * '*' = default, the bracket is optional (absent on Devices:/Streams:
 * lines, which we skip). The regex is unanchored so leading box-drawing
 * characters/indentation don't need stripping.
 *
 * Invariant: a Snapshot's Node pointers are only valid until
 * snapshot_free(); anything that must outlive it is copied out first.
 */

#include "pw-tray.h"

static void node_free(gpointer p)
{
    Node *n = p;
    if (n) { g_free(n->name); g_free(n); }
}

Snapshot *fetch_snapshot(void)
{
    gchar *out = run("wpctl status");
    if (!out) return NULL; /* run() already called report_error() */

    static GRegex *re = NULL;
    if (!re)
        re = g_regex_new(
            "(\\*)?\\s*(\\d+)\\.\\s+(.+?)\\s*"
            "(?:\\[vol:\\s*([0-9.]+)(\\s+MUTED)?\\])?\\s*$",
            0, 0, NULL);

    Snapshot *s = g_new0(Snapshot, 1);
    s->sinks   = g_ptr_array_new_with_free_func(node_free);
    s->sources = g_ptr_array_new_with_free_func(node_free);

    GPtrArray *current = NULL;
    gchar **lines = g_strsplit(out, "\n", -1);

    for (guint i = 0; lines[i]; i++) {
        gchar *t = g_strstrip(lines[i]);

        if (g_str_has_suffix(t, "Sinks:"))   { current = s->sinks;   continue; }
        if (g_str_has_suffix(t, "Sources:")) { current = s->sources; continue; }
        if (g_str_has_suffix(t, "Devices:") || g_str_has_suffix(t, "Filters:") ||
            g_str_has_suffix(t, "Streams:")) { current = NULL;       continue; }
        if (!current || !*t) continue;

        GMatchInfo *m;
        if (g_regex_match(re, t, 0, &m)) {
            gchar *star  = g_match_info_fetch(m, 1);
            gchar *id_s  = g_match_info_fetch(m, 2);
            gchar *vol_s = g_match_info_fetch(m, 4);
            gchar *mute_s= g_match_info_fetch(m, 5);

            Node *n = g_new0(Node, 1);
            n->id         = (guint)g_ascii_strtoull(id_s, NULL, 10);
            n->name       = g_match_info_fetch(m, 3);
            n->is_default = star && *star;
            n->volume     = (vol_s && *vol_s) ? g_ascii_strtod(vol_s, NULL) : -1.0;
            n->muted      = mute_s && *mute_s;
            g_ptr_array_add(current, n);

            g_free(star); g_free(id_s); g_free(vol_s); g_free(mute_s);
        }
        g_match_info_free(m);
    }

    g_strfreev(lines);
    g_free(out);
    return s;
}

void snapshot_free(Snapshot *s)
{
    if (!s) return;
    g_ptr_array_free(s->sinks, TRUE);
    g_ptr_array_free(s->sources, TRUE);
    g_free(s);
}

Node *snapshot_default(GPtrArray *nodes)
{
    if (!nodes) return NULL;
    for (guint i = 0; i < nodes->len; i++) {
        Node *n = g_ptr_array_index(nodes, i);
        if (n->is_default) return n;
    }
    return nodes->len ? g_ptr_array_index(nodes, 0) : NULL;
}

/* What the volume slider should actually show: 0 while muted, regardless
 * of the underlying (unchanged) volume value — mute and volume are
 * independent in PipeWire, so this is a display-only override, not a
 * mutation. Single source of truth shared by refresh_icon() (state.c)
 * and build_popup() (popup.c) so both agree, always, on one definition. */
gdouble node_effective_volume(const Node *n)
{
    if (!n || n->muted) return 0.0;
    return n->volume < 0 ? 0.0 : n->volume;
}
