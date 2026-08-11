/*
 * actions.c — fire-and-forget wpctl commands.
 *
 * Thin wrappers over run_async(). None of these wait for wpctl or update
 * app state themselves beyond what's needed for immediate UI feedback
 * (do_toggle_mute); the next refresh_icon() call reconciles everything
 * against reality.
 */

#include "pw-tray.h"
#include <stdlib.h> /* abs() */

void set_volume(guint id, gdouble v) { run_async("wpctl set-volume %u %.3f", id, v); }
void set_default(guint id)           { run_async("wpctl set-default %u", id); }

/* Only called from do_toggle_mute() below, so kept file-local rather than
 * exposed as cross-module API in pw-tray.h. Deliberately synchronous
 * (unlike every other action here) — do_toggle_mute() and its callers
 * immediately re-check state via refresh_icon() right after, and that
 * re-check must see the *post*-mute state. run_async() gives no such
 * guarantee: it returns before wpctl has actually finished, so an
 * immediate refresh_icon() could win the race and read stale, pre-mute
 * state, undoing the mute it just performed. */
static void toggle_mute_cmd(void)
{
    gchar *out = run("wpctl set-mute @DEFAULT_SINK@ toggle");
    g_free(out);
}

void bump_volume(gint pct)
{
    run_async("wpctl set-volume @DEFAULT_SINK@ %d%%%s", abs(pct), pct < 0 ? "-" : "+");
}

void do_toggle_mute(App *a)
{
    toggle_mute_cmd();
    a->muted = !a->muted;
    if (a->mute_btn)
        gtk_button_set_label(GTK_BUTTON(a->mute_btn), a->muted ? "Unmute" : "Mute");
}

/* Explicit unmute, not a toggle, so this can never accidentally re-mute —
 * used by the slider's drag handler so touching the slider while muted
 * clears mute and audibly resumes, instead of the drag silently having
 * no effect while the mute flag stays set underneath it. No-op if
 * already unmuted. */
void do_unmute_if_muted(App *a)
{
    if (!a->muted) return;
    run_async("wpctl set-mute @DEFAULT_SINK@ 0");
    a->muted = FALSE;
    if (a->mute_btn)
        gtk_button_set_label(GTK_BUTTON(a->mute_btn), "Mute");
}
