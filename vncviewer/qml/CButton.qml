import QtQuick 2.15
import QtQuick.Controls 2.15

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
