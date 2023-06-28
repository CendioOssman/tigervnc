import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    objectName: "AlertDialog"
    property alias quit: container.quit
    property alias text: container.text

    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("TigerVNC Viewer")

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }
    
    AlertDialogContent {
        id: container
        onCloseRequested: root.close()
    }
}
