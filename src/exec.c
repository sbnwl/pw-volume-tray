/*
 * exec.c — process spawning.
 *
 * run() and run_async() are the only two functions in the whole app that
 * touch g_spawn_*. Every failure from either one goes through
 * report_error(), so callers never have to think about error plumbing —
 * they just get NULL back (run) or nothing happens (run_async).
 */

#include "pw-tray.h"
#include <stdarg.h>

void report_error(const gchar *msg)
{
    g_printerr("pw-tray: %s\n", msg);   /* terminal / systemd / Openbox log */

    if (!g_app || !g_app->icon) return;
    gtk_status_icon_set_from_icon_name(g_app->icon, "dialog-warning");
    gtk_status_icon_set_tooltip_text(g_app->icon, msg);
}

gchar *run(const gchar *cmd)
{
    gchar *out = NULL, *err = NULL;
    gint status = 0;
    GError *gerr = NULL;

    if (!g_spawn_command_line_sync(cmd, &out, &err, &status, &gerr)) {
        gchar *msg = g_strdup_printf("exec failed: %s (%s)", cmd, gerr->message);
        report_error(msg);
        g_free(msg);
        g_clear_error(&gerr);
        g_free(out);
        g_free(err);
        return NULL;
    }
    if (!g_spawn_check_wait_status(status, &gerr)) {
        gchar *msg = g_strdup_printf("'%s' failed: %s",
                                      cmd, (err && *err) ? err : gerr->message);
        report_error(msg);
        g_free(msg);
        g_clear_error(&gerr);
        g_free(out);
        g_free(err);
        return NULL;
    }
    g_free(err);
    return out;
}

void run_async(const gchar *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    gchar *cmd = g_strdup_vprintf(fmt, args);
    va_end(args);

    GError *gerr = NULL;
    if (!g_spawn_command_line_async(cmd, &gerr)) {
        gchar *msg = g_strdup_printf("failed to launch '%s': %s", cmd, gerr->message);
        report_error(msg);
        g_free(msg);
        g_clear_error(&gerr);
    }
    g_free(cmd);
}
