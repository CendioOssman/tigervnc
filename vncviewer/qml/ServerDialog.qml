import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: serverDialog
    width: container.childrenRect.width
    height: container.childrenRect.height
    maximumWidth: width
    maximumHeight: height
    minimumWidth: width
    minimumHeight: height
    visible: !Config.listenModeEnabled
    title: qsTr("VNC Viewer: Connection Details")

    property var servers: []

    signal optionDialogRequested()
    signal aboutDialogRequested()

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

    function createServerList() {
        servers = []
        for (var i = 0; i < Config.serverHistory.length; i++) {
            servers.push(Config.serverHistory[i])
        }
        serversChanged()
    }

    Connections {
        target: AppManager

        function onCredentialRequested(secured, userNeeded, passwordNeeded) {
            authDialog.secured = secured
            authDialog.userNeeded = userNeeded
            authDialog.passwordNeeded = passwordNeeded
            authDialog.open()
        }

        onCredentialRequested: onCredentialRequested(secured, userNeeded, passwordNeeded)
    }

    Connections {
        target: Config

        function onServerHistoryChanged(serverList = []) {
            createServerList()
        }

        onServerHistoryChanged: onServerHistoryChanged()
    }

    ColumnLayout {
        id: container
        spacing: 0

        RowLayout {
            Text {
                id: addressLabel
                Layout.leftMargin: 15
                Layout.topMargin: 15
                text: qsTr("VNC server:")
            }

            ComboBox {
                id: addressInput
                Layout.leftMargin: 5
                Layout.rightMargin: 15
                Layout.topMargin: 15
                Layout.minimumWidth: 350
                editable: true
                model: servers
                onAccepted: validateServerText(editText)
                Component.onCompleted: createServerList()
            }
        }

        RowLayout {
            Button {
                id: optionsButton
                Layout.leftMargin: 15
                Layout.topMargin: 10
                text: qsTr("Options...")
                onClicked: optionDialogRequested()
            }

            Button {
                id: loadButton
                Layout.leftMargin: 10
                Layout.topMargin: 10
                text: qsTr("Load...")
                onClicked: configLoadDialog.open()
            }

            Button {
                id: saveAsButton
                Layout.leftMargin: 10
                Layout.topMargin: 10
                text: qsTr("Save As...")
                onClicked: configSaveDialog.open()
            }
        }

        Rectangle {
            id: separator1
            Layout.topMargin: 10
            Layout.fillWidth: true
            height: 1
            color: "#ff848484"
        }

        Rectangle {
            id: separator2
            Layout.fillWidth: true
            height: 1
            color: "#fff1f1f1"
        }

        RowLayout {
            Button {
                id: aboutButton
                Layout.leftMargin: 15
                Layout.topMargin: 10
                Layout.bottomMargin: 15
                text: qsTr("About...")
                onClicked: aboutDialogRequested()
            }

            Button {
                id: cancelButton
                Layout.leftMargin: 80
                Layout.rightMargin: 15
                Layout.topMargin: 10
                Layout.bottomMargin: 15
                text: qsTr("Cancel")
                onClicked: Qt.quit()
            }

            Button {
                id: connectButton
                Layout.rightMargin: 15
                Layout.topMargin: 10
                Layout.bottomMargin: 15
                enabled: addressInput.currentText.length > 0
                text: qsTr("Connect")
                onClicked: AppManager.connectToServer(addressInput.currentText)
            }
        }
    }

    Loader {
        active: false
        AuthDialog {
            id: authDialog
            onCommit: authenticate(user, password)
            onAbort: AppManager.resetConnection()
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
