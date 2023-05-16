import QtQuick 2.12
import Qt.TigerVNC 1.0

Item {
    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14

    function alert(message) {
        alertDialog.text = message
        alertDialog.open()
    }

    Component.onCompleted: serverDialog.visible = true
    Connections {
        target: AppManager
        function onErrorOcurred(seq, message, quit) {
            alertDialog.text = message
            alertDialog.quit = quit
            alertDialog.open()
        }

        function onVncWindowOpened() {
            serverDialog.visible = false
        }

        function onInfoDialogRequested() {
            infoDialog.open();
        }

        function onOptionDialogRequested() {
            optionDialog.open();
        }

        function onAboutDialogRequested() {
            aboutDialog.open();
        }

        onErrorOcurred: onErrorOcurred(seq, message, quit)
        onVncWindowOpened: onVncWindowOpened()
        onInfoDialogRequested: onInfoDialogRequested()
        onOptionDialogRequested: onOptionDialogRequested()
        onAboutDialogRequested: onAboutDialogRequested()
    }

    ServerDialog {
        id: serverDialog
        onOptionDialogRequested: optionDialog.open()
        onAboutDialogRequested: aboutDialog.open()
    }

    Loader {
        active: false
        AlertDialog {
            id: alertDialog
        }
    }

    Loader {
        active: false
        InfoDialog {
            id: infoDialog
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
}
