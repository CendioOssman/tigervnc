import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import Qt.TigerVNC 1.0

Window {
    id: root

    objectName: "AboutDialog"
    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("About TigerVNC Viewer")

    function open() {
        // Assign the message text again. This workaround is needed for X11's weird behavior.
        // On X11, Text's content is displayed only once if the Text content has three or more lines.
        // To work around this,  Text's content must be cleared once, and then set it again.
        container.text = Config.aboutText
        visible = true
    }

    function close() {
        container.text = ""
        visible = false
    }

    AboutDialogContent {
        id: container
        onCloseRequested: root.close()
    }
}
