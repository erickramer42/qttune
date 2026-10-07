import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Page {
    id: root
    background: Rectangle { color: Theme.bg }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        Label {
            text: qsTr("Live Signals")
            font.pixelSize: 20
            color: Theme.text
        }

        Label {
            text: QtTune.connected
                ? qsTr("Subscribed: %1 signals · batched @ 10 Hz").arg(QtTune.signalModel.subscribedCount)
                : qsTr("Connect a session to stream decoded values")
            color: QtTune.connected ? Theme.textDim : Theme.danger
        }

        ListView {
            id: signalList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: QtTune.signalModel

            delegate: Rectangle {
                width: signalList.width
                height: 48
                color: index % 2 === 0 ? Theme.surface : Theme.bg

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 12

                    CheckBox {
                        checked: model.subscribed
                        onToggled: QtTune.signalModel.toggleSubscribed(index)
                    }
                    Label {
                        text: model.name
                        color: Theme.text
                        Layout.fillWidth: true
                    }
                    Label {
                        text: QtTune.connected
                            ? "%1 %2".arg(model.value.toFixed(1)).arg(model.unit)
                            : qsTr("—")
                        color: model.subscribed ? Theme.primary : Theme.textDim
                        font.family: "Consolas"   // tabular-ish digits, no jitter
                    }
                }
            }
        }
    }
}
