import QtQuick 2.12
import QtQuick.Window 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import Qt.TigerVNC 1.0

Item {
    id: root

    property bool screenLaid: false
    property var screenSelectionButtons: []
    property var selectedScreens: []
    property var optionLabels: []
    property var optionIndices: []

    function commit() {
        // Update fullscreen properties. Note that Config.fullScreen must be updated at the last, because changing it causes immediate fullscreening, so
        // all the fullscreen properties must be updated before updating Config.fullScreen.
        var selectedScreens = []
        for (var bix = 0; bix < screenSelectionButtons.length; bix++) {
            if (screenSelectionButtons[bix].checked) {
                selectedScreens.push(bix + 1) // selected screen ID is 1-origin.
            }
        }
        var selectedScreensChanged = !equals(Config.selectedScreens, selectedScreens)
        Config.selectedScreens = selectedScreens


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
            var button = screenButtonComponent.createObject(displaySelectionView, {
                                                                "x": lx,
                                                                "y": ly,
                                                                "width": lw,
                                                                "height": lh,
                                                                "checkable": true
                                                            })
            var handler = selectionHandlerFactory.createObject(this, {
                                                                   "buttonIndex": j
                                                               })
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
            var index = selectedScreens[scix] - 1 // selected screen ID is 1-origin.
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

    function reportScreenSelection() {
        // For debugging purpose.
        for (var i = 0; i < screenSelectionButtons.length; i++) {
            console.log("screenSelectionButtons[" + i + "]=" + screenSelectionButtons[i].checked)
        }
    }

    Component.onCompleted: {
        reset()
    }

    StackLayout {
        id: contentStack
        width: childrenRect.width
        height: childrenRect.height
        x: categoryContainer.width
        currentIndex: optionIndices[categoryList.currentIndex]
        onCurrentIndexChanged: reportScreenSelection()
        Rectangle {
            id: displayTab
            implicitWidth: childrenRect.width + childrenRect.x
            implicitHeight: childrenRect.height + childrenRect.y
            ButtonGroup {
                id: displayButtonGroup
            }
            ColumnLayout {
                id: displayDescContainer
                x: 10
                y: 10
                spacing: 0
                Text {
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
            }
        }
    }
}
