/*
 * trayicon.c — GtkStatusIcon signal handlers and the right-click menu.
 *
 * trayicon_init() wires the icon up; everything else here is either a
 * direct signal handler or a helper used only by on_popup_menu().
 */

#include "pw-tray.h"

static void on_activate(GtkStatusIcon *icon, App *a) { (void)icon; toggle_popup(a); }

static void on_scroll(GtkStatusIcon *icon, GdkEventScroll *ev, App *a)
{
    (void)icon;
    /* No immediate refresh_icon() here on purpose: bump_volume() is
     * async (deliberately, since a fast scroll burst can fire many of
     * these events in under a second — making it synchronous, like
     * set_default()/toggle_mute_cmd(), would risk each one briefly
     * blocking the main loop and stacking into visible judder). An
     * immediate synchronous refresh right after would just race an
     * async command, same failure mode already fixed elsewhere. The
     * periodic REFRESH_SECS tick reconciles the tray icon glyph shortly
     * after; the actual audio change is instant either way. */
    gint delta = 0;
    if (ev->direction == GDK_SCROLL_UP   || ev->delta_y < 0) delta = +5;
    if (ev->direction == GDK_SCROLL_DOWN || ev->delta_y > 0) delta = -5;
    if (!delta) return;

    bump_volume(delta);

    /* Same "optimistic local estimate" approach used elsewhere (e.g.
     * do_toggle_mute()'s label flip) — good enough for a quick OSD
     * glance, self-corrects at the next poll tick, and needs no new
     * wpctl query (which would reintroduce the exact race/jank risk
     * avoided above). */
    a->last_volume = CLAMP(a->last_volume + delta / 100.0, 0.0, 1.0);
    osd_show(a->last_volume, a->muted);
}

static gboolean on_button_press(GtkStatusIcon *icon, GdkEventButton *ev, App *a)
{
    (void)icon;
    if (ev->button == 2) {
        do_toggle_mute(a);
        osd_show(a->last_volume, a->muted);
        refresh_icon(a);
        return TRUE;
    }
    return FALSE;
}

static void on_menu_node(GtkMenuItem *item, App *a)
{
    guint id = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item), "id"));
    set_default(id);
    a->sink_id = id;
    refresh_icon(a);
}

/* Renders one submenu (Output or Input) from a caller-supplied node list. */
static GtkWidget *build_node_submenu(GPtrArray *nodes, App *a)
{
    GtkWidget *menu = gtk_menu_new();

    if (!nodes || !nodes->len) {
        GtkWidget *item = gtk_menu_item_new_with_label("No devices found");
        gtk_widget_set_sensitive(item, FALSE);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    } else {
        for (guint i = 0; i < nodes->len; i++) {
            Node *n = g_ptr_array_index(nodes, i);
            gchar *text = g_strdup_printf("%s%s", n->name, n->is_default ? "  \xE2\x9C\x93" : "");
            GtkWidget *item = gtk_menu_item_new_with_label(text);
            g_free(text);
            g_object_set_data(G_OBJECT(item), "id", GUINT_TO_POINTER(n->id));
            g_signal_connect(item, "activate", G_CALLBACK(on_menu_node), a);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        }
    }
    gtk_widget_show_all(menu);
    return menu;
}

/* Silent on purpose (see run_async_silent()'s own comment) — the mixer
 * is a genuinely optional convenience, same category as the OSD's
 * notify-send. If pavucontrol (or a custom PWTRAY_MIXER) isn't
 * installed, clicking this simply does nothing; that's not an error
 * pw-tray should be raising its own warning icon over. */
static void open_mixer(GtkMenuItem *item, gpointer data)
{
    (void)item; (void)data;
    const gchar *mixer = g_getenv(MIXER_ENV);
    run_async_silent("%s", (mixer && *mixer) ? mixer : MIXER_DEFAULT);
}

static void on_popup_menu(GtkStatusIcon *icon, guint button, guint time, App *a)
{
    (void)icon; (void)button; (void)time;
    Snapshot *s = fetch_snapshot(); /* shared by both submenus below */

    GtkWidget *menu = gtk_menu_new();
    /* gtk_menu_popup*() does not free a popup menu once it's dismissed —
     * this is a documented GTK behavior, not an oversight here — so a
     * freshly built menu like this one must be told to self-destruct
     * once it's done, or it leaks on every right-click. */
    g_signal_connect(menu, "selection-done", G_CALLBACK(gtk_widget_destroy), NULL);

    GtkWidget *out = gtk_menu_item_new_with_label("Output");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(out), build_node_submenu(s ? s->sinks : NULL, a));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), out);

    GtkWidget *in = gtk_menu_item_new_with_label("Input");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(in), build_node_submenu(s ? s->sources : NULL, a));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), in);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

    GtkWidget *mixer = gtk_menu_item_new_with_label("Open mixer");
    g_signal_connect(mixer, "activate", G_CALLBACK(open_mixer), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), mixer);

    gtk_widget_show_all(menu);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);

    snapshot_free(s); /* menu items already copied out the ids they need */
}

void trayicon_init(App *a)
{
    a->icon = gtk_status_icon_new_from_icon_name(ICON_NAME);
    gtk_status_icon_set_title(a->icon, APP_NAME);
    gtk_status_icon_set_visible(a->icon, TRUE);

    g_signal_connect(a->icon, "activate",           G_CALLBACK(on_activate), a);
    g_signal_connect(a->icon, "scroll-event",       G_CALLBACK(on_scroll), a);
    g_signal_connect(a->icon, "popup-menu",         G_CALLBACK(on_popup_menu), a);
    g_signal_connect(a->icon, "button-press-event", G_CALLBACK(on_button_press), a);
}
