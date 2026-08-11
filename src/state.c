/*
 * state.c — reconciling live state with the tray icon and popup.
 *
 * refresh_icon() is the single place that does this, from one fresh
 * Snapshot. It's called on the poll timer and after a menu/combo
 * selection — never more than once per event.
 */

#include "pw-tray.h"

/* Which icon name represents a given (muted, volume) pair. Pure and
 * shared — used here, popup.c, and osd.c, so none of them can disagree
 * about what "low" vs "medium" volume looks like. */
const gchar *volume_icon_name(gboolean muted, gdouble volume)
{
    return (muted || volume <= 0.001) ? "audio-volume-muted"
         : (volume < 0.34)            ? "audio-volume-low"
         : (volume < 0.67)            ? "audio-volume-medium"
                                       : "audio-volume-high";
}

/* volume (0.0-1.0+) as a rounded whole-number percent. Shared by osd.c
 * and popup.c so the OSD and the popup's own percentage label can't
 * round differently or drift apart. */
gint volume_percent(gdouble volume)
{
    return (gint)(CLAMP(volume, 0.0, 1.0) * 100.0 + 0.5);
}

void refresh_icon(App *a)
{
    Snapshot *s = fetch_snapshot();
    if (!s) return;

    Node *def = snapshot_default(s->sinks);
    if (!def) { snapshot_free(s); return; }

    a->sink_id = def->id;
    a->muted   = def->muted;
    gdouble vol = def->volume < 0 ? 0 : def->volume;
    a->last_volume = vol;

    gtk_status_icon_set_from_icon_name(a->icon, volume_icon_name(def->muted, vol));

    if (a->scale) {
        g_signal_handler_block(a->scale, a->scale_handler);
        gtk_range_set_value(GTK_RANGE(a->scale), node_effective_volume(def));
        g_signal_handler_unblock(a->scale, a->scale_handler);
    }
    if (a->vol_icon)
        gtk_image_set_from_icon_name(GTK_IMAGE(a->vol_icon),
                                      volume_icon_name(def->muted, vol),
                                      GTK_ICON_SIZE_SMALL_TOOLBAR);
    if (a->vol_label) {
        gchar *pct = g_strdup_printf("%d%%", volume_percent(node_effective_volume(def)));
        gtk_label_set_text(GTK_LABEL(a->vol_label), pct);
        g_free(pct);
    }
    update_mute_button(a->mute_btn, def->muted);

    snapshot_free(s);
}

gboolean tick_cb(gpointer a) { refresh_icon(a); return G_SOURCE_CONTINUE; }
