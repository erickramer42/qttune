import QtQuick
import QtQuick.Controls

Button {
    id: control

    // Expose so callers can tweak if needed
    property color baseColor: Theme.elevated
    property color hoverColor: Theme.elevatedLight
    property color pressedColor: Theme.surface

    background: Rectangle {
        implicitWidth: 120
        implicitHeight: 36
        radius: Theme.radius
        color: control.down    ? control.pressedColor
             : control.hovered ? control.hoverColor
             :                   control.baseColor
    }

    contentItem: Text {
        text: control.text
        font.pixelSize: 14
        color: Theme.text
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }
}
