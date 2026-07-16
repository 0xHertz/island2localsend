<h1 align="center">Island2LocalSend</h1>

<p align="center">
  <img src="icons/island2localsend.svg" alt="Island2LocalSend Icon" width="128" />
</p>

Island2LocalSend is a GNOME Shell extension + companion GTK application that provides a **floating "Dynamic Island" style drop target** for quickly sending files via drag and drop.

Supports **LocalSend** and **GSConnect** — drag left for GSConnect (blue), drag right for LocalSend (green) with inline device selection.

Designed for **GNOME Shell 46+**.

---

## Features

- Floating island-style UI triggered by file drag events
- **Split-zone island**: left half → GSConnect, right half → LocalSend
- **Color-coded fill animation**: blue for GSConnect, green for LocalSend
- **Inline GSConnect device picker**: horizontal pill buttons with scrollable list
- **Spring-physics animations**: smooth expand/collapse, scroll inertia, bounce feedback
- Drag & drop files onto island or specific GSConnect device
- GNOME Shell extension (JavaScript) + native GTK helper (C++)
- DBus-based communication between Shell, native app, and GSConnect
- System tray icon with enable/disable/quit menu

---

## Architecture

Three components connected by DBus:

### 1. GNOME Shell Extension
- `gnome-extension/extension.js` — monitors global DND, detects drag near top of screen
- `gnome-extension/metadata.json` — GNOME 46/47 compatible

### 2. Native GTK App (C++)
- `island2localsend.cpp` — Cairo-drawn floating island, split-zone drag detection, device list, animations
- `island2localsend.desktop` — application launcher
- `install.sh` — build & install script

### 3. GSConnect (external)
- DBus service at `org.gnome.Shell.Extensions.GSConnect`
- Called via `org.gtk.Actions.Activate` on device objects

---

## Installation

### Prerequisites
- GNOME Shell 46+
- `g++` (C++17), `gtkmm-3.0`, `ayatana-appindicator3-0.1`, `gio-2.0`

```bash
sudo apt install g++ pkg-config libgtkmm-3.0-dev libayatana-appindicator3-dev
```

### Build & Install
```bash
cd island2localsend@kechen
chmod +x install.sh
./install.sh
```

### Enable Extension
```bash
gnome-extensions install gnome-extension/island2localsend@kechen
gnome-extensions enable island2localsend@kechen
```

Restart GNOME Shell (Alt+F2 → `r` on X11, or log out/in on Wayland).

---

## Usage

1. Start `island2localsend` (auto-starts on login if configured)
2. Drag a file from Nautilus to the top of the screen
3. The floating island appears:

| Action | Visual | Backend |
|--------|--------|---------|
| Drag to **left half** | Blue fill → device list | GSConnect |
| Drag to **right half** | Green fill | LocalSend |
| Select a device | Green pill highlight | GSConnect → selected device |
| Release | Island contracts, sends file | — |

GSConnect device list: up to 3 visible. With 4+ devices, drag to island edges to scroll.

---

## GSConnect Send (DBus)
```
gdbus call --session \
  --dest org.gnome.Shell.Extensions.GSConnect \
  --object-path /org/gnome/Shell/Extensions/GSConnect/Device/{ID} \
  --method org.gtk.Actions.Activate \
  shareFile "[<('/path/to/file', false)>]" {}
```

---

## GNOME Version Support

| GNOME | Supported |
|-------|-----------|
| 46    | ✅ |
| 47    | ✅ |
| ≤ 45  | ❌ |

---

## License

MIT

---

## Author

KeChen
