/* pw-tray
 * Copyright (C) 2026 Surendra Beniwal
 * Author Email: surendra_beniwal@yahoo.co.in
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

/*
 * popup.c — the speech-bubble popup window.
 *
 * Everything here is scoped to one open/close cycle of the popup: its
 * shape (bubble_path/draw_bubble), where it's anchored (toggle_popup),
 * its controls (slider, output combo, mute button), and the auto-close
 * behavior that decides when it should go away on its own.
 */

#include "pw-tray.h"

/* Rough allowance for the combo box's own chrome (dropdown arrow, cell
 * padding, button frame) outside the text cell itself, so capping the
 * cell to (POPUP_WIDTH_MAX - COMBO_CHROME_PX) keeps the whole combo
 * widget — not just its text — at or under POPUP_WIDTH_MAX. Tune by eye
 * against your GTK theme; Adwaita-family themes are typically fine in the
 * 32-40px range. */
#define COMBO_CHROME_PX 36

static void on_scale_changed(GtkRange *r, App *a)
{
    if (!a->sink_id) return;
    do_unmute_if_muted(a);   /* touching the slider always means "I want sound" */
    set_volume(a->sink_id, gtk_range_get_value(r));
}

static void on_mute_clicked(GtkButton *b, App *a) { (void)b; do_toggle_mute(a); refresh_icon(a); }

static void on_output_changed(GtkComboBox *combo, App *a)
{
    gint idx = gtk_combo_box_get_active(combo);
    GArray *ids = g_object_get_data(G_OBJECT(combo), "ids");
    if (idx < 0 || !ids || (guint)idx >= ids->len) return;

    guint id = g_array_index(ids, guint, idx);
    set_default(id);
    a->sink_id = id;
    refresh_icon(a);
}

static void popup_destroyed(GtkWidget *w, App *a)
{
    (void)w;
    GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
    if (seat) gdk_seat_ungrab(seat);

    if (a->leave_timeout_id) {
        g_source_remove(a->leave_timeout_id);
        a->leave_timeout_id = 0;
    }
    a->popup = a->scale = a->mute_btn = a->vol_icon = a->vol_label = NULL;
}

/* Traces a rounded-rectangle body with an optional triangular pointer cut
 * into its top or bottom edge, in the widget's own pixel coordinates. */
static void bubble_path(cairo_t *cr, gint w, gint h,
                         gboolean arrow_up, gboolean show_arrow, gint arrow_x)
{
    const gdouble r = BUBBLE_RADIUS;
    gdouble top    = (show_arrow && arrow_up)  ? ARROW_H : 0;
    gdouble bottom = (show_arrow && !arrow_up) ? h - ARROW_H : h;

    cairo_new_path(cr);
    cairo_move_to(cr, r, top);

    if (show_arrow && arrow_up) {
        cairo_line_to(cr, arrow_x - ARROW_W / 2.0, top);
        cairo_line_to(cr, arrow_x, 0);
        cairo_line_to(cr, arrow_x + ARROW_W / 2.0, top);
    }
    cairo_line_to(cr, w - r, top);
    cairo_arc(cr, w - r, top + r, r, -G_PI_2, 0);
    cairo_line_to(cr, w, bottom - r);
    cairo_arc(cr, w - r, bottom - r, r, 0, G_PI_2);

    if (show_arrow && !arrow_up) {
        cairo_line_to(cr, arrow_x + ARROW_W / 2.0, bottom);
        cairo_line_to(cr, arrow_x, h);
        cairo_line_to(cr, arrow_x - ARROW_W / 2.0, bottom);
    }
    cairo_line_to(cr, r, bottom);
    cairo_arc(cr, r, bottom - r, r, G_PI_2, G_PI);
    cairo_line_to(cr, 0, top + r);
    cairo_arc(cr, r, top + r, r, G_PI, 3 * G_PI_2);
    cairo_close_path(cr);
}

/* Paints the bubble background/frame using the active theme's colors,
 * clipped to bubble_path(). Returns FALSE so GTK still propagates the
 * draw signal to the child box afterwards. */
static gboolean draw_bubble(GtkWidget *w, cairo_t *cr, App *a)
{
    gint width  = gtk_widget_get_allocated_width(w);
    gint height = gtk_widget_get_allocated_height(w);

    cairo_save(cr);
    bubble_path(cr, width, height, a->arrow_up, a->show_arrow, a->arrow_x);
    cairo_clip(cr);

    GtkStyleContext *ctx = gtk_widget_get_style_context(w);
    gtk_render_background(ctx, cr, 0, 0, width, height);
    gtk_render_frame(ctx, cr, 0, 0, width, height);
    cairo_restore(cr);

    return FALSE;
}

