import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    ColumnLayout {
        anchors.centerIn: parent
        spacing: 12

        Label { text: qsTr("Dashboard"); font.pixelSize: 24; font.bold: true }
        Label { text: qsTr("Live gauge view lands here (phase 2)") }

        Button {
            text: qsTr("Connect (not implemented)")
            onClicked: statusLabel.text = QtTune.statusString(-2)  // QT_ERR_NOT_IMPLEMENTED
        }
        Label { id: statusLabel; text: "" }
    }
}
