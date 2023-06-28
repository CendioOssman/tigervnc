import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    objectName: "OptionDialog"
    width: container.width
    height: container.height
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("TigerVNC Options")

    onVisibleChanged: {
        if (visible) {
            container.reset()
        }
    }

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }

    OptionDialogContent {
        id: container
        onCloseRequested: root.close()
    }
}
