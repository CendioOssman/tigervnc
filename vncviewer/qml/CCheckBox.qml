import QtQuick 2.15
import QtQuick.Controls 2.15

CheckBox {
    id: control

    indicator: Rectangle {
        implicitWidth: 14
        implicitHeight: 14
        x: control.leftPadding
        y: parent.height / 2 - height / 2
        radius: 3
        border.color: "#ff878787"

        Text {
            y: -2
            visible: control.checked
            color: "#ff000080"
            text: String.fromCodePoint(0x2713)
            font.bold: true
            font.pixelSize: 14
        }
    }
}
