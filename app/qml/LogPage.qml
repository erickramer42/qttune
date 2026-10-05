import QtQuick
import QtQuick.Controls

Pane {
    background: Rectangle { color: Theme.bg }

    Label {
        anchors.centerIn: parent
        text: qsTr("Frame log / trace viewer lands here.")
        horizontalAlignment: Text.AlignHCenter
        color: Theme.text
    }
}