/* True if some widget other than w currently holds the input grab — e.g.
 * the output combo's dropdown while it's open. Both auto-close handlers
 * below skip closing in that case, since the dropdown taking the grab
 * also (depending on the WM) steals the popup's pointer hover and/or
 * input focus, which would otherwise look like "the user left/unfocused
 * the popup" even though they're still actively using it. */
static gboolean grab_held_elsewhere(GtkWidget *w)
{
    GtkWidget *g = gtk_grab_get_current();
    return g && g != w;
}

/* Delayed close for "mouse moved away": gives the user LEAVE_DELAY_MS to
 * come back before the popup actually disappears. Scheduled by
 * on_popup_leave(), cancelled by on_popup_enter() (or by popup_destroyed()
 * if the popup closes some other way first). */
static gboolean leave_timeout_cb(gpointer data)
{
    App *a = data;
    a->leave_timeout_id = 0;
    if (a->popup) {
        a->last_autoclose_us = g_get_monotonic_time();
        gtk_widget_destroy(a->popup);
    }
    return G_SOURCE_REMOVE;
}

/* GDK_NOTIFY_INFERIOR means the pointer only crossed into a child widget,
 * not out of the window, so that case is ignored — same filter used on
 * the "enter" side below to keep the two symmetric. */
static gboolean on_popup_leave(GtkWidget *w, GdkEventCrossing *ev, App *a)
{
    if (ev->detail == GDK_NOTIFY_INFERIOR) return FALSE;
    if (grab_held_elsewhere(w)) return FALSE;

    if (!a->leave_timeout_id)
        a->leave_timeout_id = g_timeout_add(LEAVE_DELAY_MS, leave_timeout_cb, a);
    return FALSE;
}

static gboolean on_popup_enter(GtkWidget *w, GdkEventCrossing *ev, App *a)
{
    (void)w;
    if (ev->detail == GDK_NOTIFY_INFERIOR) return FALSE;

    if (a->leave_timeout_id) {
        g_source_remove(a->leave_timeout_id);
        a->leave_timeout_id = 0;
    }
    return FALSE;
}

/* Closes the popup on click-elsewhere. This fires when the popup loses
 * input focus, which some window managers grant to an undecorated popup
 * window inconsistently — on_popup_outside_click() below is the more
 * reliable mechanism; this is a cheap second path in case it isn't. */
static gboolean on_popup_focus_out(GtkWidget *w, GdkEventFocus *ev, App *a)
{
    (void)ev;
    if (grab_held_elsewhere(w)) return FALSE;

    a->last_autoclose_us = g_get_monotonic_time();
    gtk_widget_destroy(w);
    return FALSE;
}

/* Primary mechanism for "click outside closes the popup, immediately".
 * toggle_popup() takes a pointer grab (owner_events=TRUE) when the popup
 * opens, so every button press anywhere on screen is delivered here
 * rather than relying on focus changes; clicks on our own widgets (the
 * slider, the combo and its dropdown, the mute button) still behave
 * normally under that grab. A grab-redirected event reports our own
 * window regardless of where the click physically landed, so "outside"
 * is determined from root coordinates against the popup's own bounds,
 * not from event->window. */
static gboolean on_popup_outside_click(GtkWidget *w, GdkEventButton *ev, App *a)
{
    GdkWindow *gwin = gtk_widget_get_window(w);
    if (!gwin) return FALSE;

    gint wx, wy;
    gdk_window_get_origin(gwin, &wx, &wy);
    gboolean inside = ev->x_root >= wx && ev->x_root < wx + gdk_window_get_width(gwin) &&
                      ev->y_root >= wy && ev->y_root < wy + gdk_window_get_height(gwin);
    if (inside) return FALSE;

    a->last_autoclose_us = g_get_monotonic_time();
    gtk_widget_destroy(w);
    return TRUE;
}

/* Builds the combo box, mute button, and slider from one Snapshot, and
 * shapes the window as a speech bubble pointing at (arrow_up, show_arrow)
 * — see toggle_popup() for how those are determined. */
