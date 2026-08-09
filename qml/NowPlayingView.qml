pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-5.1 / §9.2: large artwork and title/artist/album. That list is the
// whole screen, and it is shorter than it used to be on purpose.
//
// **This screen owns nothing that another one already has.** It once carried
// its own transport row, its own mix panel, the engine's format badge and its
// own full-width seek bar — every one of them a second copy of something the
// bottom bar or the diagnostics screen already showed. Two progress bars sixty
// pixels apart, both tracking the same track and both seekable, is not
// redundancy that helps: it is two things to read where one would do, and the
// pair drift the moment either is touched. The bottom bar is visible from
// every screen, so a control here can only ever be a duplicate (prd.md §9.1).
//
// The position went to the seam between this pane and the bottom bar, where
// `SeekBar` is now the divider. It is directly below this view and spans the
// whole window, so nothing was lost by taking it off the cover.
//
// What is left is the cover, what is playing, and — beside the cover rather
// than under it — what comes after.
Item {
    id: root

    Theme { id: theme }

    // The row after the current one, or null when there is none. Reads
    // `app.queue.count` as well as the cursor so it re-evaluates when the queue
    // is rebuilt under a mix, not only when the track changes.
    readonly property var nextRow: {
        const total = app.queue.count
        const next = app.queue.currentIndex + 1
        if (next <= 0 || next >= total)
            return null
        return app.queue.get(next)
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: theme.margin * 2
        spacing: theme.margin

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }

        // The cover is the item that gives way when the window is short, and it
        // has to be told so. A Layout child pinned to a preferred size will not
        // shrink no matter how little room is left — the layout overflows its
        // parent instead and the rows below are drawn under the bottom bar.
        //
        // So the cover's height is clamped to the space this Item was actually
        // given, and its width follows its height. One-way: width follows
        // height, height never follows width.
        Item {
            id: coverRow
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 96

            // The gutter each side of a centred cover. `upNext` lives in the
            // right one and hides when that gutter is too narrow to read in,
            // which is what keeps the cover centred rather than nudged left.
            readonly property real gutter: (width - cover.width) / 2

            Artwork {
                id: cover
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                height: Math.max(96, Math.min(parent.height,
                                              Math.min(root.width * 0.42, 460)))
                width: height
                coverId: app.player.coverId
                requestSize: 600
            }

            // ── Up next, beside the cover instead of under the seek bar. It is
            // about a different track from everything in the centre column, and
            // a line of it directly below the current track's own three lines
            // read as a fourth line about the current track.
            ColumnLayout {
                id: upNext
                anchors.left: cover.right
                anchors.leftMargin: theme.margin * 1.5
                anchors.top: cover.top
                width: Math.max(0, Math.min(220, coverRow.gutter - theme.margin * 2))
                spacing: 1
                visible: root.nextRow !== null && coverRow.gutter >= 200

                Label {
                    text: qsTr("NEXT")
                    color: theme.textFaint
                    font.pixelSize: theme.fontSmall
                    font.letterSpacing: 1.5
                    bottomPadding: 4
                }
                Label {
                    Layout.fillWidth: true
                    text: root.nextRow ? root.nextRow.title : ""
                    color: theme.textMuted
                    font.pixelSize: theme.fontNormal
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: root.nextRow ? root.nextRow.artist : ""
                    visible: text.length > 0
                    color: theme.textFaint
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
            }
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

        // ── The passive sync indicator (prd.md FR-6.5). It survived the cull
        // that took the format badge because it is not a readout: it says
        // somebody else is steering this player, which changes what the buttons
        // in the bottom bar will do. It is also absent almost always.
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            visible: app.player.synced
            spacing: 6

            Label {
                text: theme.iconSync
                font.family: theme.iconFont
                font.pixelSize: theme.fontSmall
                color: theme.textFaint
            }
            Label {
                text: qsTr("Synced by another controller")
                font.pixelSize: theme.fontSmall
                color: theme.textFaint
            }
        }

        Item { Layout.fillHeight: true; Layout.preferredHeight: 1 }
    }
}
