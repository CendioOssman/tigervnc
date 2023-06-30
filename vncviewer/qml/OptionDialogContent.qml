import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Item {
    id: root

    objectName: "OptionDialogContent"
    property bool screenLaid: false
    property var screenSelectionButtons: []
    property var selectedScreens: []
    property var optionLabels: []
    property var optionIndices: []

    width: categoryContainer.width + contentStack.width
    height: contentStack.height + buttonBox.height

    signal closeRequested()

    function close() {
        closeRequested()
        AppManager.closeOverlay()
    }

    function commit() {
        Config.autoSelect = compressionAutoSelect.checked
        if (compressionEncodingTight.checked) { Config.preferredEncodingNum = 7 }
        if (compressionEncodingZRLE.checked) { Config.preferredEncodingNum = 16 }
        if (compressionEncodingHextile.checked) { Config.preferredEncodingNum = 5 }
        if (compressionEncodingH264.checked) { Config.preferredEncodingNum = 50 }
        if (compressionEncodingRaw.checked) { Config.preferredEncodingNum = 0 }
        Config.fullColour = compressionColorLevelFull.checked
        if (compressionColorLevelMedium.checked) { Config.lowColourLevel = 2 }
        if (compressionColorLevelLow.checked) { Config.lowColourLevel = 1 }
        if (compressionColorLevelVeryLow.checked) { Config.lowColourLevel = 0 }
        Config.customCompressLevel = compressionCustomCompressionLevel.checked
        Config.compressLevel = compressionCustomCompressionLevelTextEdit.text
        Config.noJpeg = !compressionJPEGCompression.checked
        Config.qualityLevel = compressionJPEGCompressionTextEdit.text
        //
        Config.encNone = securityEncryptionNone.checked
        Config.encTLSAnon = securityEncryptionTLSWithAnonymousCerts.checked
        Config.encTLSX509 = securityEncryptionTLSWithX509Certs.checked
        Config.x509CA = securityEncryptionTLSWithX509CATextEdit.text
        Config.x509CRL = securityEncryptionTLSWithX509CRLTextEdit.text
        Config.authNone = securityAuthenticationNone.checked
        Config.authVNC = securityAuthenticationStandard.checked
        Config.authPlain = securityAuthenticationUsernameAndPassword.checked
        Config.encAES = securityEncryptionAES.checked
        //
        Config.viewOnly = inputViewOnly.checked
        Config.emulateMiddleButton = inputMouseEmulateMiddleButton.checked
        Config.dotWhenNoCursor = inputMouseShowDot.checked
        Config.fullscreenSystemKeys = inputKeyboardPassSystemKeys.checked
        Config.menuKey = inputKeyboardMenuKeyCombo.currentText
        Config.acceptClipboard = inputClipboardFromServer.checked
        Config.sendClipboard = inputClipboardToServer.checked
        //
        // Update fullscreen properties. Note that Config.fullScreen must be updated at the last, because changing it causes immediate fullscreening, so
        // all the fullscreen properties must be updated before updating Config.fullScreen.
        var selectedScreens = []
        for (var bix = 0; bix < screenSelectionButtons.length; bix++) {
            if (screenSelectionButtons[bix].checked) {
                selectedScreens.push(bix + 1)  // selected screen ID is 1-origin.
            }
        }
        var selectedScreensChanged = !equals(Config.selectedScreens, selectedScreens)
        Config.selectedScreens = selectedScreens
        if (displayWindowed.checked) {
            Config.fullScreen = false
        }
        else {
            var newFullScreenMode = displayFullScreenOnAllMonitors.checked ? Config.FSAll : displayFullScreenOnSelectedMonitors.checked ? Config.FSSelected : Config.FSCurrent
            if (!Config.fullScreen || newFullScreenMode !== Config.fullScreenMode || (newFullScreenMode === Config.FSSelected && (Config.fullScreenMode !== Config.FSSelected || selectedScreensChanged))) {
                Config.fullScreenMode = newFullScreenMode
                // To reconfigure the fullscreen mode, set Config.fullScreen to false once, then set it to true.
                Config.fullScreen = false
                Config.fullScreen = true
            }
        }
        //
        Config.shared = miscShared.checked
        Config.reconnectOnError = miscReconnectQuery.checked
        close()
    }

    function equals(a1, a2) {
        if (a1.length !== a2.length) {
            return false
        }
        for (var i = 0; i < a1.length; i++) {
            if (a1[i] !== a2[i]) {
                return false
            }
        }

        return true
    }

    function reset() {
        compressionAutoSelect.checked = Config.autoSelect
        compressionEncodingTight.checked = Config.preferredEncodingNum === 7
        compressionEncodingZRLE.checked = Config.preferredEncodingNum === 16
        compressionEncodingHextile.checked = Config.preferredEncodingNum === 5
        compressionEncodingH264.checked = Config.preferredEncodingNum === 50
        compressionEncodingRaw.checked = Config.preferredEncodingNum === 0
        compressionColorLevelFull.checked = Config.fullColour
        compressionColorLevelMedium.checked = Config.lowColourLevel === 2
        compressionColorLevelLow.checked = Config.lowColourLevel === 1
        compressionColorLevelVeryLow.checked = Config.lowColourLevel === 0
        compressionCustomCompressionLevel.checked = Config.customCompressLevel
        compressionCustomCompressionLevelTextEdit.text = Config.compressLevel
        compressionJPEGCompression.checked = !Config.noJpeg
        compressionJPEGCompressionTextEdit.text = Config.qualityLevel
        //
        securityEncryptionNone.checked = Config.encNone
        securityEncryptionTLSWithAnonymousCerts.checked = Config.encTLSAnon
        securityEncryptionTLSWithX509Certs.checked = Config.encTLSX509
        securityEncryptionTLSWithX509CATextEdit.text = Config.x509CA
        securityEncryptionTLSWithX509CRLTextEdit.text = Config.x509CRL
        securityAuthenticationNone.checked = Config.authNone
        securityAuthenticationStandard.checked = Config.authVNC
        securityAuthenticationUsernameAndPassword.checked = Config.authPlain
        securityEncryptionAES.checked = Config.encAES
        //
        inputViewOnly.checked = Config.viewOnly
        inputMouseEmulateMiddleButton.checked = Config.emulateMiddleButton
        inputMouseShowDot.checked = Config.dotWhenNoCursor
        inputKeyboardPassSystemKeys.checked = Config.fullscreenSystemKeys
        inputKeyboardMenuKeyCombo.currentIndex = Config.menuKeyIndex
        inputClipboardFromServer.checked = Config.acceptClipboard
        inputClipboardToServer.checked = Config.sendClipboard
        //
        displayWindowed.checked = !Config.fullScreen
        displayFullScreenOnCurrentMonitor.checked = Config.fullScreen && Config.fullScreenMode === Config.FSCurrent
        displayFullScreenOnAllMonitors.checked = Config.fullScreen && Config.fullScreenMode === Config.FSAll
        displayFullScreenOnSelectedMonitors.checked = Config.fullScreen && Config.fullScreenMode === Config.FSSelected
        //
        miscShared.checked = Config.shared
        miscReconnectQuery.checked = Config.reconnectOnError
        selectedScreens = Config.selectedScreens
    }

    function layoutScreens() {
        if (screenLaid) {
            return
        }
        screenLaid = true

        //displaySelectionView
        var ratio = 0.8
        var availableWidth = displaySelectionView.width * ratio
        var availableHeight = displaySelectionView.height * ratio
        var originX = (displaySelectionView.width - availableWidth) / 2
        var originY = (displaySelectionView.height - availableHeight) / 2
        //console.log("rectWxH= " + displaySelectionView.width + "x" + displaySelectionView.height + ", originXY=(" + originX + "," + originY + "), available WxH=" + availableWidth + "x" + availableHeight)

        var minX = 0
        var maxX = 0
        var minY = 0
        var maxY = 0
        var screens = Qt.application.screens
        for (var i = 0; i < screens.length; i++) {
            var s = screens[i]
            var x = s.virtualX
            var y = s.virtualY
            var w = s.width
            var h = s.height
            if (minX > x) {
                minX = x
            }
            if (maxX < x + w) {
                maxX = x + w
            }
            if (minY > y) {
                minY = y
            }
            if (maxY < y + h) {
                maxY = y + h
            }
            //console.log("screen[" + i + "] x=" + x + ",y=" + y + ",w=" + w + ",h=" +h)
        }
        //console.log("minX=" + minX + ",minY=" + minY + ",maxX=" + maxX + ",maxY=" + maxY) // = -1024 0 1920 1280

        for (var k = 0; k < screenSelectionButtons.length; k++) {
            screenSelectionButtons[k].destroy()
        }
        screenSelectionButtons = []

        var width = maxX - minX
        var height = maxY - minY
        for (var j = 0; j < screens.length; j++) {
            var sc = screens[j]
            var rx = (sc.virtualX - minX) / width
            var ry = (sc.virtualY - minY) / height
            var rw = sc.width / width
            var rh = sc.height / height
            //console.log("screen[" + j + "] rx=" + rx + ",ry=" + ry + ",rw=" + rw + ",lh=" +rh)
            var lx = rx * availableWidth + originX
            var ly = ry * availableHeight + originY
            var lw = rw * availableWidth
            var lh = rh * availableHeight
            //console.log("screen[" + j + "] lx=" + lx + ",ly=" + ly + ",lw=" + lw + ",lh=" +lh)
            var button = screenButtonComponent.createObject(displaySelectionView, { x: lx, y: ly, width: lw, height: lh, checkable: true })
            var handler = selectionHandlerFactory.createObject(this, { buttonIndex: j })
            button.toggled.connect(handler.validateDisplaySelection)
            screenSelectionButtons.push(button)
            if (!Config.canFullScreenOnMultiDisplays) {
                displaySelectionButtonGroup.addButton(button)
            }
        }

        for (var bix = 0; bix < screenSelectionButtons.length; bix++) {
            screenSelectionButtons[bix].checked = false
        }
        for (var scix = 0; scix < selectedScreens.length; scix++) {
            var index = selectedScreens[scix] - 1  // selected screen ID is 1-origin.
            screenSelectionButtons[index].checked = true
            if (!Config.canFullScreenOnMultiDisplays) {
                break
            }
        }
    }

    Component {
        id: selectionHandlerFactory
        QtObject {
            property int buttonIndex
            // Ensure the number of selected screens never becomes zero.
            function validateDisplaySelection() {
                var found = false
                var currentButtonIndex = 0
                for (var i = 0; i < screenSelectionButtons.length; i++) {
                    if (screenSelectionButtons[i].checked) {
                        found = true
                    }
                }
                if (!found) {
                    screenSelectionButtons[buttonIndex].checked = true
                }
            }
        }
    }

    Component {
        id: screenButtonComponent
        Button {
            id: button

            property alias source: icon.source

            width: 116
            height: 26
            background: Rectangle {
                id: bgrect
                radius: 3
                border.width: button.focus ? 2 : 1
                border.color: "#ff6e6e6e"
                color: "#ffdcdcdc"
            }
            Image {
                id: icon
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
            }

            states: [
                State {
                    when: !button.checkable && button.pressed
                    PropertyChanges {
                        target: bgrect
                        color: "#ff808080"
                    }
                },
                State {
                    when: button.checkable && button.checked && button.enabled
                    PropertyChanges {
                        target: bgrect
                        color: "#ff5454ff"
                    }
                },
                State {
                    when: button.checkable && !button.checked
                    PropertyChanges {
                        target: bgrect
                        color: "#fff3f3f3"
                    }
                },
                State {
                    when: button.checkable && button.checked && !button.enabled
                    PropertyChanges {
                        target: bgrect
                        color: "#ffafafe7"
                    }
                }
            ]
        }
    }

    function reportScreenSelection() { // For debugging purpose.
        for (var i = 0; i < screenSelectionButtons.length; i++) {
            console.log("screenSelectionButtons[" + i + "]=" + screenSelectionButtons[i].checked)
        }
    }

    function createOptionModel() {
        var ix = 0
        optionLabels = []
        optionIndices = []
        optionLabels.push(qsTr("Compression"))
        optionIndices.push(ix++)
        if (Config.haveGNUTLS || Config.haveNETTLE) {
            optionLabels.push(qsTr("Security"))
            optionIndices.push(ix++)
        }
        else {
            ix++
        }
        optionLabels.push(qsTr("Input"))
        optionIndices.push(ix++)
        optionLabels.push(qsTr("Display"))
        optionIndices.push(ix++)
        optionLabels.push(qsTr("Miscellaneous"))
        optionIndices.push(ix++)
        optionLabelsChanged()
        optionIndicesChanged()
    }

    Component.onCompleted: {
        createOptionModel()
        reset()
    }

    Rectangle {
        id: categoryContainer
          anchors.right: contentStack.left
          anchors.top: contentStack.top
          anchors.bottom: contentStack.bottom
        width: 145

        ListView {
            id: categoryList
            anchors.fill: parent
            model: optionLabels
            delegate: ItemDelegate {
                width: categoryList.width
                contentItem: Label {
                    anchors.fill: parent
                    text: modelData
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    MouseArea {
                        anchors.fill: parent
                        onClicked: categoryList.currentIndex = index
                    }
                }
                highlighted: categoryList.currentIndex - index === 0
            }
        }
    }

    StackLayout {
        id: contentStack
        width: childrenRect.width
        height: childrenRect.height
        x: categoryContainer.width
        currentIndex: optionIndices[categoryList.currentIndex]
        onCurrentIndexChanged: reportScreenSelection()
        Rectangle {
            id: compressionTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ColumnLayout {
                id: compressionContainer
                x: 10
                y: 10
                spacing: 5
                CheckBox {
                    id: compressionAutoSelect
                    Layout.preferredWidth: compressionAutoSelectEditor.implicitWidth
                    font.bold: true
                    text: qsTr("Auto select")
                }
                GridLayout {
                    id: compressionAutoSelectEditor
                    enabled: !compressionAutoSelect.checked
                    rows: 2
                    columns: 2
                    rowSpacing: 0
                    columnSpacing: 40
                    Label {
                        id: preferredEncodingLabel
                        Layout.leftMargin: 6
                        Layout.preferredWidth: Math.max(preferredEncodingLabel.implicitWidth, colorLevelLabel.implicitWidth)
                        Layout.alignment: Qt.AlignLeft
                        font.bold: true
                        text: qsTr("Preferred encoding")
                    }
                    Label {
                        id: colorLevelLabel
                        Layout.leftMargin: 6
                        Layout.preferredWidth: Math.max(preferredEncodingLabel.implicitWidth, colorLevelLabel.implicitWidth)
                        Layout.alignment: Qt.AlignLeft
                        font.bold: true
                        text: qsTr("Color level")
                    }
                    ColumnLayout {
                        id: compressionEncodingTypes
                        Layout.leftMargin: 12
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                        RadioButton {
                            id: compressionEncodingTight
                            text: qsTr("Tight")
                        }
                        RadioButton {
                            id: compressionEncodingZRLE
                            text: qsTr("ZRLE")
                        }
                        RadioButton {
                            id: compressionEncodingHextile
                            text: qsTr("Hextile")
                        }
                        RadioButton {
                            id: compressionEncodingH264
                            visible: Config.haveH264
                            text: qsTr("H.264")
                        }
                        RadioButton {
                            id: compressionEncodingRaw
                            text: qsTr("Raw")
                        }
                    }
                    ColumnLayout {
                        id: compressionColorLevels
                        Layout.leftMargin: 12
                        Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                        RadioButton {
                            id: compressionColorLevelFull
                            text: qsTr("Full")
                        }
                        RadioButton {
                            id: compressionColorLevelMedium
                            text: qsTr("Medium")
                        }
                        RadioButton {
                            id: compressionColorLevelLow
                            text: qsTr("Low")
                        }
                        RadioButton {
                            id: compressionColorLevelVeryLow
                            text: qsTr("Very low")
                        }
                    }
                    ButtonGroup { buttons: compressionEncodingTypes.children }
                    ButtonGroup { buttons: compressionColorLevels.children }
                }
                CheckBox {
                    id: compressionCustomCompressionLevel
                    Layout.preferredWidth: compressionAutoSelectEditor.implicitWidth
                    Layout.leftMargin: 6
                    font.bold: true
                    text: qsTr("Custom compression level:")
                }
                RowLayout {
                    id: compressionCustomCompressionLevelEditor
                    Layout.leftMargin: 24
                    enabled: compressionCustomCompressionLevel.checked
                    spacing: 2
                    TextField {
                        id: compressionCustomCompressionLevelTextEdit
                        Layout.preferredWidth: 24
                        selectByMouse: true
                        validator: IntValidator {
                            bottom: 0
                            top: 9
                        }
                    }
                    Label {
                        text: qsTr("level (0=fast, 9=best)")
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                CheckBox {
                    id: compressionJPEGCompression
                    Layout.preferredWidth: compressionAutoSelectEditor.implicitWidth
                    Layout.leftMargin: 6
                    font.bold: true
                    text: qsTr("Allow JPEG compression:")
                }
                RowLayout {
                    id: compressionJPEGCompressionEditor
                    enabled: compressionJPEGCompression.checked
                    spacing: 2
                    Layout.leftMargin: 24
                    TextField {
                        id: compressionJPEGCompressionTextEdit
                        Layout.preferredWidth: 24
                        selectByMouse: true
                        validator: IntValidator {
                            bottom: 0
                            top: 9
                        }
                    }
                    Label {
                        text: qsTr("quality (0=poor, 9=best)")
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Rectangle {
            id: securityTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ColumnLayout {
                id: securityContainer                
                x: 10
                y: 10
                spacing: 0
                visible: Config.haveGNUTLS || Config.haveNETTLE
                Text {
                    font.bold: true
                    text: qsTr("Encryption")
                }
                CheckBox {
                    id: securityEncryptionNone
                    Layout.leftMargin: 10
                    text: qsTr("None")
                }
                CheckBox {
                    id: securityEncryptionTLSWithAnonymousCerts
                    Layout.leftMargin: 10
                    visible: Config.haveGNUTLS
                    text: qsTr("TLS with anonymous certificates")
                }
                CheckBox {
                    id: securityEncryptionTLSWithX509Certs
                    Layout.leftMargin: 10
                    visible: Config.haveGNUTLS
                    text: qsTr("TLS with X509 certificates")
                }
                Label {
                    Layout.leftMargin: 40
                    visible: Config.haveGNUTLS
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    text: qsTr("Path to X509 CA certificate")
                }
                TextField {
                    id: securityEncryptionTLSWithX509CATextEdit
                    Layout.preferredWidth: 250
                    Layout.leftMargin: 44
                    visible: Config.haveGNUTLS
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    selectByMouse: true
                }
                Label {
                    Layout.leftMargin: 40
                    visible: Config.haveGNUTLS
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    text: qsTr("Path to X509 CRL file")
                }
                TextField {
                    id: securityEncryptionTLSWithX509CRLTextEdit
                    Layout.preferredWidth: 250
                    Layout.leftMargin: 44
                    visible: Config.haveGNUTLS
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    selectByMouse: true
                }
                CheckBox {
                    id: securityEncryptionAES
                    Layout.leftMargin: 10
                    visible: Config.haveNETTLE
                    text: qsTr("RSA-AES")
                }
                Text {
                    Layout.topMargin: 10
                    font.bold: true
                    text: qsTr("Authentication")
                }
                CheckBox {
                    id: securityAuthenticationNone
                    Layout.leftMargin: 10
                    text: qsTr("None")
                }
                CheckBox {
                    id: securityAuthenticationStandard
                    Layout.leftMargin: 10
                    text: qsTr("Standard VNC (insecure without encryption)")
                }
                CheckBox {
                    id: securityAuthenticationUsernameAndPassword
                    Layout.leftMargin: 10
                    text: qsTr("Username and password (insecure without encryption)")
                }
            }
        }
        Rectangle {
            id: inputTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ColumnLayout {
                id: inputContainer
                x: 10
                y: 10
                spacing: 0
                CheckBox {
                    id: inputViewOnly
                    text: qsTr("View only (ignore mouse and keyboard)")
                }
                Text {
                    Layout.leftMargin: 5
                    Layout.topMargin: 10
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Mouse")
                }
                CheckBox {
                    id: inputMouseEmulateMiddleButton
                    Layout.leftMargin: 20
                    text: qsTr("Emulate middle mouse button")
                }
                CheckBox {
                    id: inputMouseShowDot
                    Layout.leftMargin: 20
                    text: qsTr("Show dot when no cursor")
                }
                Text {
                    Layout.leftMargin: 5
                    Layout.topMargin: 10
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Keyboard")
                }
                CheckBox {
                    id: inputKeyboardPassSystemKeys
                    Layout.leftMargin: 20
                    text: qsTr("Pass system keys directly to server (full screen)")
                }
                RowLayout {
                    Layout.leftMargin: 20
                    spacing: 3
                    Text {
                        text: qsTr("Menu key")
                    }
                    ComboBox {
                        id: inputKeyboardMenuKeyCombo
                        Layout.preferredWidth: 90
                        model: Config.menuKeys
                    }
                }
                Text {
                    Layout.leftMargin: 5
                    Layout.topMargin: 10
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Clipboard")
                }
                CheckBox {
                    id: inputClipboardFromServer
                    Layout.leftMargin: 20
                    text: qsTr("Accept clipboard from server")
                }
                CheckBox {
                    id: inputClipboardToServer
                    Layout.leftMargin: 20
                    text: qsTr("Send clipboard to server")
                }
            }
        }
        Rectangle {
            id: displayTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ButtonGroup { id: displayButtonGroup }
            ColumnLayout {
                id: displayDescContainer
                x: 10
                y: 10
                spacing: 0
                Text {
                    id: displayDesc
                    Layout.leftMargin: 12
                    font.bold: true
                    text: qsTr("Display mode")
                }
                RadioButton {
                    id: displayWindowed
                    Layout.leftMargin: 12
                    ButtonGroup.group: displayButtonGroup
                    text: qsTr("Windowed")
                }
                RadioButton {
                    id: displayFullScreenOnCurrentMonitor
                    Layout.leftMargin: 12
                    ButtonGroup.group: displayButtonGroup
                    text: qsTr("Full screen on current monitor")
                }
                RadioButton {
                    id: displayFullScreenOnAllMonitors
                    Layout.leftMargin: 12
                    ButtonGroup.group: displayButtonGroup
                    enabled: Config.canFullScreenOnMultiDisplays
                    text: qsTr("Full screen on all monitors")
                }
                RadioButton {
                    id: displayFullScreenOnSelectedMonitors
                    Layout.leftMargin: 12
                    ButtonGroup.group: displayButtonGroup
                    text: qsTr("Full screen on selected monitor(s)")
                }
                Rectangle {
                    id: displaySelectionView
                    Layout.preferredWidth: 300
                    Layout.preferredHeight: 150
                    Layout.leftMargin: 35
                    Layout.rightMargin: 10
                    Layout.topMargin: 5
                    enabled: displayFullScreenOnSelectedMonitors.checked
                    color: "#ffe7e7e7"
                    border.width: 1
                    border.color: "#ff737373"
                    onWidthChanged: {
                        if (width > 0) {
                            layoutScreens()
                        }
                    }
                }
                ButtonGroup { id: displaySelectionButtonGroup }
            }
        }
        Rectangle {
            id: miscTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ColumnLayout {
                id: miscContainer
                x: 10
                y: 10
                CheckBox {
                    id: miscShared
                    text: qsTr("Shared (don't disconnect other viewers)")
                }
                CheckBox {
                    id: miscReconnectQuery
                    text: qsTr("Ask to reconnect on connection errors")
                }
            }
        }
    }

    Rectangle {
        id: buttonBox
        anchors.left: contentStack.left
        anchors.right: contentStack.right
        anchors.top: contentStack.bottom
        height: 52

        Button {
            id: cancelButton
            anchors.right: okButton.left
            anchors.bottom: okButton.bottom
            anchors.rightMargin: 15
            text: qsTr("Cancel")
            onClicked: close()
        }
        Button {
            id: okButton
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.rightMargin: 15
            anchors.bottomMargin: 10
            text: qsTr("OK")
            onClicked: commit()
        }
    }
}
