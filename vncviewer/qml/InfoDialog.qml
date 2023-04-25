import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14

    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("VNC connection info")
    color: "#ffdcdcdc"

    function open() {
        infoText.text = AppManager.connection.infoText()
        visible = true
    }

    function close() {
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
            font.pixelSize: labelFontPixelSize
        }
        CButton {
            id: closeButton
            Layout.row: 1
            Layout.column: 1
            Layout.rightMargin: 15
            Layout.topMargin: 5
            Layout.bottomMargin: 10
            Layout.alignment: Qt.AlignRight | Qt.AlignBottom
            Layout.preferredWidth: 72
            focus: true
            font.pixelSize: buttonFontPixelSize
            text: qsTr("Close")
            onClicked: close()
        }
    }
}
