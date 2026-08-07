pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// prd.md FR-5.4: a small always-on-top window with artwork and transport.
//
// A separate Window rather than a mode of the main one: "always on top" is a
// window flag, and toggling it on the shell would drag the whole browse UI in
// front of everything the user is actually working in.
Window {
    id: root

    property bool pinned: true

    Theme { id: theme }

    width: 360
    height: 108
    minimumWidth: 300
    minimumHeight: 96
    maximumHeight: 140
    title: qsTr("SqeezeAmp")
    color: theme.surfaceRaised
    flags: Qt.Window | Qt.WindowStaysOnTopHint | Qt.WindowTitleHint
           | Qt.WindowCloseButtonHint

    RowLayout {
        anchors.fill: parent
        anchors.margins: theme.spacing
        spacing: theme.spacing

        Artwork {
            Layout.preferredWidth: root.height - theme.spacing * 2
            Layout.preferredHeight: Layout.preferredWidth
            coverId: app.player.coverId
            requestSize: 150
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: app.player.hasTrack ? app.player.title : qsTr("Nothing playing")
                color: theme.textPrimary
                font.pixelSize: theme.fontNormal
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                text: app.player.artist
                visible: text.length > 0
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                elide: Text.ElideRight
            }

            SeekBar { Layout.fillWidth: true }

            RowLayout {
                spacing: 0
                IconButton {
                    glyph: theme.iconPrevious
                    glyphSize: theme.fontNormal
                    onClicked: app.player.previous()
                }
                IconButton {
                    glyph: app.player.playing ? theme.iconPause : theme.iconPlay
                    glyphSize: theme.fontNormal
                    onClicked: app.player.playPause()
                }
                IconButton {
                    glyph: theme.iconNext
                    glyphSize: theme.fontNormal
                    onClicked: app.player.next()
                }
                Item { Layout.fillWidth: true }
                IconButton {
                    glyph: theme.iconPin
                    glyphSize: theme.fontSmall
                    active: root.pinned
                    tooltip: qsTr("Keep on top")
                    onClicked: {
                        root.pinned = !root.pinned
                        root.flags = root.pinned
                            ? (Qt.Window | Qt.WindowStaysOnTopHint | Qt.WindowTitleHint
                               | Qt.WindowCloseButtonHint)
                            : (Qt.Window | Qt.WindowTitleHint | Qt.WindowCloseButtonHint)
                    }
                }
            }
        }
    }
}
