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
    title: qsTr("VNC connection info")

    function open() {
        infoText.text = AppManager.connection.infoText()
        visible = true
    }

    function close() {
        infoText.text = ""
        visible = false
    }

    Timer {
        id: refreshTimer
        running: root.visible
        interval: 500
        repeat: true
        onTriggered: infoText.text = AppManager.connection.infoText()
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
            Layout.topMargin: 15
            source: "qrc:/images/info_48px.png"
        }
        Text {
            id: infoText
            Layout.row: 0
            Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 300
            Layout.leftMargin: 10
            Layout.rightMargin: 15
            Layout.topMargin: 18
            verticalAlignment: Text.AlignVCenter
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
