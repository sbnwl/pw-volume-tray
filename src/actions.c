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
 * exposed as cross-module API in pw-tray.h. */
static void toggle_mute_cmd(void)    { run_async("wpctl set-mute @DEFAULT_SINK@ toggle"); }

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
