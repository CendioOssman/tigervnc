import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("About TigerVNC Viewer")

    function open() {
        // Assign the message text again. This workaround is needed for X11's weird behavior.
        // On X11, Text's content is displayed only once if the Text content has three or more lines.
        // To work around this,  Text's content must be cleared once, and then set it again.
        aboutText.text = Config.aboutText
        visible = true
    }

    function close() {
        aboutText.text = ""
        visible = false
    }

    GridLayout {
        id: container
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Image {
            id: infoIcon
            Layout.row: 0
            Layout.column: 0
            Layout.leftMargin: 10
            Layout.topMargin: 20
            source: "image://qticons/SP_MessageBoxInformation"
        }
        Text {
            id: aboutText
            Layout.row: 0
            Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 10
            Layout.rightMargin: 15
            Layout.topMargin: 18
            Layout.bottomMargin: 10
            lineHeight: 1.2
        }

        Button {
            id: closeButton
            Layout.row: 1
            Layout.column: 1
            Layout.rightMargin: 15
            Layout.topMargin: 5
            Layout.bottomMargin: 10
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.preferredWidth: 72
            focus: true
            text: qsTr("Close")
            onClicked: close()
        }
    }
}