static GtkWidget *build_popup(App *a, gboolean arrow_up, gboolean show_arrow)
{
    a->arrow_up   = arrow_up;
    a->show_arrow = show_arrow;

    Snapshot *s = fetch_snapshot();

    GtkWidget *win = gtk_window_new(GTK_WINDOW_POPUP);
    gtk_window_set_decorated(GTK_WINDOW(win), FALSE);
    gtk_window_set_type_hint(GTK_WINDOW(win), GDK_WINDOW_TYPE_HINT_POPUP_MENU);

    /* app-paintable + an RGBA visual let draw_bubble() paint a shaped,
     * transparent-cornered window instead of GTK's default plain rect. */
    gtk_widget_set_app_paintable(win, TRUE);
    GdkScreen *screen = gtk_widget_get_screen(win);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual) gtk_widget_set_visual(win, visual);
    g_signal_connect(win, "draw", G_CALLBACK(draw_bubble), a);

    gtk_widget_add_events(win, GDK_LEAVE_NOTIFY_MASK | GDK_ENTER_NOTIFY_MASK |
                                GDK_FOCUS_CHANGE_MASK | GDK_BUTTON_PRESS_MASK);
    g_signal_connect(win, "leave-notify-event", G_CALLBACK(on_popup_leave), a);
    g_signal_connect(win, "enter-notify-event", G_CALLBACK(on_popup_enter), a);
    g_signal_connect(win, "focus-out-event", G_CALLBACK(on_popup_focus_out), a);
    g_signal_connect(win, "button-press-event", G_CALLBACK(on_popup_outside_click), a);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_widget_set_margin_start(box, BUBBLE_MARGIN);
    gtk_widget_set_margin_end(box, BUBBLE_MARGIN);
    gtk_widget_set_margin_top(box, BUBBLE_MARGIN + (show_arrow && arrow_up  ? (gint)ARROW_H : 0));
    gtk_widget_set_margin_bottom(box, BUBBLE_MARGIN + (show_arrow && !arrow_up ? (gint)ARROW_H : 0));
    gtk_widget_set_size_request(box, POPUP_WIDTH_MAX, -1);
    gtk_container_add(GTK_CONTAINER(win), box);

    GtkWidget *vol_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    a->vol_icon = gtk_image_new_from_icon_name("audio-volume-muted", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(vol_row), a->vol_icon, FALSE, FALSE, 0);

    a->vol_label = gtk_label_new("0%");
    gtk_widget_set_size_request(a->vol_label, 32, -1); /* fixed width so
        the slider next to it doesn't shift as the digit count changes
        (e.g. "8%" vs "100%") */
    gtk_box_pack_start(GTK_BOX(vol_row), a->vol_label, FALSE, FALSE, 0);

    a->scale = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0.0, 1.0, 0.01);
    gtk_scale_set_draw_value(GTK_SCALE(a->scale), FALSE); /* the level is
        already shown by vol_label above; drawing it a second time on the
        slider itself would just repeat the same number twice in a row
        this compact */
    gtk_style_context_add_class(gtk_widget_get_style_context(a->scale), "pwtray-scale");
    gtk_box_pack_start(GTK_BOX(vol_row), a->scale, TRUE, TRUE, 0);
    a->scale_handler = g_signal_connect(a->scale, "value-changed",
                                         G_CALLBACK(on_scale_changed), a);

    gtk_box_pack_start(GTK_BOX(box), vol_row, FALSE, FALSE, 0);

    GtkWidget *combo = gtk_combo_box_text_new();
    GArray *ids = g_array_new(FALSE, FALSE, sizeof(guint));
    gint active = -1;
    Node *def = NULL;

    if (s) {
        for (guint i = 0; i < s->sinks->len; i++) {
            Node *n = g_ptr_array_index(s->sinks, i);
            gchar *text = g_strdup_printf("%s%s", n->name, n->is_default ? "  (default)" : "");
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), text);
            g_free(text);
            g_array_append_val(ids, n->id);
            if (n->is_default) { active = (gint)i; def = n; }
        }
        if (!def) def = snapshot_default(s->sinks);
    }
    g_object_set_data_full(G_OBJECT(combo), "ids", ids, (GDestroyNotify)g_array_unref);
    if (active >= 0) gtk_combo_box_set_active(GTK_COMBO_BOX(combo), active);
    g_signal_connect(combo, "changed", G_CALLBACK(on_output_changed), a);

    /* Long device names (e.g. "Built-in Audio Digital Stereo (HDMI)")
     * would otherwise stretch the popup — truncate with an ellipsis
     * instead. gtk_widget_set_size_request() on the box only ever sets a
     * *minimum*, never a maximum, so it can't stop this on its own; the
     * cell renderer's fixed size below is what actually caps the combo's
     * natural width request, which is what makes the ellipsize below
     * actually take effect instead of sitting unused. */
    GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(combo));
    if (cells) {
        GtkCellRenderer *cell = GTK_CELL_RENDERER(cells->data);
        g_object_set(cell, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
        gtk_cell_renderer_set_fixed_size(cell, POPUP_WIDTH_MAX - COMBO_CHROME_PX, -1);
        g_list_free(cells);
    }
    gtk_style_context_add_class(gtk_widget_get_style_context(combo), "pwtray-control");
    gtk_box_pack_start(GTK_BOX(box), combo, FALSE, FALSE, 0);

    a->mute_btn = gtk_button_new_with_label("Mute");
    gtk_style_context_add_class(gtk_widget_get_style_context(a->mute_btn), "pwtray-control");
    g_signal_connect(a->mute_btn, "clicked", G_CALLBACK(on_mute_clicked), a);
    gtk_box_pack_start(GTK_BOX(box), a->mute_btn, FALSE, FALSE, 0);
    update_mute_button(a->mute_btn, def && def->muted);

    if (def) {
        a->sink_id = def->id;
        a->muted   = def->muted;
        gdouble vol = node_effective_volume(def);
        gtk_range_set_value(GTK_RANGE(a->scale), vol);
        gtk_image_set_from_icon_name(GTK_IMAGE(a->vol_icon),
                                      volume_icon_name(def->muted, def->volume < 0 ? 0 : def->volume),
                                      GTK_ICON_SIZE_SMALL_TOOLBAR);
        gchar *pct = g_strdup_printf("%d%%", volume_percent(vol));
        gtk_label_set_text(GTK_LABEL(a->vol_label), pct);
        g_free(pct);
    }

    g_signal_connect(win, "destroy", G_CALLBACK(popup_destroyed), a);
    snapshot_free(s);
    return win;
}

