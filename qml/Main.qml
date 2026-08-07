import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The shell (prd.md §9.1): left rail, content pane, always-visible bottom bar.
// Skeleton — the panes are placeholders until M2.
ApplicationWindow {
    id: root

    width: 1100
    height: 720
    minimumWidth: 720
    minimumHeight: 480
    visible: true
    title: qsTr("SqeezeAmp")

    // One theme, no skinning system (prd.md N8). Colours live here until the
    // theme singleton exists; they are hard-coded against a single look on
    // purpose, so nothing needs to be pluggable.
    readonly property color surface: "#14161a"
    readonly property color surfaceRaised: "#1c1f25"
    readonly property color textPrimary: "#e8eaed"
    readonly property color textMuted: "#9aa0a6"
    readonly property color accent: "#4a9eff"

    color: surface

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Left rail. No player section: there is one player and it is
            // this app (prd.md N5).
            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: root.surfaceRaised

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 2

                    Repeater {
                        model: ["Now Playing", "Artists", "Albums", "Genres",
                                "Years", "Playlists", "Folders", "New Music"]

                        delegate: ItemDelegate {
                            required property string modelData
                            Layout.fillWidth: true
                            text: modelData
                            contentItem: Label {
                                text: parent.text
                                color: root.textPrimary
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    ItemDelegate {
                        Layout.fillWidth: true
                        text: qsTr("Settings")
                        contentItem: Label {
                            text: parent.text
                            color: root.textMuted
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }

            // ── Content pane.
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: root.surface

                Label {
                    anchors.centerIn: parent
                    color: root.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Not connected to a server.\nBrowse arrives at M2.")
                }
            }
        }

        // ── Bottom bar: always visible, always this app's own playback.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 72
            color: root.surfaceRaised

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 48
                    Layout.preferredHeight: 48
                    radius: 4
                    color: "#2a2e35"
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: qsTr("Nothing playing")
                        color: root.textPrimary
                    }
                    Label {
                        text: qsTr("—")
                        color: root.textMuted
                        font.pixelSize: 12
                    }
                }

                Label {
                    text: qsTr("⏮   ⏵   ⏭")
                    color: root.textPrimary
                }
            }
        }
    }
}
