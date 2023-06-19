import QtQuick 2.12
import Qt.TigerVNC 1.0

Item {
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

        function onMessageDialogRequested(flags, title, text) {
            messageDialog.setting = flags
            messageDialog.title = title
            messageDialog.text = text
            messageDialog.open()
        }

        function onCredentialRequested(secured, userNeeded, passwordNeeded) {
            authDialog.secured = secured
            authDialog.userNeeded = userNeeded
            authDialog.passwordNeeded = passwordNeeded
            authDialog.open()
        }

        onErrorOcurred: onErrorOcurred(seq, message, quit)
        onVncWindowOpened: onVncWindowOpened()
        onInfoDialogRequested: onInfoDialogRequested()
        onOptionDialogRequested: onOptionDialogRequested()
        onAboutDialogRequested: onAboutDialogRequested()
        onMessageDialogRequested: onMessageDialogRequested(flags, title, text)
        onServerHistoryChanged: onServerHistoryChanged()
        onCredentialRequested: onCredentialRequested(secured, userNeeded, passwordNeeded)
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

    Loader {
        active: false
        MessageDialog {
            id: messageDialog
        }
    }

    Loader {
        active: false
        AuthDialog {
            id: authDialog
            onCommit: AppManager.authenticate(user, password)
            onAbort: AppManager.cancelAuth()
        }
    }
}
