import QtQuick 2.12
import QtQuick.Controls 2.12

ComboBox {
    id: control

    property real labelFontPixelSize: 12

    signal textModified(string pendingText)

    height: 24
    currentIndex: 0
    font.pixelSize: labelFontPixelSize
    background: Rectangle {
        radius: 3
        color: "#ffffffff"
        border.color: "#ff6e6e6e"
        border.width: control.focus ? 2 : 1
    }
    delegate: ItemDelegate {
        width: control.width
        highlighted: control.highlightedIndex - index === 0
        padding: 2
        background: Rectangle {
            color: highlighted ? "#ffdcdcdc" : "#ffffffff"
        }
        contentItem: Text {
            text: modelData
            color: "#ff000000"
            font: control.font
            verticalAlignment: Text.AlignVCenter
        }
    }
    onHighlightedIndexChanged: {
        if (highlightedIndex >= 0) {
            popupList.positionViewAtIndex(highlightedIndex, ListView.Contain)
        }
    }
    contentItem: TextField {
        leftPadding: 4
        rightPadding: control.indicator.width + control.spacing
        readOnly: !control.editable
        enabled: control.editable
        text: control.displayText
        font: control.font
        verticalAlignment: Text.AlignVCenter
        palette.text: "#ff000000"
        background: Rectangle {
            border.width: 1
            border.color: "#ff000000"
        }
        onEditingFinished: {
            if (control.currentText !== text) {
                control.textModified(text)
            }
        }
    }
    popup: Popup {
        y: control.height - 1
        width: control.width
        implicitHeight: Math.min(700, contentItem.contentHeight) + spacing + padding * 2
        padding: 1

        contentItem: ListView {
            id: popupList
            clip: true
            implicitHeight: contentHeight
            spacing: 0
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }

        background: Rectangle {
            border.width: 1
            border.color: "#ff000000"
        }
    }
    indicator: Rectangle {
        x: control.availableWidth - 1.5
        y: control.topPadding + (control.availableHeight - height) / 2
        width: height * 0.95
        height: control.availableHeight - control.topPadding - control.bottomPadding - 4
        radius: 3
        color: "#ffffffff"
        border.width: 1
        border.color: "#ff6e6e6e"
        Image {
            id: icon
            anchors.fill: parent
            anchors.margins: 5
            fillMode: Image.PreserveAspectFit
            source: "qrc:/images/dropdown_24px.png"
        }
    }
}
