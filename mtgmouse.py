import sys
import json
import numpy as np
import cv2
import pyautogui
import keyboard
from mss import mss
from PySide6.QtCore import QObject, QTimer, Signal, Property, QBuffer, QIODevice, Qt
from PySide6.QtGui import QGuiApplication, QImage
from PySide6.QtQml import QQmlApplicationEngine
import base64

class ZoomController(QObject):
    toggleState = Signal(bool)
    mouseXChanged = Signal(int)
    mouseYChanged = Signal(int)
    zoomImageChanged = Signal(str)  # Base64

    def __init__(self, config):
        super().__init__()
        self.hotkey = config.get("hotkey", "ctrl+shift+z")
        self.zoom_factor = config.get("zoom_factor", 2)
        self.area_size = config.get("area_size", 120)
        self.is_zoom_on = False

        self._mouse_x = -1
        self._mouse_y = -1
        self.sct = mss()
        self.screen_width, self.screen_height = pyautogui.size()

        keyboard.add_hotkey(self.hotkey, self.toggle_zoom)
        print(f"ショートカットキー [{self.hotkey}] でズーム切替")

        # タイマー
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_view)
        self.timer.start(30)

    def toggle_zoom(self):
        self.is_zoom_on = not self.is_zoom_on
        self.toggleState.emit(self.is_zoom_on)
        print("Zoom ON" if self.is_zoom_on else "Zoom OFF")

    def update_view(self):
        x, y = pyautogui.position()

        # マウスが動かない場合、更新不要
        if x == self._mouse_x and y == self._mouse_y and self.is_zoom_on:
            return

        if x != self._mouse_x:
            self._mouse_x = x
            self.mouseXChanged.emit(x)
        if y != self._mouse_y:
            self._mouse_y = y
            self.mouseYChanged.emit(y)

        if not self.is_zoom_on:
            return

        # 全画面キャプチャ
        monitor = {"top": 0, "left": 0, "width": self.screen_width, "height": self.screen_height}
        img = np.array(self.sct.grab(monitor))
        img = cv2.cvtColor(img, cv2.COLOR_BGRA2RGB)

        # マウス周辺切り出し（画面端補正）
        half = self.area_size // 2
        top = max(min(y - half, self.screen_height - self.area_size), 0)
        left = max(min(x - half, self.screen_width - self.area_size), 0)
        bottom = top + self.area_size
        right = left + self.area_size

        cropped = img[top:bottom, left:right]

        # 2倍拡大
        zoomed = cv2.resize(
            cropped,
            (self.area_size * self.zoom_factor, self.area_size * self.zoom_factor),
            interpolation=cv2.INTER_LINEAR
        )

        # QImage → PNG → Base64
        h, w, _ = zoomed.shape
        qimg = QImage(zoomed.data, w, h, QImage.Format_RGB888)
        buffer = QBuffer()
        buffer.open(QIODevice.WriteOnly)
        qimg.save(buffer, "PNG")
        img_base64 = base64.b64encode(buffer.data()).decode('ascii')
        self.zoomImageChanged.emit(img_base64)

    @Property(int, notify=mouseXChanged)
    def mouseX(self):
        return self._mouse_x

    @Property(int, notify=mouseYChanged)
    def mouseY(self):
        return self._mouse_y


def load_config():
    default_config = {"hotkey": "ctrl+shift+z", "zoom_factor": 2, "area_size": 120}
    try:
        with open("config.json", "r", encoding="utf-8") as f:
            user_conf = json.load(f)
            default_config.update(user_conf)
    except FileNotFoundError:
        with open("config.json", "w", encoding="utf-8") as f:
            json.dump(default_config, f, indent=4, ensure_ascii=False)
    return default_config


if __name__ == "__main__":
    app = QGuiApplication(sys.argv)
    engine = QQmlApplicationEngine()

    config = load_config()
    controller = ZoomController(config)
    engine.rootContext().setContextProperty("controller", controller)

    engine.load("zoom_overlay.qml")
    if not engine.rootObjects():
        sys.exit(-1)

    root_obj = engine.rootObjects()[0]
    # 完全透明かつマウス透過
    root_obj.setFlags(root_obj.flags() | Qt.WindowTransparentForInput)

    sys.exit(app.exec())
