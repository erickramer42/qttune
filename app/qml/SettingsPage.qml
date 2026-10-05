import QtQuick
import QtQuick.Controls

Pane {
    background: Rectangle { color: Theme.bg }

    Label {
        anchors.centerIn: parent
        text: qsTr("Settings — transport selection, profiles,\ncasual/pro mode toggle lands here.")
        horizontalAlignment: Text.AlignHCenter
    }
}
