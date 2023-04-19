import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Window {
    id: root

    property real labelFontPixelSize: 12
    property real buttonFontPixelSize: 14
    property bool screenLaid: false
    property var screenSelectionButtons: []
    property var selectedScreens: []

    width: 570
    height: 440
    flags: Qt.SubWindow
    modality: Qt.ApplicationModal
    title: qsTr("TigerVNC Options")
    color: "#ff8f8f8f"

    function open() {
        visible = true
    }

    function close() {
        visible = false
    }

    function commit() {
        close()
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
            if (newFullScreenMode !== Config.fullScreenMode || (newFullScreenMode === Config.FSSelected && (Config.fullScreenMode !== Config.FSSelected || selectedScreensChanged))) {
                Config.fullScreenMode = newFullScreenMode
                // To reconfigure the fullscreen mode, set Config.fullScreen to false once, then set it to true.
                Config.fullScreen = false
                Config.fullScreen = true
            }
        }
        //
        Config.shared = miscShared.checked
        Config.reconnectOnError = miscReconnectQuery.checked
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
            var comp = Qt.createComponent("CButton.qml", Component.PreferSynchronous)
            var button = comp.createObject(displaySelectionView, { x: lx, y: ly, width: lw, height: lh, checkable: true })
            var handler = selectionHandlerFactory.createObject(this, { buttonIndex: j })
            button.toggled.connect(handler.validateDisplaySelection)
            screenSelectionButtons.push(button)
        }

        for (var bix = 0; bix < screenSelectionButtons.length; bix++) {
            screenSelectionButtons[bix].checked = false
        }
        for (var scix = 0; scix < selectedScreens.length; scix++) {
            var index = selectedScreens[scix] - 1  // selected screen ID is 1-origin.
            screenSelectionButtons[index].checked = true
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

    function reportScreenSelection() { // For debugging purpose.
        for (var i = 0; i < screenSelectionButtons.length; i++) {
            console.log("screenSelectionButtons[" + i + "]=" + screenSelectionButtons[i].checked)
        }
    }

    onVisibleChanged: {
        if (visible) {
            reset()
        }
    }

    Rectangle {
        id: categoryContainer
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: buttonBox.top
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        width: 145
        color: "#ffffffff"

        ListView {
            id: categoryList
            anchors.fill: parent
            model: [ qsTr("Compression"), qsTr("Security"), qsTr("Input"), qsTr("Display"), qsTr("Miscellaneous"), ]
            delegate: ItemDelegate {
                width: categoryList.width
                contentItem: Label {
                    anchors.fill: parent
                    text: modelData
                    font.pixelSize: labelFontPixelSize
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    color: highlighted ? "#ffffffff" : "#ff000000"
                    background: Rectangle {
                        color: highlighted ? "#ff000080" : "#ffffffff"
                    }
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
        anchors.left: categoryContainer.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: buttonBox.top
        anchors.leftMargin: 1
        anchors.topMargin: 1
        anchors.bottomMargin: 1
        currentIndex: categoryList.currentIndex
        onCurrentIndexChanged: reportScreenSelection()
        Rectangle {
            id: compressionTab
            color: "#ffdcdcdc"
            Column {
                id: compressionContainer
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                spacing: 5
                CCheckBox {
                    id: compressionAutoSelect
                    width: parent.width
                    font.bold: true
                    text: qsTr("Auto select")
                }
                Grid {
                    id: compressionAutoSelectEditor
                    enabled: !compressionAutoSelect.checked
                    rows: 2
                    columns: 2
                    rowSpacing: 0
                    columnSpacing: 0
                    leftPadding: 6
                    Text {
                        width: compressionContainer.width / 2
                        font.pixelSize: labelFontPixelSize
                        font.bold: true
                        color: compressionAutoSelectEditor.enabled ? "#ff000000" : "#ff939393"
                        text: qsTr("Preferred encoding")
                    }
                    Text {
                        width: compressionContainer.width / 2
                        font.pixelSize: labelFontPixelSize
                        font.bold: true
                        color: compressionAutoSelectEditor.enabled ? "#ff000000" : "#ff939393"
                        text: qsTr("Color level")
                    }
                    Column {
                        id: compressionEncodingTypes
                        CRadioButton {
                            id: compressionEncodingTight
                            text: qsTr("Tight")
                        }
                        CRadioButton {
                            id: compressionEncodingZRLE
                            text: qsTr("ZRLE")
                        }
                        CRadioButton {
                            id: compressionEncodingHextile
                            text: qsTr("Hextile")
                        }
                        CRadioButton {
                            id: compressionEncodingH264
                            text: qsTr("H.264")
                        }
                        CRadioButton {
                            id: compressionEncodingRaw
                            text: qsTr("Raw")
                        }
                    }
                    Column {
                        id: compressionColorLevels
                        CRadioButton {
                            id: compressionColorLevelFull
                            text: qsTr("Full")
                        }
                        CRadioButton {
                            id: compressionColorLevelMedium
                            text: qsTr("Medium")
                        }
                        CRadioButton {
                            id: compressionColorLevelLow
                            text: qsTr("Low")
                        }
                        CRadioButton {
                            id: compressionColorLevelVeryLow
                            text: qsTr("Very low")
                        }
                    }
                    ButtonGroup { buttons: compressionEncodingTypes.children }
                    ButtonGroup { buttons: compressionColorLevels.children }
                }
                CCheckBox {
                    id: compressionCustomCompressionLevel
                    width: parent.width
                    font.bold: true
                    text: qsTr("Custom compression level:")
                }
                Row {
                    id: compressionCustomCompressionLevelEditor
                    enabled: compressionCustomCompressionLevel.checked
                    spacing: 2
                    leftPadding: 12
                    TextField {
                        id: compressionCustomCompressionLevelTextEdit
                        width: 24
                        height: 24
                        font.pixelSize: labelFontPixelSize
                        validator: IntValidator {
                            bottom: 0
                            top: 9
                        }
                    }
                    Text {
                        height: 24
                        font.pixelSize: labelFontPixelSize
                        color: compressionCustomCompressionLevelEditor.enabled ? "#ff000000" : "#ff939393"
                        text: qsTr("level (0=fast, 9=best)")
                        verticalAlignment: Text.AlignVCenter
                    }
                }
                CCheckBox {
                    id: compressionJPEGCompression
                    width: parent.width
                    font.bold: true
                    text: qsTr("Allow JPEG compression:")
                }
                Row {
                    id: compressionJPEGCompressionEditor
                    enabled: !compressionJPEGCompression.checked
                    spacing: 2
                    leftPadding: 12
                    TextField {
                        id: compressionJPEGCompressionTextEdit
                        width: 24
                        height: 24
                        font.pixelSize: labelFontPixelSize
                        validator: IntValidator {
                            bottom: 0
                            top: 9
                        }
                    }
                    Text {
                        height: 24
                        font.pixelSize: labelFontPixelSize
                        color: compressionJPEGCompressionEditor.enabled ? "#ff000000" : "#ff939393"
                        text: qsTr("quality (0=poor, 9=best)")
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
        Rectangle {
            id: securityTab
            color: "#ffdcdcdc"
            Column {
                id: securityContainer
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                spacing: 0
                Text {
                    font.pixelSize: labelFontPixelSize
                    font.bold: true
                    text: qsTr("Encryption")
                }
                CCheckBox {
                    id: securityEncryptionNone
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("None")
                }
                CCheckBox {
                    id: securityEncryptionTLSWithAnonymousCerts
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("TLS with anonymous certificates")
                }
                CCheckBox {
                    id: securityEncryptionTLSWithX509Certs
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("TLS with X509 certificates")
                }
                Text {
                    leftPadding: 20
                    font.pixelSize: labelFontPixelSize
                    color: securityEncryptionTLSWithX509Certs.checked ? "#ff000000" : "#ff939393"
                    text: qsTr("Path to X509 CA certificate")
                }
                TextField {
                    id: securityEncryptionTLSWithX509CATextEdit
                    height: 24
                    leftPadding: 24
                    leftInset: 20
                    width: parent.width - leftPadding
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    font.pixelSize: labelFontPixelSize
                }
                Text {
                    leftPadding: 20
                    font.pixelSize: labelFontPixelSize
                    color: securityEncryptionTLSWithX509Certs.checked ? "#ff000000" : "#ff939393"
                    text: qsTr("Path to X509 CRL file")
                }
                TextField {
                    id: securityEncryptionTLSWithX509CRLTextEdit
                    height: 24
                    leftPadding: 24
                    leftInset: 20
                    width: parent.width - leftPadding
                    enabled: securityEncryptionTLSWithX509Certs.checked
                    font.pixelSize: labelFontPixelSize
                }
                CCheckBox {
                    id: securityEncryptionAES
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("RSA-AES")
                }
                Text {
                    font.pixelSize: labelFontPixelSize
                    font.bold: true
                    text: qsTr("Authentication")
                }
                CCheckBox {
                    id: securityAuthenticationNone
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("None")
                }
                CCheckBox {
                    id: securityAuthenticationStandard
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("Standard VNC (insecure without encryption)")
                }
                CCheckBox {
                    id: securityAuthenticationUsernameAndPassword
                    width: parent.width
                    leftPadding: 10
                    text: qsTr("Username and password (insecure without encryption)")
                }
            }
        }
        Rectangle {
            id: inputTab
            color: "#ffdcdcdc"
            Column {
                id: inputContainer
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                spacing: 0
                CCheckBox {
                    id: inputViewOnly
                    width: parent.width
                    text: qsTr("View only (ignore mouse and keyboard)")
                }
                Text {
                    leftPadding: 5
                    height: 20
                    font.pixelSize: labelFontPixelSize
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Mouse")
                }
                CCheckBox {
                    id: inputMouseEmulateMiddleButton
                    width: parent.width
                    leftPadding: 20
                    text: qsTr("Emulate middle mouse button")
                }
                CCheckBox {
                    id: inputMouseShowDot
                    width: parent.width
                    leftPadding: 20
                    text: qsTr("Show dot when no cursor")
                }
                Text {
                    leftPadding: 5
                    height: 20
                    font.pixelSize: labelFontPixelSize
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Keyboard")
                }
                CCheckBox {
                    id: inputKeyboardPassSystemKeys
                    width: parent.width
                    leftPadding: 20
                    text: qsTr("Pass system keys directly to server (full screen)")
                }
                Row {
                    leftPadding: 20
                    spacing: 3
                    Text {
                        height: 24
                        font.pixelSize: labelFontPixelSize
                        text: qsTr("Menu key")
                        verticalAlignment: Text.AlignVCenter
                    }
                    CComboBox {
                        id: inputKeyboardMenuKeyCombo
                        width: 90
                        model: Config.menuKeys
                    }
                }
                Text {
                    leftPadding: 5
                    height: 20
                    font.pixelSize: labelFontPixelSize
                    font.bold: true
                    verticalAlignment: Text.AlignBottom
                    text: qsTr("Clipboard")
                }
                CCheckBox {
                    id: inputClipboardFromServer
                    width: parent.width
                    leftPadding: 20
                    text: qsTr("Accept clipboard from server")
                }
                CCheckBox {
                    id: inputClipboardToServer
                    width: parent.width
                    leftPadding: 20
                    text: qsTr("Send clipboard to server")
                }
            }
        }
        Rectangle {
            id: displayTab
            color: "#ffdcdcdc"
            Text {
                id: displayDesc
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.leftMargin: 12
                anchors.topMargin: 10
                font.pixelSize: labelFontPixelSize
                font.bold: true
                text: qsTr("Display mode")
            }
            Column {
                id: displayContainer
                anchors.left: parent.left
                anchors.top: displayDesc.bottom
                CRadioButton {
                    id: displayWindowed
                    text: qsTr("Windowed")
                }
                CRadioButton {
                    id: displayFullScreenOnCurrentMonitor
                    text: qsTr("Full screen on current monitor")
                }
                CRadioButton {
                    id: displayFullScreenOnAllMonitors
                    text: qsTr("Full screen on all monitors")
                }
                CRadioButton {
                    id: displayFullScreenOnSelectedMonitors
                    text: qsTr("Full screen on selected monitor(s)")
                }
            }
            ButtonGroup { buttons: displayContainer.children }
            Rectangle {
                id: displaySelectionView
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: displayContainer.bottom
                anchors.leftMargin: 35
                anchors.rightMargin: 10
                anchors.topMargin: 5
                height: 150
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
        }
        Rectangle {
            id: miscTab
            color: "#ffdcdcdc"
            Column {
                id: miscContainer
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.topMargin: 10
                CCheckBox {
                    id: miscShared
                    text: qsTr("Shared (don't disconnect other viewers)")
                }
                CCheckBox {
                    id: miscReconnectQuery
                    text: qsTr("Ask to reconnect on connection errors")
                }
            }
        }
    }

    Rectangle {
        id: buttonBox
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 52
        color: "#ffdcdcdc"

        CButton {
            id: cancelButton
            anchors.right: okButton.left
            anchors.bottom: okButton.bottom
            anchors.rightMargin: 15
            text: qsTr("Cancel")
            onClicked: close()
        }
        CButton {
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
