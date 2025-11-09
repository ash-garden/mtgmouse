import QtQuick
import QtQuick.Window
import QtQuick.Controls

Window {
    id: root
    width: 300
    height: 300
    visible: true
    color: "transparent" // 完全透明
    flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint

    // 背景完全透明
    Rectangle {
        anchors.fill: parent
        color: "#00000000"
    }

    // ズームエリア（ズームON/OFF）
    Rectangle {
        id: zoomArea
        width: 200
        height: 200
        radius: width / 2
        anchors.centerIn: parent
        clip: true
        border.color: "white"
        border.width: 1
        visible: false

        Image {
            id: zoomImage
            anchors.fill: parent
            fillMode: Image.PreserveAspectCrop
        }
    }

    // 強調円（常に表示）
    Rectangle {
        id: cursorHighlight
        width: 60
        height: 60
        radius: 30
        color: "transparent"
        border.color: "#FFFF00"
        border.width: 2
        anchors.centerIn: parent
    }

    Connections {
        target: controller
        function onToggleState(on) {
            zoomArea.visible = on
        }
        function onZoomImageChanged(base64Data) {
            zoomImage.source = "data:image/png;base64," + base64Data
        }
    }

    // マウス追従
    Timer {
        interval: 30
        running: true
        repeat: true
        onTriggered: {
            root.x = controller.mouseX - root.width / 2
            root.y = controller.mouseY - root.height / 2
        }
    }
}
