import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.12
import QtQuick.Window 2.15

Window {
    id: root

    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14
    property bool secured: false
    property alias userNeeded: userText.visible
    property alias passwordNeeded: passwordText.visible

    width: 420
    height: 170
    flags: Qt.SubWindow
    modality: Qt.ApplicationModal
    title: qsTr("VNC Authentication")
    color: "#ffdcdcdc"

    signal commit(string user, string password)
    signal abort()

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }

    function cancel() {
        passwordText.text = ""
        close()
        abort()
    }

    function accept() {
        close()
        commit(userText.text, passwordText.text)
        passwordText.text = ""
    }

    onVisibleChanged: {
        if (visible) {
            if (userNeeded) {
                userText.forceActiveFocus()
            }
            else if (passwordNeeded) {
                passwordText.forceActiveFocus()
            }
        }
    }

    Rectangle {
        id: statusArea
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: statusContent.height
        color: "#ffff0000"

        Item {
            id: statusContent
            anchors.horizontalCenter: statusArea.horizontalCenter
            width: statusIcon.width + statusMessage.width + statusMessage.anchors.leftMargin
            height: 20
            Image {
                id: statusIcon
                y: 3
                width: 14
                height: 14
                fillMode: Image.PreserveAspectFit
                source: "qrc:/images/lock_48px.png"
                Rectangle {
                    id: insecureBar
                    x: parent.width / 2
                    y: parent.width / 4
                    width: 2
                    height: parent.height
                    rotation: 50
                    transformOrigin: Item.Center
                    color: "#ffff0000"
                }
            }
            Text {
                id: statusMessage
                anchors.left: statusIcon.right
                anchors.leftMargin: 3
                anchors.verticalCenter: statusIcon.verticalCenter
                height: statusIcon.height
                text: qsTr("This connection is not secure")
                verticalAlignment: Text.AlignVCenter
            }
        }
        states: [
            State {
                when: secured
                PropertyChanges {
                    target: statusArea
                    color: "#ff00ff00"
                }
                PropertyChanges {
                    target: statusMessage
                    text: qsTr("This connection is secure")
                }
                PropertyChanges {
                    target: insecureBar
                    visible: false
                }
            }
        ]
    }

    Image {
        id: authIcon
        anchors.left: parent.left
        anchors.top: statusArea.bottom
        anchors.leftMargin: 15
        anchors.topMargin: 20
        source: "qrc:/images/help_48px.png"
    }
    GridLayout {
        anchors.left: authIcon.right
        anchors.right: parent.right
        anchors.top: statusArea.bottom
        anchors.leftMargin: 10
        anchors.topMargin: 20
        columns: 2
        rows: 2
        rowSpacing: 3
        columnSpacing: 5

        Text {
            id: userLabel
            Layout.leftMargin: 10
            visible: userText.visible
            font.pixelSize: labelFontPixelSize
            text: qsTr("User:")
        }
        TextField {
            id: userText
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 25
            Layout.preferredHeight: 25
            font.pixelSize: labelFontPixelSize
        }
        Text {
            id: passwordLabel
            Layout.leftMargin: 10
            visible: passwordText.visible
            font.pixelSize: labelFontPixelSize
            text: qsTr("Password:")
        }
        TextField {
            id: passwordText
            Layout.fillWidth: true
            Layout.leftMargin: 10
            Layout.rightMargin: 25
            Layout.preferredHeight: 25
            echoMode: TextInput.Password
            font.pixelSize: labelFontPixelSize
            Keys.onEnterPressed: accept()
            Keys.onReturnPressed: accept()
        }
    }

    CButton {
        id: cancelButton
        anchors.right: okButton.left
        anchors.bottom: okButton.bottom
        anchors.rightMargin: 10
        width: 110
        font.pixelSize: buttonFontPixelSize
        text: qsTr("Cancel")
        onClicked: cancel()
    }
    CButton {
        id: okButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 15
        anchors.bottomMargin: 10
        width: 110
        font.pixelSize: buttonFontPixelSize
        text: qsTr("OK")
        onClicked: accept()
    }
}
