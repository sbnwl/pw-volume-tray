/*
 * state.c — reconciling live state with the tray icon and popup.
 *
 * refresh_icon() is the single place that does this, from one fresh
 * Snapshot. It's called on the poll timer, after scroll, and after a
 * menu/combo selection — never more than once per event.
 */

#include "pw-tray.h"

void refresh_icon(App *a)
{
    Snapshot *s = fetch_snapshot();
    if (!s) return;

    Node *def = snapshot_default(s->sinks);
    if (!def) { snapshot_free(s); return; }

    a->sink_id = def->id;
    a->muted   = def->muted;
    gdouble vol = def->volume < 0 ? 0 : def->volume;

    const gchar *icon = (def->muted || vol <= 0.001) ? "audio-volume-muted"
                       : (vol < 0.34)                 ? "audio-volume-low"
                       : (vol < 0.67)                 ? "audio-volume-medium"
                                                       : "audio-volume-high";
    gtk_status_icon_set_from_icon_name(a->icon, icon);

    if (a->scale) {
        g_signal_handler_block(a->scale, a->scale_handler);
        gtk_range_set_value(GTK_RANGE(a->scale), vol);
        g_signal_handler_unblock(a->scale, a->scale_handler);
    }
    if (a->mute_btn)
        gtk_button_set_label(GTK_BUTTON(a->mute_btn), def->muted ? "Unmute" : "Mute");

    snapshot_free(s);
}

gboolean tick_cb(gpointer a) { refresh_icon(a); return G_SOURCE_CONTINUE; }
