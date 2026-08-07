pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-5.1 / §9.2: large artwork, metadata, seek bar, transport, the
// format badge, and an up-next strip.
//
// The format badge is the one place the engine's own state surfaces in the
// main UI. Under Backend B most of it is scraped from squeezelite's log, so it
// is often partial — and prd.md FR-2.5 is explicit that a field the backend
// could not determine is *hidden*, not shown as zero. EngineController
// assembles what it knows and this view draws whatever came out, including
// nothing.
Item {
    id: root

    Theme { id: theme }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.margin * 2
        spacing: theme.margin

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }

        Artwork {
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Math.min(root.width * 0.5, root.height * 0.46, 460)
            Layout.preferredHeight: Layout.preferredWidth
            coverId: app.player.coverId
            requestSize: 600
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignHCenter
            spacing: 2

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: app.player.hasTrack ? app.player.title : qsTr("Nothing playing")
                color: theme.textPrimary
                font.pixelSize: theme.fontHuge
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: app.player.artist
                visible: text.length > 0
                color: theme.textMuted
                font.pixelSize: theme.fontLarge
                elide: Text.ElideRight
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: app.player.album
                visible: text.length > 0
                color: theme.textFaint
                font.pixelSize: theme.fontNormal
                elide: Text.ElideRight
            }
        }

        // ── Seek bar with the two clocks around it.
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 720
            Layout.alignment: Qt.AlignHCenter
            spacing: theme.spacing

            Label {
                text: theme.duration(app.player.elapsed)
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                Layout.preferredWidth: 46
                horizontalAlignment: Text.AlignRight
            }

            SeekBar { Layout.fillWidth: true }

            Label {
                // Remaining rather than total: what is left is the number a
                // listener actually looks at.
                text: app.player.duration > 0
                      ? "-" + theme.duration(Math.max(0, app.player.duration
                                                          - app.player.elapsed))
                      : "—"
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                Layout.preferredWidth: 46
            }
        }

        // ── Transport
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: theme.spacing

            IconButton {
                glyph: theme.iconShuffle
                active: app.player.shuffleMode !== 0
                tooltip: app.player.shuffleMode === 0 ? qsTr("Shuffle off")
                       : app.player.shuffleMode === 1 ? qsTr("Shuffle songs")
                                                      : qsTr("Shuffle albums")
                onClicked: app.player.cycleShuffle()
            }
            IconButton {
                glyph: theme.iconPrevious
                glyphSize: theme.fontLarge
                tooltip: qsTr("Previous")
                onClicked: app.player.previous()
            }
            IconButton {
                glyph: app.player.playing ? theme.iconPause : theme.iconPlay
                glyphSize: theme.fontHuge
                tooltip: app.player.playing ? qsTr("Pause") : qsTr("Play")
                onClicked: app.player.playPause()
            }
            IconButton {
                glyph: theme.iconNext
                glyphSize: theme.fontLarge
                tooltip: qsTr("Next")
                onClicked: app.player.next()
            }
            IconButton {
                glyph: app.player.repeatMode === 1 ? theme.iconRepeatOne : theme.iconRepeatAll
                active: app.player.repeatMode !== 0
                tooltip: app.player.repeatMode === 0 ? qsTr("Repeat off")
                       : app.player.repeatMode === 1 ? qsTr("Repeat one")
                                                     : qsTr("Repeat all")
                onClicked: app.player.cycleRepeat()
            }
        }

        // ── Format badge and the passive sync indicator (prd.md FR-6.5).
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: theme.spacing
            visible: app.engine.formatBadge.length > 0 || app.player.synced

            Label {
                text: app.engine.formatBadge
                visible: text.length > 0
                color: theme.textFaint
                font.pixelSize: theme.fontSmall
            }

            Label {
                visible: app.player.synced
                text: theme.iconSync + "  " + qsTr("Synced by another controller")
                font.family: theme.iconFont
                color: theme.textFaint
                font.pixelSize: theme.fontSmall
            }
        }

        // ── Up next.
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            Layout.maximumWidth: 720
            spacing: theme.spacing
            visible: nextTitle.text.length > 0

            Label {
                text: qsTr("Up next")
                color: theme.textFaint
                font.pixelSize: theme.fontSmall
            }
            Label {
                id: nextTitle
                Layout.fillWidth: true
                elide: Text.ElideRight
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                text: {
                    // Depends on count so it re-evaluates when the queue is
                    // rebuilt as well as when the cursor moves.
                    const total = app.queue.count
                    const next = app.queue.currentIndex + 1
                    if (next <= 0 || next >= total)
                        return ""
                    const row = app.queue.get(next)
                    return row.artist ? row.title + " — " + row.artist : row.title
                }
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }
    }
}
