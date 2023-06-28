import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

GridLayout {
    id: container

    property alias text: infoText.text
    objectName: "InfoDialogContent"
    rows: 2
    columns: 2
    rowSpacing: 0
    columnSpacing: 0

    signal closeRequested()

    function close() {
        closeRequested()
        AppManager.closeOverlay()
    }

    Timer {
        id: refreshTimer
        running: AppManager.visibleInfo
        interval: 500
        repeat: true
        triggeredOnStart: true
        onTriggered: infoText.text = AppManager.connection.infoText()
    }

    Image {
        id: infoIcon
        Layout.row: 0
        Layout.column: 0
        Layout.leftMargin: 10
        Layout.topMargin: 15
        source: "image://qticons/SP_MessageBoxInformation"
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
