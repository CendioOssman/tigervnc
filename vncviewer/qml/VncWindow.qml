import QtQuick.Window 2.15
import Qt.TigerVNC 1.0

Window {
    id: vncWindow

    VncCanvas {
        anchors.fill: parent
    }
}
