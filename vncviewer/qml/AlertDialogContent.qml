import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

GridLayout {
    id: container
    
    property bool quit: false
    property alias text: messageText.text

    rows: 2
    columns: 2
    rowSpacing: 0
    columnSpacing: 0

    signal closeRequested()

    function close() {
        closeRequested()
        AppManager.closeOverlay()
        if (quit) {
            Qt.quit()
        }
    }

    function reconnect() {
        closeRequested()
        AppManager.closeOverlay()
        AppManager.connectToServer("")
    }

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
        objectName: "AlertDialogMessageText"
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
