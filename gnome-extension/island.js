import St from "gi://St";
import Clutter from "gi://Clutter";
import GObject from "gi://GObject";
import Gio from "gi://Gio";

import * as DND from "resource:///org/gnome/shell/ui/dnd.js";

// Define a class that behaves as a Drop Target
export const DropIsland = GObject.registerClass(
  class DropIsland extends St.BoxLayout {
    _init() {
      this._setupDND();
    }

    destroy() {
      if (this._dragMonitor) {
        DND.removeDragMonitor(this._dragMonitor);
        this._dragMonitor = null;
      }
    }
    /* ---------------- Drag & Drop ---------------- */

    _isIslandRunning() {
      try {
        const proc = Gio.Subprocess.new(
          ["pgrep", "-f", "island2localsend"],
          Gio.SubprocessFlags.STDOUT_PIPE | Gio.SubprocessFlags.STDERR_PIPE,
        );

        proc.wait_check(null);
        return true; // pgrep 找到进程
      } catch (e) {
        return false; // 未运行
      }
    }
    _setupDND() {
      console.log("[LocalSend] Setting up DND monitor");

      this._dragMonitor = {
        dragMotion: (event) => {
          if (this._isIslandRunning()) return DND.DragMotionResult.CONTINUE;

          log(`Drag motion detected`);
          try {
            Gio.Subprocess.new(["island2localsend"], Gio.SubprocessFlags.NONE);
          } catch (e) {
            console.error(e);
          }

          return DND.DragMotionResult.CONTINUE;
        },
        dragDrop: (dragEvent) => {
          // 拖拽释放
          log("dragDrop");
          return false;
        },

        dragCancel: () => {
          log("dragCancel");
          // 拖拽取消（Esc / 非法 drop）
        },
        dragEnd: () => {
          // 拖拽结束后重置
          console.log("[LocalSend] dragEnd → reset trigger");
        },
      };
      DND.addDragMonitor(this._dragMonitor);
      console.log("[LocalSend] DND monitor added");
    }
  },
);
