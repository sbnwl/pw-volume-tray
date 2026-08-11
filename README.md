# pw-tray

A deliberately small GTK3/XEmbed volume tray applet for PipeWire/WirePlumber,
built for Openbox + Tint2.

## Controls

- Left click: volume/output popup
- Scroll: volume up/down
- Middle click: mute/unmute
- Right click: output/input selection + Open mixer

## Architecture

This is a presentation layer only — it doesn't replace PipeWire or
WirePlumber, and doesn't go through the PulseAudio compatibility API. It
talks to the native PipeWire stack via `wpctl`, WirePlumber's control CLI.

The tray icon uses GTK3 `GtkStatusIcon`/XEmbed, which fits Tint2's system
tray. It's intentionally a small, replaceable piece: a future
Ubuntu-packaged native PipeWire tray app can swap in for this executable
without changing anything else in the workstation setup.

## Dependencies (Ubuntu)

    sudo apt install build-essential pkg-config libgtk-3-dev wireplumber pavucontrol

`wpctl` comes from WirePlumber. The mixer command defaults to
`pavucontrol` — override with `PWTRAY_MIXER`.

## Build

```sh
make
```

## Run without installing

```sh
./pw-tray
```

## Install (current user)

Installs to `~/.local` by default — no root needed:

```sh
make install
```

To start it automatically on login, copy the desktop entry into your
autostart folder yourself:

```sh
mkdir -p ~/.config/autostart
cp data/pw-tray.desktop ~/.config/autostart/
```

Or wire `~/.local/bin/pw-tray &` into your Openbox autostart script directly.

## Install (system-wide)

For packaging or a shared machine, override `PREFIX` (and, if you want
the desktop entry in the conventional system location rather than
`$PREFIX/share/applications`, `DESKTOPDIR`):

```sh
sudo make PREFIX=/usr/local install
# or, to place the desktop entry in /usr/share/applications:
sudo make PREFIX=/usr/local DESKTOPDIR=/usr/share/applications install
```

## Uninstall

```sh
make uninstall            # matches whatever PREFIX you installed with
```

## Configuration

- `PWTRAY_MIXER` — command launched by "Open mixer" (default: `pavucontrol`)

## Layout

```
.
├── data/           desktop entry (data/pw-tray.desktop)
├── Makefile
├── README.md
└── src/
    ├── pw-tray.h   shared types/constants + module map
    ├── exec.c      process spawning (run/run_async)
    ├── model.c     Node/Snapshot + wpctl status parsing
    ├── actions.c   fire-and-forget wpctl commands
    ├── state.c     tray icon + popup reconciliation
    ├── popup.c     the speech-bubble popup window
    ├── trayicon.c  GtkStatusIcon signals + right-click menu
    └── main.c      wiring
```
