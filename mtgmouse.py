import sys
import numpy as np
import cv2
import pyautogui
import keyboard
from mss import mss
from PySide6.QtWidgets import QApplication, QWidget
from PySide6.QtCore import Qt, QTimer, QPoint
from PySide6.QtGui import QPainter, QColor, QImage, QPixmap


class CursorZoom(QWidget):
    def __init__(self):
        super().__init__()
        self.setWindowFlags(Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool)
        self.setAttribute(Qt.WA_TranslucentBackground)
        self.resize(300, 300)

        self.sct = mss()
        self.zoom_factor = 2
        self.area_size = 100
        self.is_zoom_on = False

        # 定期更新用タイマー
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_view)
        self.timer.start(30)

        # ショートカット監視（別スレッドで動作）
        keyboard.add_hotkey("ctrl+shift+z", self.toggle_zoom)

    def toggle_zoom(self):
        self.is_zoom_on = not self.is_zoom_on
        if self.is_zoom_on:
            self.show()
        else:
            self.hide()
        print("Zoom ON" if self.is_zoom_on else "Zoom OFF")

    def update_view(self):
        if not self.is_zoom_on:
            return

        x, y = pyautogui.position()
        self.move(x - self.width() // 2, y - self.height() // 2)

        # キャプチャ範囲設定
        monitor = {
            "top": y - self.area_size // 2,
            "left": x - self.area_size // 2,
            "width": self.area_size,
            "height": self.area_size
        }

        img = np.array(self.sct.grab(monitor))
        img = cv2.cvtColor(img, cv2.COLOR_BGRA2RGB)
        zoomed = cv2.resize(img, (self.width(), self.height()), interpolation=cv2.INTER_LINEAR)
        self.qimg = QImage(zoomed.data, zoomed.shape[1], zoomed.shape[0], QImage.Format_RGB888)
        self.update()

    def paintEvent(self, event):
        if not hasattr(self, "qimg"):
            return

        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)

        # 拡大画像描画
        painter.drawPixmap(0, 0, QPixmap.fromImage(self.qimg))

        # 背景マスク
        overlay = QColor(0, 0, 0, 100)
        painter.setBrush(overlay)
        painter.setPen(Qt.NoPen)
        painter.drawRect(self.rect())

        painter.setCompositionMode(QPainter.CompositionMode_Clear)
        painter.drawEllipse(QPoint(self.width()//2, self.height()//2), 100, 100)

        # カーソル強調円
        painter.setCompositionMode(QPainter.CompositionMode_SourceOver)
        painter.setBrush(QColor(255, 255, 0, 120))
        painter.setPen(Qt.NoPen)
        painter.drawEllipse(QPoint(self.width()//2, self.height()//2), 8, 8)


if __name__ == "__main__":
    app = QApplication(sys.argv)
    w = CursorZoom()
    w.hide()  # 起動時は非表示
    sys.exit(app.exec())
