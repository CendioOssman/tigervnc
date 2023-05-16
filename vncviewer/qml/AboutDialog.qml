import QtQuick 2.12
import QtQuick.Window 2.12
import Qt.TigerVNC 1.0

Window {
    id: root

    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14

    width: 525
    height: 155
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("About TigerVNC Viewer")
    color: "#ffdcdcdc"

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }

    Image {
        id: infoIcon
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.leftMargin: 10
        anchors.topMargin: 20
        source: "qrc:/images/info_48px.png"
    }
    Text {
        id: aboutText
        anchors.left: infoIcon.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: closeButton.top
        anchors.leftMargin: 10
        anchors.rightMargin: 10
        anchors.topMargin: 18
        anchors.bottomMargin: 10
        font.pixelSize: labelFontPixelSize
        text: Config.aboutText
    }
    CButton {
        id: closeButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 15
        anchors.bottomMargin: 10
        width: 72
        focus: true
        font.pixelSize: buttonFontPixelSize
        text: qsTr("Close")
        onClicked: close()
    }
}
