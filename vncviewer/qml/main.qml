import QtQuick 2.15
import Qt.TigerVNC 1.0

Item {
    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14

    function alert(message) {
        alertDialog.text = message
        alertDialog.open()
    }

    Component.onCompleted: serverDialog.visible = true
    Connections {
        target: AppManager
        function onErrorOcurred(seq, message) {
            alertDialog.text = message
            alertDialog.open()
        }

        function onNewVncWindowRequested(width, height, name) {
            serverDialog.visible = false

            var component = Qt.createComponent("VncWindow.qml");
            if (component.status === Component.Ready) {
                component.createObject(null, { width: width, height: height, title: name, visible: true })
            }
        }
    }

    ServerDialog {
        id: serverDialog
    }

    Loader {
        active: false
        AlertDialog {
            id: alertDialog
        }
    }
}
