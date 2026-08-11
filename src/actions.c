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
/* Deliberately synchronous (unlike most actions here) — same reasoning
 * as toggle_mute_cmd() below: both of this function's call sites
 * (popup.c's output combo, trayicon.c's right-click menu) immediately
 * re-check state via refresh_icon() right after. run_async() gives no
 * guarantee the switch has actually landed by then, so an immediate
 * refresh_icon() could win the race and briefly show the old sink's
 * state — including its mute-button color — on what looks like the
 * newly selected output. */
void set_default(guint id)
{
    gchar *cmd = g_strdup_printf("wpctl set-default %u", id);
    gchar *out = run(cmd);
    g_free(cmd);
    g_free(out);
}

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

/* Sets both the mute button's label and its visual state from one place,
 * so do_toggle_mute(), do_unmute_if_muted(), and build_popup()'s initial
 * state can't drift out of sync with each other. The "destructive-action"
 * style class is GTK's own built-in convention for "dangerous/attention"
 * buttons (the same one theme delete/remove buttons use) — this lets the
 * active theme pick the actual shade rather than hardcoding a color here. */
void update_mute_button(GtkWidget *btn, gboolean muted)
{
    if (!btn) return;
    gtk_button_set_label(GTK_BUTTON(btn), muted ? "Unmute" : "Mute");

    GtkStyleContext *ctx = gtk_widget_get_style_context(btn);
    if (muted)
        gtk_style_context_add_class(ctx, "destructive-action");
    else
        gtk_style_context_remove_class(ctx, "destructive-action");
}

void do_toggle_mute(App *a)
{
    toggle_mute_cmd();
    a->muted = !a->muted;
    update_mute_button(a->mute_btn, a->muted);
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
    update_mute_button(a->mute_btn, FALSE);
}
