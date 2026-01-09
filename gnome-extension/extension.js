import { Extension } from "resource:///org/gnome/shell/extensions/extension.js";
import * as DND from "resource:///org/gnome/shell/ui/dnd.js";

import Gio from "gi://Gio";
import GLib from "gi://GLib";

export default class LocalSendIsland extends Extension {
  enable() {
    this._enabled = true;
    this._remoteEnabled = false;
    this._dbusProxy = null;

    this._setupDBusConnection();
    this._setupDND();
  }

  disable() {
    this._enabled = false;

    if (this._dragMonitor) {
      DND.removeDragMonitor(this._dragMonitor);
      this._dragMonitor = null;
    }
    if (this._dbusProxy && this._signalId) {
      this._dbusProxy.disconnectSignal(this._signalId);
      this._signalId = null;
    }

    this._dbusProxy = null;
  }

  _setupDBusConnection() {
    try {
      this._dbusProxy = new Gio.DBusProxy({
        g_connection: Gio.DBus.session,
        g_name: "com.kechen.island2localsend",
        g_object_path: "/com/kechen/island2localsend",
        g_interface_name: "com.kechen.island2localsend",
        g_flags: Gio.DBusProxyFlags.DO_NOT_LOAD_PROPERTIES,
      });
      try {
        this._dbusProxy.init_async(null, (proxy, res) => {
          if (!this._enabled) return;

          try {
            proxy.init_finish(res);
            this._signalId = proxy.connectSignal(
              "StatusChanged",
              (_proxy, _sender, params) => {
                const [isEnable, message] = params.deepUnpack();

                console.log(
                  `[LocalSend Island] StatusChanged: ${isEnable}, ${message}`,
                );

                this._remoteEnabled = isEnable;
              },
            );
            console.log("[LocalSend Island] DBus connected");
          } catch (e) {
            console.error("[LocalSend Island] DBus init failed:", e);
          }
        });
      } catch (e) {
        console.error("[LocalSend Island] DBus init failed:", e);
      }
    } catch (e) {
      console.error("[LocalSend Island] DBus setup failed:", e);
    }
  }

  _sendEnableSignal() {
    if (!this._dbusProxy || !this._enabled) return;

    const parameters = new GLib.Variant("(bs)", [true, "From GNOME Extension"]);

    this._dbusProxy.call(
      "Enable",
      parameters,
      Gio.DBusCallFlags.NONE,
      -1,
      null,
      () => {},
    );
  }

  _setupDND() {
    this._dragMonitor = {
      dragMotion: (event) => {
        if (this._remoteEnabled) return DND.DragMotionResult.CONTINUE;

        if (event?.y < 300) {
          this._triggered = true;
          this._sendEnableSignal();
        }

        return DND.DragMotionResult.CONTINUE;
      },

      dragDrop: () => {
        this._triggered = false;
        return DND.DragMotionResult.CONTINUE;
      },

      dragCancel: () => {
        this._triggered = false;
      },
    };

    DND.addDragMonitor(this._dragMonitor);
  }
}
