import QtQuick 2.15
import QtQuick.Controls 2.15

RadioButton {
    id: control

    indicator: Rectangle {
         implicitWidth: 12
         implicitHeight: 12
         x: control.leftPadding
         y: parent.height / 2 - height / 2
         radius: 6
         border.color: "#ff878787"

         Rectangle {
             width: 8
             height: 8
             x: 2
             y: 2
             radius: 4
             color: control.enabled ? "#ff000080" : "#ff878787"
             visible: control.checked
         }
     }
}
