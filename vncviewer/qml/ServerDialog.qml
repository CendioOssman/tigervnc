import QtQml 2.12
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Window 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: serverDialog
    width: container.implicitWidth
    height: container.implicitHeight
    flags: Qt.Window | Qt.CustomizeWindowHint | Qt.WindowTitleHint | Qt.WindowMinimizeButtonHint | Qt.WindowSystemMenuHint | Qt.WindowCloseButtonHint
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

    function validateServerText(serverText) {
        var index = -1
        if (addressInput.indexOfValue !== undefined) {
            index = addressInput.indexOfValue(serverText)
        }
        else { // For Qt 5.12, which does not have indexOfValue().
            for (var is = 0; is < servers.length; is++) {
                if (servers[is] === serverText) {
                    index = is
                    break
                }
            }
        }

        if (index >= 0) {
            addressInput.currentIndex = index
        }
        else {
            var servers0 = servers
            servers = []
            servers.push(serverText)
            for (var i = 0; i < servers0.length; i++) {
                servers.push(servers0[i])
            }
            Config.serverHistory = servers
        }
    }

    function createServerList() {
        servers = []
        for (var i = 0; i < Config.serverHistory.length; i++) {
            servers.push(Config.serverHistory[i])
        }
        serversChanged()
    }

    function accept()  {
        validateServerText(addressInput.editText)
        AppManager.connectToServer(addressInput.currentText)
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
                Layout.fillWidth: true
                editable: true
                model: servers
                focus: true
                //selectTextByMouse: true // CAVEAT: This line must be commented out when compiling with Qt5.12.
                onAccepted: validateServerText(editText)
                Component.onCompleted: {
                    createServerList()
                    if (Config.qtVersionMajor === 5 && Config.qtVersionMinor >= 15) {
                        addressInput.selectTextByMouse = true
                    }
                }
                Keys.onEnabledChanged: accept()
                Keys.onReturnPressed: accept()
                Keys.onEscapePressed: Qt.quit()
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
                Layout.leftMargin: 30
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
                onClicked: accept()
            }
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

    Shortcut {
        sequence: "Enter"
        onActivated: accept()
    }
    Shortcut {
        sequence: "Return"
        onActivated: accept()
    }
    Shortcut {
        sequence: StandardKey.Cancel
        onActivated: Qt.quit()
    }
}
