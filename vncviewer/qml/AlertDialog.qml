import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    property bool quit: false
    property alias text: messageText.text

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
        if (quit) {
            Qt.quit()
        }
    }

    function reconnect() {
        visible = false
        AppManager.connectToServer("")
    }

    GridLayout {
        id: container
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Image {
            id: alertIcon
            Layout.row: 0
            Layout.column: 0
            Layout.leftMargin: 10
            Layout.topMargin: 15
            source: "image://qticons/SP_MessageBoxWarning"
        }
        Text {
            id: messageText
            Layout.row: 0
            Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 300
            Layout.maximumWidth: 600
            Layout.leftMargin: 10
            Layout.rightMargin: 15
            verticalAlignment: Text.AlignVCenter
            wrapMode: Text.Wrap
        }
        Row {
            id: buttonRow
            Layout.row: 1
            Layout.column: 1
            Layout.rightMargin: 15
            Layout.topMargin: 5
            Layout.bottomMargin: 10
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            spacing: 10
            Button {
                id: noButton
                width: 72
                visible: Config.reconnectOnError && !quit
                text: qsTr("No")
                onClicked: close()
            }
            Button {
                id: yesButton
                width: 72
                visible: Config.reconnectOnError && !quit
                focus: true
                text: qsTr("Yes")
                onClicked: reconnect()
            }
            Button {
                id: closeButton
                width: 72
                visible: !Config.reconnectOnError || quit
                focus: true
                text: qsTr("Close")
                onClicked: close()
            }
        }
    }
}
