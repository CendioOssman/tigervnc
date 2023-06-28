import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    objectName: "InfoDialog"
    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("VNC connection info")

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }

    InfoDialogContent {
        id: container
        onCloseRequested: root.close()
    }
}
