/*
 * pw-tray.h — shared types and cross-module declarations.
 *
 * One header for the whole app on purpose: pw-tray is small enough that a
 * per-module header each re-declaring a slice of Node/Snapshot/App would
 * add file-hunting for no real benefit. Each .c file below owns one
 * concern; this header is just the shared vocabulary they use to talk to
 * each other.
 *
 * Module map (mirrors the .c files in this directory):
 *   exec.c     — run()/run_async(): the only two functions that touch
 *                g_spawn_*. Every failure goes through report_error().
 *   model.c    — Node/Snapshot + fetch_snapshot(): one `wpctl status` call
 *                parses sinks and sources (id, name, default flag, volume,
 *                mute) in a single pass. Everything else that needs sink
 *                or source data takes a Snapshot as an argument rather
 *                than fetching its own.
 *                Invariant: a Snapshot's Node pointers are only valid
 *                until snapshot_free(); anything that must outlive it
 *                (an id for a menu item, a name string) is copied out
 *                first.
 *   actions.c  — set_volume/set_default/bump_volume/do_toggle_mute/
 *                do_unmute_if_muted: thin fire-and-forget wrappers over
 *                run_async().
 *   state.c    — refresh_icon(): the single place that reconciles the
 *                tray icon and open popup with one fresh Snapshot.
 *   popup.c    — the speech-bubble popup window: shape, positioning,
 *                slider/combo/mute button, and its own open/close and
 *                auto-close behavior.
 *   trayicon.c — GtkStatusIcon signal handlers and the right-click menu.
 *   main.c     — wiring: owns the App instance and g_app, calls gtk_init/
 *                gtk_main.
 */

#ifndef PWTRAY_H
#define PWTRAY_H

#include <gtk/gtk.h>

/* ------------------------------------------------------------- config */

#define APP_NAME      "PipeWire Tray"
#define POPUP_TITLE   "Audio Volume"   /* user-facing heading shown at the
                                           top of the popup; kept separate
                                           from APP_NAME, which is used for
                                           the tray icon's own title/
                                           tooltip and is not shown in the
                                           popup itself */
#define ICON_NAME     "audio-volume-high"
#define MIXER_ENV     "PWTRAY_MIXER"
#define MIXER_DEFAULT "pavucontrol"
#define REFRESH_SECS  2

#define POPUP_WIDTH_MAX 260   /* hard ceiling on popup content width, in
                                 pixels; enforced in popup.c by capping the
                                 output combo's cell renderer directly, not
                                 by this constant alone — gtk_widget_set_
                                 size_request() (used on both the box and,
                                 formerly, this value) only ever sets a
                                 minimum, never a maximum */
#define BUBBLE_MARGIN   12   /* padding between content and popup edge */
#define BUBBLE_RADIUS   10.0 /* corner radius of the speech-bubble body */
#define ARROW_W         20.0 /* width of the pointer triangle's base */
#define ARROW_H         10.0 /* height the pointer triangle adds */
#define NOMINAL_ICON_HALF 10 /* small gap left between the popup and the
                                 click point (see toggle_popup: many tray
                                 hosts, Tint2 included, misreport
                                 gtk_status_icon_get_geometry(), so the
                                 popup is anchored on the pointer's actual
                                 position at click time instead — that's
                                 always exactly where the icon is) */
#define AUTOCLOSE_GUARD_US 200000 /* ignore a reopen this soon after an
                                      auto-close, so the click that closes
                                      the popup doesn't also reopen it */
#define LEAVE_DELAY_MS 700       /* grace period before a mouse-away
                                      close, so briefly overshooting the
                                      popup with the pointer doesn't
                                      dismiss it (see on_popup_leave) */

/* --------------------------------------------------------------- types */

typedef struct {
    guint    id;
    gchar   *name;
    gboolean is_default;
    gdouble  volume;  /* -1 if not present on this line (e.g. Devices:) */
    gboolean muted;
} Node;

typedef struct {
    GPtrArray *sinks;   /* Node*, owned */
    GPtrArray *sources; /* Node*, owned */
} Snapshot;

typedef struct {
    GtkStatusIcon *icon;
    GtkWidget     *popup;
    GtkWidget     *scale;
    GtkWidget     *mute_btn;
    gulong         scale_handler;
    guint          sink_id;
    gboolean       muted;

    /* Popup shape/position, set once per open in toggle_popup() and read
     * by draw_bubble(). arrow_x is in the popup window's own coordinates. */
    gboolean       show_arrow;
    gboolean       arrow_up;
    gint           arrow_x;
    gint64         last_autoclose_us;
    guint          leave_timeout_id; /* pending delayed close, 0 if none */
} App;

/* The one running App instance (defined in main.c). report_error() uses
 * it to reach the tray icon; nothing else reads or writes it. */
extern App *g_app;

/* ----------------------------------------------------------------- exec */

gchar *run(const gchar *cmd);
void   run_async(const gchar *fmt, ...);
void   report_error(const gchar *msg);

/* ---------------------------------------------------------------- model */

Snapshot *fetch_snapshot(void);
void      snapshot_free(Snapshot *s);
Node     *snapshot_default(GPtrArray *nodes);
gdouble   node_effective_volume(const Node *n);

/* -------------------------------------------------------------- actions */

void set_volume(guint id, gdouble v);
void set_default(guint id);
void bump_volume(gint pct);
void do_toggle_mute(App *a);
void do_unmute_if_muted(App *a);

/* --------------------------------------------------------------- state */

void     refresh_icon(App *a);
gboolean tick_cb(gpointer a);

/* ---------------------------------------------------------------- popup */

void toggle_popup(App *a);

/* ------------------------------------------------------------- trayicon */

void trayicon_init(App *a);

#endif /* PWTRAY_H */
