# Island2LocalSend

Island2LocalSend is a GNOME Shell extension + companion GTK application that provides a **floating “Dynamic Island” style drop target** for quickly sending files to **LocalSend** via drag and drop.

The project is designed specifically for **GNOME Shell 46+** and follows modern GNOME extension and IPC practices.

## Screenshots

[video](https://github.com/0xHertz/island2localsend/blob/main/Screencast%20from%202026-01-08%2012-05-20.mp4)

---

## Features

- Floating island-style UI triggered by file drag events
- Drag & drop files directly onto the island to send via LocalSend
- Automatic expand / collapse animation during drag operations
- GNOME Shell extension (JavaScript) + native GTK helper (C++)
- DBus-based communication between GNOME Shell and native app
- Desktop launcher and icon integration
- Minimal, low-overhead implementation

---

## Architecture Overview

Island2LocalSend consists of **three main components**:

### 1. GNOME Shell Extension

Location:
```
/gnome-extension
```

Key files:
- `extension.js` – Implements drag monitoring
- `metadata.json` – Extension metadata (GNOME 46/47 compatible)

Responsibilities:
- Monitor global drag-and-drop (DND) events
- Launch or communicate with the native sender process via DBUS

---

### 2. Native GTK App (C++)

Location:
```
/island2localsend.cpp
```

Responsibilities:
- Acts as a lightweight GTK application
- Display and animate the floating island UI
- Detect drag enter / leave / drop
- Bridges GNOME Shell with LocalSend
- Ensures reliable file handoff outside of the Shell sandbox

This separation is required because **GNOME Shell extensions cannot directly perform complex file or process operations**.

---

### 3. Desktop & Integration Assets

- `island2localsend.desktop` – Application launcher
- `icons/` – Full hicolor icon set (16x16 → 256x256, SVG)
- Optional autostart support (commented in install script)

---

## Installation

### Prerequisites

- GNOME Shell **46 or newer**
- `g++` with C++17 support
- `gtkmm-3.0`
- `pkg-config`

On Debian/Ubuntu-based systems:

```bash
sudo apt install g++ pkg-config libgtkmm-3.0-dev
```

---

### Install Steps

```bash
unzip island2localsend@kechen.zip
cd island2localsend@kechen
chmod +x install.sh
./install.sh
```

The script will:

1. Compile `island2localsend.cpp`
2. Generate the native app binary
3. Install desktop and icon assets

---

### Enable GNOME Extension

```bash
gnome-extensions install gnome-extension/island2localsend@kechen

gnome-extensions enable island2localsend@kechen
```

Then restart GNOME Shell:

- **X11**: `Alt + F2`, type `r`, press Enter
- **Wayland**: log out and back in

---

## Usage

1. Start island2localsend on your devices
2. Drag a file from Files (Nautilus)
3. The floating island appears automatically
4. Drop the file onto the island
5. LocalSend transfer begins immediately

No manual app switching required.

---

## Design Principles

- GNOME-native behavior (no hacks, no legacy APIs)
- Minimal UI footprint
- Clear separation between Shell and native code
- GNOME 46+ only (no backward compatibility burden)

---

## GNOME Version Support

| GNOME Version | Supported |
|--------------|-----------|
| 46           | ✅ Yes |
| 47           | ✅ Yes |
| ≤ 45         | ❌ No |

---

## Known Limitations

- Requires X11 for certain drag monitoring behaviors
- Wayland support depends on GNOME internal DND changes
- LocalSend must be installed and discoverable

---

## Development Notes

- GNOME Shell extensions are written in JavaScript (GJS)
- Native sender uses GTKmm (C++17)
- IPC via DBus for robustness and security

---

## License

MIT License

---

## Author

**KeChen**



