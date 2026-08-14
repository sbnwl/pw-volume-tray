/* pw-tray
 * Copyright (C) 2026 Surendra Beniwal
 * Author Email: surendra_beniwal@yahoo.co.in
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * main.c — wiring.
 *
 * Owns the one App instance and g_app. Everything else in the app is a
 * self-contained module (see pw-tray.h for the map); this file just
 * starts them up in order and hands control to gtk_main().
 */

#include "pw-tray.h"

App *g_app = NULL;

/* A couple of lines of visual polish, layered on top of whatever GTK
 * theme is active rather than replacing it. Delete this function and its
 * call below to go back to plain theme defaults. */
static void install_css(void)
{
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
        "scale.pwtray-scale trough,"
        "scale.pwtray-scale trough highlight { min-height: 1px; border-radius: 2px; }"
        ".pwtray-control { min-height: 22px; padding-top: 2px; padding-bottom: 2px; }"
        , -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(), GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);
}

int main(int argc, char **argv)
{
    gtk_init(&argc, &argv);
    install_css();

    App app = {0};
    g_app = &app;

    trayicon_init(&app);

    refresh_icon(&app);
    g_timeout_add_seconds(REFRESH_SECS, tick_cb, &app);

    gtk_main();
    return 0;
}
