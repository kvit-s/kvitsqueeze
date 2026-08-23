// SPDX-License-Identifier: MPL-2.0

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

                // prd.md FR-5.5. The cover is the only thing on this screen
                // big enough to be an obvious target and the only one that is
                // not already a control, and the sheet it opens is about the
                // track the cover is of.
                TapHandler {
                    enabled: app.player.hasTrack
                    onTapped: app.lyrics.toggle()
                }

                HoverHandler {
                    id: coverHover
                    enabled: app.player.hasTrack
                    cursorShape: Qt.PointingHandCursor
                }

                // Nothing else on this screen is clickable, so the cover has
                // to say that it is — once, on hover, rather than as a badge
                // that sits over the artwork forever.
                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: theme.spacing
                    width: hint.width + theme.spacing * 2
                    height: hint.height + theme.spacing
                    radius: height / 2
                    color: Qt.rgba(0, 0, 0, 0.66)
                    opacity: coverHover.hovered && !app.lyrics.open ? 1 : 0
                    visible: opacity > 0

                    Behavior on opacity {
                        NumberAnimation { duration: theme.animation }
                    }

                    Label {
                        id: hint
                        anchors.centerIn: parent
                        text: qsTr("Lyrics")
                        color: "#ffffff"
                        font.pixelSize: theme.fontSmall
                    }
                }
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

    // ── The lyric sheet (prd.md FR-5.5), over the pane rather than inside the
    // cover. In a small window the cover is barely 200 px square, which is a
    // column four or five words wide — the sheet needs the whole pane to be
    // readable at all, and it is a mode the user opened rather than a panel
    // competing for the layout.
    //
    // Every state says which one it is. LMS omits the field for a file that
    // carries no lyrics, which on the wire looks exactly like a request that
    // failed, and drawing both as an empty sheet is the metadata version of
    // reporting an unknown as a fact (prd.md FR-2.5).
    Rectangle {
        id: lyricsPane

        anchors.fill: parent
        color: Qt.rgba(theme.surface.r, theme.surface.g, theme.surface.b, 0.94)
        opacity: app.lyrics.open ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: theme.animation }
        }

        // Anywhere off the text closes it, which is the same gesture that
        // opened it. The close button is for the people who look for one.
        TapHandler { onTapped: app.lyrics.open = false }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: theme.margin
            spacing: theme.spacing

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: app.lyrics.trackTitle
                    color: theme.textMuted
                    font.pixelSize: theme.fontNormal
                    elide: Text.ElideRight
                }

                IconButton {
                    glyph: theme.iconClose
                    glyphSize: theme.fontNormal
                    baseColor: theme.textMuted
                    tooltip: qsTr("Close the lyrics")
                    onClicked: app.lyrics.open = false
                }
            }

            // The sheet. A list rather than one block of text, because the
            // line being sung has to be findable — and because a timed sheet
            // scrolls itself, which a Text cannot do.
            //
            // ApplyRange, not StrictlyEnforceRange: the current line is kept in
            // the middle band while the song plays, and a reader who flicks
            // ahead is left where they flicked rather than snapped back.
            ListView {
                id: sheet

                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                model: app.lyrics.status === Lyrics.Ready ? app.lyrics.lines : []
                currentIndex: app.lyrics.currentLine
                highlightRangeMode: ListView.ApplyRange
                preferredHighlightBegin: height / 2 - theme.rowHeight
                preferredHighlightEnd: height / 2 + theme.rowHeight
                highlightMoveDuration: theme.animation * 3
                boundsBehavior: Flickable.StopAtBounds
                visible: app.lyrics.status === Lyrics.Ready

                delegate: Label {
                    required property int index
                    required property string modelData

                    width: ListView.view.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: modelData
                    topPadding: theme.spacing / 2
                    bottomPadding: theme.spacing / 2
                    font.pixelSize: theme.fontLarge
                    lineHeight: 1.3

                    // Three weights, not two: the line being sung, the lines
                    // around it, and — when the sheet is untimed — every line
                    // equally, because nothing is known about which is current
                    // and dimming the rest would be a claim.
                    color: !app.lyrics.timed ? theme.textMuted
                         : index === app.lyrics.currentLine ? theme.textPrimary
                         : theme.textFaint
                    font.bold: app.lyrics.timed && index === app.lyrics.currentLine

                    Behavior on color {
                        ColorAnimation { duration: theme.animation }
                    }
                }
            }

            // Every other state is one line about the state itself, centred in
            // the pane rather than at the top of an empty list.
            Label {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: app.lyrics.status !== Lyrics.Ready
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                wrapMode: Text.WordWrap
                color: theme.textFaint
                font.pixelSize: theme.fontNormal
                text: {
                    switch (app.lyrics.status) {
                    case Lyrics.Loading: return qsTr("Looking…")
                    case Lyrics.Absent:  return qsTr("This track carries no lyrics.")
                    case Lyrics.Unavailable:
                        return qsTr("The server did not answer. Tap to try again.")
                    default: return qsTr("Nothing to show.")
                    }
                }

                // A failed request is the one state with something to do about
                // it, and this is the only place the retry belongs: a button
                // for it would be on screen in the four states where it means
                // nothing.
                TapHandler {
                    enabled: app.lyrics.status === Lyrics.Unavailable
                    onTapped: app.lyrics.refresh()
                }
            }
        }
    }
}
