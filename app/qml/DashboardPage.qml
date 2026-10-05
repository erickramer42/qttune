import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    background: Rectangle { color: Theme.bg }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label {
            text: qsTr("Dashboard")
            font.pixelSize: 24
            font.bold: true
            color: Theme.text
        }
        Label {
            text: qsTr("Live gauge view lands here (phase 2)")
            color: Theme.textDim
        }
        ThemedButton {
            text: qsTr("Connect (not implemented)")
            onClicked: statusLabel.text = QtTune.statusString(-2)
        }
        Label { id: statusLabel; text: ""; color: Theme.textDim }
    }
}
