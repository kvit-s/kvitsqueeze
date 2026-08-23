// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md FR-3.6: an artist's albums as a grid, plus an "all tracks" list.
//
// Both are the same composable filter — `artist_id` — sent to two different
// typed commands, which is the whole of why N4's decision to drop the generic
// menu renderer costs nothing here.
//
// Appears-on and compilation handling is the server's: `albums` with an
// artist_id returns what LMS considers that artist's albums under its own
// album_artist / compilation rules, and second-guessing it in the client would
// disagree with every other controller on the network.
Item {
    id: root

    required property string artistId
    required property string artistName

    signal openAlbum(string albumId, string title, string artist, string artistId,
                     string coverId, int year)

    Theme { id: theme }

    readonly property var albums: app.library.browse(
        Library.Albums, ["artist_id:" + artistId, "sort:yearalbum"])
    readonly property var tracks: app.library.browse(
        Library.Tracks, ["artist_id:" + artistId, "sort:albumtrack"])

    QueueMenu { id: rowMenu }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: header.implicitHeight + theme.margin * 2
            color: theme.surfaceRaised

            RowLayout {
                id: header
                anchors.fill: parent
                anchors.margins: theme.margin
                spacing: theme.spacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        text: root.artistName
                        color: theme.textPrimary
                        font.pixelSize: theme.fontHuge
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    Label {
                        text: {
                            var parts = []
                            if (root.albums && root.albums.loaded)
                                parts.push(qsTr("%n album(s)", "", root.albums.total))
                            if (root.tracks && root.tracks.loaded)
                                parts.push(qsTr("%n track(s)", "", root.tracks.total))
                            return parts.join("  ·  ")
                        }
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }
                }

                Button {
                    text: qsTr("Play all")
                    onClicked: app.library.enqueue("artist_id", root.artistId,
                                                   Library.PlayNow)
                }
                Button {
                    text: qsTr("Add all")
                    onClicked: app.library.enqueue("artist_id", root.artistId,
                                                   Library.AddToEnd)
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: qsTr("Albums") }
            TabButton { text: qsTr("All tracks") }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: tabs.currentIndex

            GridView {
                id: albumGrid
                model: root.albums
                cellWidth: theme.gridCell
                cellHeight: theme.gridCell + theme.rowHeight + theme.spacing
                clip: true
                cacheBuffer: theme.gridCell * 4
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: cell
                    required property int index
                    required property string itemId
                    required property string title
                    required property string coverId
                    required property int year

                    width: albumGrid.cellWidth - theme.spacing
                    height: albumGrid.cellHeight - theme.spacing

                    Component.onCompleted: root.albums.ensureLoaded(index)

                    Artwork {
                        id: cover
                        width: parent.width
                        height: parent.width
                        coverId: cell.coverId
                        requestSize: 300
                    }

                    Column {
                        anchors.top: cover.bottom
                        anchors.topMargin: 4
                        width: parent.width

                        Label {
                            width: parent.width
                            text: cell.title
                            color: theme.textPrimary
                            font.pixelSize: theme.fontNormal
                            elide: Text.ElideRight
                        }
                        Label {
                            width: parent.width
                            text: cell.year > 0 ? cell.year : ""
                            color: theme.textMuted
                            font.pixelSize: theme.fontSmall
                        }
                    }

                    TapHandler {
                        onTapped: root.openAlbum(cell.itemId, cell.title, root.artistName,
                                                 root.artistId, cell.coverId, cell.year)
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            rowMenu.selectorKey = "album_id"
                            rowMenu.selectorValue = cell.itemId
                            rowMenu.selectorFilters = []
                            rowMenu.popup()
                        }
                    }
                }
            }

            ListView {
                id: trackList
                model: root.tracks
                clip: true
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: row
                    required property int index
                    required property string itemId
                    required property string title
                    required property double duration
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
                            Layout.fillWidth: true
                            text: row.loaded ? row.title : "…"
                            color: theme.textPrimary
                            font.pixelSize: theme.fontNormal
                            elide: Text.ElideRight
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
}
