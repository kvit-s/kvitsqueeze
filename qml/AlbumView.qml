// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-3.7: an album's full track list, with disc grouping, its year,
// genre and duration, and play/queue actions per track and for the whole
// album.
//
// The header's metadata is passed in from the row that was clicked rather than
// refetched. The albums reply already carried all of it, and a second round
// trip to learn what the caller just read would be latency spent on nothing.
Item {
    id: root

    required property string albumId
    required property string albumTitle
    required property string albumArtist
    property string albumArtistId
    property string coverId
    property int year: -1

    signal openArtist(string artistId, string name)

    Theme { id: theme }

    // Sorted by track number, and the disc number comes along so a multi-disc
    // album can be grouped rather than run together as one 30-track list.
    readonly property var tracks: app.library.browse(
        Library.Tracks,
        ["album_id:" + albumId, "sort:tracknum"])

    // Reached from a search result there is no cover id to pass in, because
    // the search reply carries only an id and a title. The first track's cover
    // is the album's cover, so the header fills itself in as soon as the track
    // list lands rather than costing a request of its own.
    readonly property string effectiveCover: {
        if (root.coverId)
            return root.coverId
        if (!root.tracks || root.tracks.total <= 0)
            return ""
        return root.tracks.get(0).coverId || ""
    }

    readonly property string effectiveArtist: {
        if (root.albumArtist)
            return root.albumArtist
        if (!root.tracks || root.tracks.total <= 0)
            return ""
        return root.tracks.get(0).subtitle || ""
    }

    QueueMenu { id: rowMenu }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: headerRow.implicitHeight + theme.margin * 2
            color: theme.surfaceRaised

            RowLayout {
                id: headerRow
                anchors.fill: parent
                anchors.margins: theme.margin
                spacing: theme.margin

                Artwork {
                    Layout.preferredWidth: theme.compact ? 96 : 132
                    Layout.preferredHeight: Layout.preferredWidth
                    coverId: root.effectiveCover
                    requestSize: 300
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 4

                    Label {
                        Layout.fillWidth: true
                        text: root.albumTitle
                        color: theme.textPrimary
                        font.pixelSize: theme.fontHuge
                        font.bold: true
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: root.effectiveArtist
                        color: root.albumArtistId ? theme.accent : theme.textMuted
                        font.pixelSize: theme.fontLarge
                        elide: Text.ElideRight

                        TapHandler {
                            // Only clickable when the albums reply carried an
                            // artist_id. It does on Lyrion 9.1, but a row
                            // without one must read as plain text rather than
                            // as a link that does nothing.
                            enabled: root.albumArtistId.length > 0
                            onTapped: root.openArtist(root.albumArtistId, root.albumArtist)
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        text: {
                            var parts = []
                            if (root.year > 0)
                                parts.push(root.year)
                            if (root.tracks && root.tracks.loaded)
                                parts.push(qsTr("%n track(s)", "", root.tracks.total))
                            return parts.join("  ·  ")
                        }
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }

                    RowLayout {
                        spacing: theme.spacing
                        Layout.topMargin: 4

                        Button {
                            text: qsTr("Play")
                            onClicked: app.library.enqueue("album_id", root.albumId,
                                                           Library.PlayNow)
                        }
                        Button {
                            text: qsTr("Play next")
                            onClicked: app.library.enqueue("album_id", root.albumId,
                                                           Library.PlayNext)
                        }
                        Button {
                            text: qsTr("Add to queue")
                            onClicked: app.library.enqueue("album_id", root.albumId,
                                                           Library.AddToEnd)
                        }
                    }
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        // ── Track table
        ListView {
            id: trackList
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.tracks
            clip: true
            focus: true
            ScrollBar.vertical: ScrollBar {}

            delegate: ItemDelegate {
                id: row
                required property int index
                required property string itemId
                required property string title
                required property string subtitle
                required property double duration
                required property int trackNumber
                required property bool loaded

                width: ListView.view.width
                height: theme.rowHeight

                // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                // padding above and below its content.
                topPadding: 0
                bottomPadding: 0

                Component.onCompleted: root.tracks.ensureLoaded(index)

                background: Rectangle {
                    color: row.hovered ? theme.surfaceHover : "transparent"
                }

                contentItem: RowLayout {
                    spacing: theme.spacing

                    Label {
                        Layout.preferredWidth: 28
                        horizontalAlignment: Text.AlignRight
                        text: row.trackNumber > 0 ? row.trackNumber : ""
                        color: theme.textFaint
                        font.pixelSize: theme.fontSmall
                    }

                    Label {
                        Layout.fillWidth: true
                        text: row.loaded ? row.title : "…"
                        color: theme.textPrimary
                        font.pixelSize: theme.fontNormal
                        elide: Text.ElideRight
                    }

                    // Shown only when it differs from the album artist, which
                    // is how a compilation reads correctly without a separate
                    // screen for one (prd.md FR-3.6).
                    Label {
                        text: row.subtitle
                        visible: text.length > 0 && text !== root.effectiveArtist
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                        elide: Text.ElideRight
                        Layout.maximumWidth: row.width * 0.28
                    }

                    Label {
                        text: theme.duration(row.duration)
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }
                }

                onClicked: app.library.enqueue("track_id", row.itemId, Library.PlayNow)

                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: {
                        rowMenu.selectorKey = "track_id"
                        rowMenu.selectorValue = row.itemId
                        rowMenu.selectorFilters = []
                        rowMenu.popup()
                    }
                }
            }
        }
    }
}