/* Anchors the popup on the mouse pointer's position at click time, with a
 * triangular pointer aimed at that spot. This deliberately does NOT use
 * gtk_status_icon_get_geometry() — many tray hosts (Tint2 among them)
 * report it inaccurately or not at all, which is what put the bubble in
 * the wrong place. The pointer's actual position is always exactly where
 * the user clicked, i.e. on the icon, so it's a more reliable anchor.
 */
void toggle_popup(App *a)
{
    if (a->popup) { gtk_widget_destroy(a->popup); return; }

    /* Swallow a reopen that lands right after an auto-close — that's
     * almost always the same click that closed the popup (e.g. clicking
     * the tray icon itself while the popup is open) rather than a fresh
     * request to open it. */
    if (g_get_monotonic_time() - a->last_autoclose_us < AUTOCLOSE_GUARD_US) return;

    GdkDisplay *disp = gdk_display_get_default();
    GdkSeat *seat = gdk_display_get_default_seat(disp);
    GdkDevice *pointer = seat ? gdk_seat_get_pointer(seat) : NULL;

    gint px = 0, py = 0;
    gboolean have_pointer = FALSE;
    if (pointer) {
        GdkScreen *pscreen = NULL;
        gdk_device_get_position(pointer, &pscreen, &px, &py);
        have_pointer = TRUE;
    }

    GdkMonitor *mon = have_pointer
        ? gdk_display_get_monitor_at_point(disp, px, py)
        : gdk_display_get_primary_monitor(disp);
    GdkRectangle work;
    gdk_monitor_get_workarea(mon, &work);

    /* Pointer in the top half of its monitor -> panel is likely at the
     * top, so open below it (pointer up); otherwise open above (pointer
     * down), matching a bottom panel. */
    gboolean arrow_up = have_pointer && py < (work.y + work.height / 2);

    a->popup = build_popup(a, arrow_up, have_pointer);
    gtk_widget_show_all(a->popup);
    gtk_widget_grab_focus(a->popup);

    GtkRequisition req;
    gtk_widget_get_preferred_size(a->popup, &req, NULL);

    gint x, y;
    if (have_pointer) {
        x = CLAMP(px - req.width / 2, work.x + 4, work.x + work.width - req.width - 4);
        y = arrow_up ? py + NOMINAL_ICON_HALF : py - NOMINAL_ICON_HALF - req.height;
        y = CLAMP(y, work.y + 4, work.y + work.height - req.height - 4);

        a->arrow_x = CLAMP(px - x,
                            (gint)(BUBBLE_RADIUS + ARROW_W / 2),
                            req.width - (gint)(BUBBLE_RADIUS + ARROW_W / 2));
    } else {
        x = work.x + work.width  - req.width  - 12;
        y = work.y + work.height - req.height - 42;
    }

    gtk_window_move(GTK_WINDOW(a->popup), x, y);
    gtk_widget_queue_draw(a->popup); /* arrow_x/arrow_up are final now */

    /* See on_popup_outside_click(): this is what makes clicking anywhere
     * outside the popup close it immediately and reliably, independent
     * of whatever focus behavior the window manager gives an undecorated
     * popup window. */
    GdkWindow *gwin = gtk_widget_get_window(a->popup);
    if (seat && gwin)
        gdk_seat_grab(seat, gwin, GDK_SEAT_CAPABILITY_ALL_POINTING, TRUE,
                      NULL, NULL, NULL, NULL);
}
