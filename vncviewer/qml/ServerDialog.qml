import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15
import Qt.TigerVNC 1.0

Window {
    id: serverDialog
    width: 450
    height: 140
    maximumWidth: width
    maximumHeight: height
    minimumWidth: width
    minimumHeight: height
    visible: true
    title: qsTr("VNC Viewer: Connection Details")

    property var servers: []

    function loadConfig(url) {
        var server = Config.loadViewerParameters(Config.toLocalFile(url))
        validateServerText(server)
    }

    function saveConfig(url) {
        Config.saveViewerParameters(Config.toLocalFile(url), addressInput.currentText)
    }

    function authenticate(user, password) {
        authDialog.close()
        AppManager.authenticate(user, password)
    }

    function validateServerText(serverText) {
        var index = addressInput.indexOfValue(serverText)
        if (index >= 0) {
            addressInput.currentIndex = index
        }
        else {
            servers.push(serverText)
            serversChanged()
            addressInput.currentIndex = servers.length - 1
            Config.serverHistory = servers
        }
        //console.log("Config.serverHistory=" + Config.serverHistory)
    }

    Connections {
        target: AppManager

        function onCredentialRequested(secured, userNeeded, passwordNeeded) {
            authDialog.secured = secured
            authDialog.userNeeded = userNeeded
            authDialog.passwordNeeded = passwordNeeded
            authDialog.open()
        }
    }

    Rectangle {
        id: container
        anchors.fill: parent
        color: "#ffdcdcdc"

        Text {
            id: addressLabel
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.leftMargin: 15
            anchors.topMargin: 15
            text: qsTr("VNC server:")
            font.pixelSize: labelFontPixelSize
        }

        CComboBox {
            id: addressInput
            anchors.left: addressLabel.right
            anchors.right: parent.right
            anchors.verticalCenter: addressLabel.verticalCenter
            anchors.leftMargin: 5
            anchors.rightMargin: 15
            editable: true
            model: servers
            onTextModified: validateServerText(pendingText)
            Component.onCompleted: {
                servers = []
                for (var i = 0; i < Config.serverHistory.length; i++) {
                    servers.push(Config.serverHistory[i])
                }
                serversChanged()
            }
        }

        CButton {
            id: optionsButton
            anchors.left: parent.left
            anchors.top: addressInput.bottom
            anchors.leftMargin: 15
            anchors.topMargin: 12
            text: qsTr("Options...")
            onClicked: optionDialog.open()
        }

        CButton {
            id: loadButton
            anchors.left: optionsButton.right
            anchors.top: optionsButton.top
            anchors.leftMargin: 10
            text: qsTr("Load...")
            onClicked: configLoadDialog.open()
        }

        CButton {
            id: saveAsButton
            anchors.left: loadButton.right
            anchors.top: optionsButton.top
            anchors.leftMargin: 10
            text: qsTr("Save As...")
            onClicked: configSaveDialog.open()
        }

        Rectangle {
            id: separator1
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: optionsButton.bottom
            anchors.topMargin: 10
            height: 1
            color: "#ff848484"
        }

        Rectangle {
            id: separator2
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: separator1.bottom
            height: 1
            color: "#fff1f1f1"
        }

        CButton {
            id: aboutButton
            anchors.left: optionsButton.left
            anchors.bottom: parent.bottom
            anchors.topMargin: 16
            anchors.bottomMargin: 10
            text: qsTr("About...")
            onClicked: aboutDialog.open()
        }

        CButton {
            id: cancelButton
            anchors.right: connectButton.left
            anchors.top: aboutButton.top
            anchors.rightMargin: 15
            text: qsTr("Cancel")
            onClicked: Qt.quit()
        }

        CButton {
            id: connectButton
            anchors.right: addressInput.right
            anchors.top: aboutButton.top
            enabled: addressInput.currentText.length > 0
            text: qsTr("Connect")
            onClicked: AppManager.connectToServer(addressInput.currentValue)
        }
    }

    Loader {
        active: false
        OptionDialog {
            id: optionDialog
        }
    }

    Loader {
        active: false
        AboutDialog {
            id: aboutDialog
        }
    }

    Loader {
        active: false
        AuthDialog {
            id: authDialog
            onCommit: authenticate(user, password)
        }
    }

    Loader {
        active: false
        ConfigLoadDialog {
            id: configLoadDialog
            onAccepted: loadConfig(file)
        }
    }

    Loader {
        active: false
        ConfigSaveDialog {
            id: configSaveDialog
            onAccepted: saveConfig(file)
        }
    }
}
