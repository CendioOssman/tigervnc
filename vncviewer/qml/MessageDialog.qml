import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    property int response: 0
    property int setting: 0
    property alias text: messageText.text

    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("TigerVNC Viewer")

    function open() {
        var buttonFlag = (setting & 0x0f)
        standardButtons = DialogButtonBox.Ok
        if (buttonFlag == 1 /* M_OKCANCEL */) {
            standardButtons = (DialogButtonBox.Ok | DialogButtonBox.Cancel)
        }
        else if (buttonFlag == 4 /* M_YESNO */) {
            standardButtons = (DialogButtonBox.Yes | DialogButtonBox.No)
        }

        var iconFlag = (setting & 0xf0)
        dialogIcon.source = "image://qticons/SP_MessageBoxCritical" // M_ICONERROR
        if (iconFlag == 0x20 /* M_ICONQUESTION */) {
            dialogIcon.source = "image://qticons/SP_MessageBoxQuestion"
        }
        else if (iconFlag == 0x30 /* M_ICONWARNING */) {
            dialogIcon.source = "image://qticons/SP_MessageBoxWarning"
        }
        else if (iconFlag == 0x40 /* M_ICONINFORMATION */) {
            dialogIcon.source = "image://qticons/SP_MessageBoxInformation"
        }

        response = 0
        visible = true
    }

    function close() {
        visible = false
        AppManager.respondToMessage(response)
    }

    GridLayout {
        id: container
        rows: 2
        columns: 2
        rowSpacing: 0
        columnSpacing: 0

        Image {
            id: dialogIcon
            Layout.row: 0
            Layout.column: 0
            Layout.leftMargin: 10
            Layout.topMargin: 15
        }
        Text {
            id: messageText
            Layout.row: 0
            Layout.column: 1
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumWidth: 300
            Layout.leftMargin: 10
            Layout.rightMargin: 15
            verticalAlignment: Text.AlignVCenter
        }
        DialogButtonBox {
            id: buttonBox
            Layout.row: 1
            Layout.column: 0
            Layout.columnSpan: 2
            Layout.rightMargin: 15
            Layout.topMargin: 5
            Layout.bottomMargin: 10
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom

            onAccepted: response = 1
            onRejected: response = 0
        }
    }
}
