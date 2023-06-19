import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import QtQuick.Window 2.12

Window {
    id: root

    property bool secured: false
    property alias userNeeded: userText.visible
    property alias passwordNeeded: passwordText.visible

    width: container.childrenRect.width
    height: container.childrenRect.height
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint
    modality: Qt.ApplicationModal
    title: qsTr("VNC authentication")

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

    ColumnLayout {
        id: container
        spacing: 0

        Rectangle {
            id: statusArea
            Layout.fillWidth: true
            Layout.preferredHeight: statusContent.height + 6
            color: "#ffff0000"

            RowLayout {
                id: statusContent
                y: 3
                anchors.horizontalCenter: statusArea.horizontalCenter
                spacing: 3
                Image {
                    id: statusIcon
                    Layout.topMargin: 3
                    Layout.preferredWidth: statusMessage.implicitHeight
                    Layout.preferredHeight: statusMessage.implicitHeight
                    fillMode: Image.PreserveAspectFit
                    source: "qrc:/insecure.svg"
                }
                Text {
                    id: statusMessage
                    text: qsTr("This connection is not secure")
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
                        target: statusIcon
                        source: "qrc:/secure.svg"
                    }
                    PropertyChanges {
                        target: statusMessage
                        text: qsTr("This connection is secure")
                    }
                }
            ]
        }

        RowLayout {
            spacing: 0

            Image {
                id: authIcon
                Layout.leftMargin: 15
                Layout.topMargin: 20
                Layout.alignment: Qt.AlignVCenter
                source: "image://qticons/SP_MessageBoxQuestion"
            }
            GridLayout {
                Layout.leftMargin: 10
                Layout.rightMargin: 10
                Layout.topMargin: 20
                Layout.bottomMargin: 20
                columns: 2
                rows: 2
                rowSpacing: 3
                columnSpacing: 5

                Text {
                    id: userLabel
                    Layout.leftMargin: 10
                    visible: userText.visible
                    text: qsTr("Username:")
                }
                TextField {
                    id: userText
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 25
                    Layout.preferredHeight: 25
                    Layout.minimumWidth: 200
                }
                Text {
                    id: passwordLabel
                    Layout.leftMargin: 10
                    visible: passwordText.visible
                    text: qsTr("Password:")
                }
                TextField {
                    id: passwordText
                    Layout.fillWidth: true
                    Layout.leftMargin: 10
                    Layout.rightMargin: 25
                    Layout.preferredHeight: 25
                    Layout.minimumWidth: 200
                    echoMode: TextInput.Password
                    Keys.onEnterPressed: accept()
                    Keys.onReturnPressed: accept()
                    Keys.onEscapePressed: cancel()
                }
            }
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight
            Button {
                id: cancelButton
                Layout.bottomMargin: 10
                Layout.topMargin: 5
                Layout.preferredWidth: 110
                text: qsTr("Cancel")
                onClicked: cancel()
            }
            Button {
                id: okButton
                Layout.leftMargin: 10
                Layout.rightMargin: 15
                Layout.topMargin: 5
                Layout.bottomMargin: 10
                Layout.preferredWidth: 110
                text: qsTr("OK")
                onClicked: accept()
            }
        }
    }

    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: cancel()
    }
}
