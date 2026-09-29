import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    width: 1100
    height: 700
    visible: true
    title: qsTr("QtTune")

    readonly property bool isCompact: width < 720

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 48
            color: "#1e1e2e"

            Label {
                anchors.centerIn: parent
                text: qsTr("QtTune — core %1 [%2]")
                    .arg(QtTune.coreVersion)
                    .arg(QtTune.coreInitialized ? "ready" : "init failed")
                color: QtTune.coreInitialized ? "white" : "#ff5555"
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            ColumnLayout {
                visible: !root.isCompact
                Layout.preferredWidth: 200
                spacing: 1

                Button { text: qsTr("Dashboard"); Layout.fillWidth: true; onClicked: stack.currentIndex = 0 }
                Button { text: qsTr("Log");        Layout.fillWidth: true; onClicked: stack.currentIndex = 1 }
                Button { text: qsTr("Settings");   Layout.fillWidth: true; onClicked: stack.currentIndex = 2 }
                Item { Layout.fillHeight: true }
            }

            StackLayout {
                id: stack
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: 0

                DashboardPage {}
                LogPage {}
                SettingsPage {}
            }
        }

        TabBar {
            visible: root.isCompact
            Layout.fillWidth: true

            TabButton { text: qsTr("Dash"); onClicked: stack.currentIndex = 0 }
            TabButton { text: qsTr("Log");  onClicked: stack.currentIndex = 1 }
            TabButton { text: qsTr("Setup"); onClicked: stack.currentIndex = 2 }
        }
    }
}
