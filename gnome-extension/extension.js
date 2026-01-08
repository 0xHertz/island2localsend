// import * as Main from "resource:///org/gnome/shell/ui/main.js";
// import { Extension } from "resource:///org/gnome/shell/extensions/extension.js";
// import { DropIsland } from "./island.js";
// import Gio from "gi://Gio";

// export default class LocalSendIsland extends Extension {
//   enable() {
//     this._dragActive = false;
//     this._collapseTimeoutId = 0;
//     this._buildUI();
//     this._grabEndId = global.display.connect("grab-op-end", () => {
//       if (!this._islandShown) return;
//       this._islandShown = false;
//       log("[LocalSend] grab-op-end → island reset");
//     });
//   }

//   disable() {
//     if (this._dragActorSignal) {
//       global.stage.disconnect(this._dragActorSignal);
//       this._dragActorSignal = 0;
//     }
//     if (this._dragMonitor) {
//       DND.removeDragMonitor(this._dragMonitor);
//       this._dragMonitor = null;
//     }

//     if (this._island) {
//       Main.layoutManager.removeChrome(this._island);
//       this._island.destroy();
//       this._island = null;
//     }
//     if (this._grabEndId) {
//       global.display.disconnect(this._grabEndId);
//       this._grabEndId = null;
//     }
//   }
//   /* ---------------- UI ---------------- */
//   _buildUI() {
//     this._island = new DropIsland();
//   }
// }

import { Extension } from "resource:///org/gnome/shell/extensions/extension.js";
import * as Main from "resource:///org/gnome/shell/ui/main.js";
import Gio from "gi://Gio";

export default class DndMonitorExtension extends Extension {
  enable() {
    this._dbusId = null;
    // 监听全局拖拽开始信号
    // 注意：GNOME 内部 DND 模块通常会在开始时触发信号
    this._onDragBeginId = Main.xdndHandler.connect("drag-begin", () => {
      this._notifyGtkApp(true);
    });

    this._onDragEndId = Main.xdndHandler.connect("drag-end", () => {
      this._notifyGtkApp(false);
    });

    console.log("DND Monitor Extension Enabled");
  }

  disable() {
    if (this._onDragBeginId) Main.xdndHandler.disconnect(this._onDragBeginId);
    if (this._onDragEndId) Main.xdndHandler.disconnect(this._onDragEndId);
    this._onDragBeginId = null;
    this._onDragEndId = null;
  }

  _notifyGtkApp(isDragging) {
    // 使用 Gio 发送 DBus 信号或调用你 GTK 程序注册的方法
    // 这里以简单的控制台打印为例，实际应用中建议使用 DBus
    log(`Global DND Status: ${isDragging}`);

    // 示例：通过 DBus 告知程序
    // Proxy 调用逻辑在此处实现...
  }
}
