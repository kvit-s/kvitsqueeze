// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// One screen for every browse list (prd.md §9.2 "Browse").
//
// Grid for things with artwork, list for everything else, both fed by the same
// BrowseModel — which is virtualized against the *server*, so this view scrolls
// a 50k-track list without knowing that it is one (prd.md FR-3.2).
//
// The breadcrumb is the accumulated filter set, which is what makes FR-3.4
// legible: "Genres › Rock › Nirvana" is one model with two filters, and every
// row's context menu queues exactly that selection.
Item {
    id: root

    // BrowseModel, created by LibraryController and owned by this page.
    required property var browseModel
    required property string heading
    property string subheading

    // Emitted instead of navigating, so the stack lives in one place.
    signal openBrowse(int kind, var filters, string heading)
    signal openAlbum(string albumId, string title, string artist, string artistId,
                     string coverId, int year)
    signal openArtist(string artistId, string name)

    Theme { id: theme }

    // Albums only. prd.md FR-3.3 asks for a grid "with artwork", and LMS has
    // no artist images in a local library — a grid of artists is 200 identical
    // placeholder squares, which is worse than a list at every window size.
    readonly property bool gridCapable: browseModel && browseModel.kind === Library.Albums
    readonly property bool gridMode: gridCapable && app.settings.albumGridView

    // Years, and only years. A year is four characters, and a full-width row
    // per year makes a library spanning 1965 to 2024 a sixty-screen column of
    // whitespace with a number at the left edge. Columns are what a list of
    // short, fixed-width, self-ordering labels wants.
    //
    // Not applied to genres, which look similar and are not: a genre is a
    // phrase of unpredictable length, and the moment one elides, a grid of
    // them is worse than a list.
    readonly property bool columnMode: browseModel && browseModel.kind === Library.Years

    // Rows that have a cover worth showing as a thumbnail.
    readonly property bool rowArtwork: browseModel
        && (browseModel.kind === Library.Albums || browseModel.kind === Library.Tracks
            || browseModel.kind === Library.PlaylistTracks)

    function activate(index) {
        if (!browseModel)
            return
        const kind = browseModel.kind
        const filters = browseModel.childFilters(index)
        const id = browseModel.selectorValue(index)
        if (!id)
            return

        const item = browseModel.get(index)

        switch (kind) {
        case Library.Artists:
            root.openArtist(id, item.title)
            break
        case Library.Albums:
            root.openAlbum(id, item.title, item.subtitle, item.artistId,
                           item.coverId, item.year)
            break
        case Library.Genres:
            root.openBrowse(Library.Artists, filters, item.title)
            break
        case Library.Years:
            root.openBrowse(Library.Albums, filters, item.title)
            break
        case Library.Playlists:
            root.openBrowse(Library.PlaylistTracks, filters, item.title)
            break
        case Library.Folder:
            if (item.isFolder)
                root.openBrowse(Library.Folder, filters, item.title)
            else
                app.library.enqueueFiltered(filters, Library.PlayNow)
            break
        default:
            // A track row plays. Everything else about a track is on the menu.
            app.library.enqueueFiltered(filters, Library.PlayNow)
            break
        }
    }

    QueueMenu { id: rowMenu }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header: title, the filter breadcrumb, and the view switch.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: header.implicitHeight + theme.margin * 2
            color: theme.surface

            RowLayout {
                id: header
                anchors.fill: parent
                anchors.margins: theme.margin
                spacing: theme.spacing

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2

                    Label {
                        text: root.heading
                        color: theme.textPrimary
                        font.pixelSize: theme.fontLarge
                        font.bold: true
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Label {
                        text: {
                            if (!root.browseModel)
                                return ""
                            if (!root.browseModel.loaded)
                                return qsTr("Loading…")
                            const count = qsTr("%n item(s)", "", root.browseModel.total)
                            return root.subheading ? root.subheading + "  ·  " + count
                                                   : count
                        }
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }
                }

                BusyIndicator {
                    running: root.browseModel ? root.browseModel.loading : false
                    visible: running
                    implicitWidth: theme.rowHeight
                    implicitHeight: theme.rowHeight
                }

                IconButton {
                    visible: root.gridCapable
                    glyph: root.gridMode ? theme.iconList : theme.iconGrid
                    tooltip: root.gridMode ? qsTr("Show as a list") : qsTr("Show as a grid")
                    onClicked: app.settings.albumGridView = !app.settings.albumGridView
                }

                IconButton {
                    glyph: theme.iconPlay
                    tooltip: qsTr("Play everything here")
                    onClicked: app.library.enqueueFiltered(
                                   root.browseModel ? root.browseModel.filters : [],
                                   Library.PlayNow)
                }
            }
        }

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        // ── The list itself.
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            Label {
                anchors.centerIn: parent
                visible: root.browseModel && root.browseModel.loaded
                         && root.browseModel.total === 0
                text: qsTr("Nothing here.")
                color: theme.textMuted
            }

            GridView {
                id: grid
                anchors.fill: parent
                anchors.margins: theme.margin
                visible: root.gridMode
                model: visible ? root.browseModel : null
                cellWidth: theme.gridCell
                cellHeight: theme.gridCell + theme.rowHeight + theme.spacing
                clip: true
                cacheBuffer: theme.gridCell * 4
                focus: visible
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: cell
                    required property int index
                    required property string title
                    required property string subtitle
                    required property string coverId
                    required property bool loaded

                    width: grid.cellWidth - theme.spacing
                    height: grid.cellHeight - theme.spacing

                    // Asking for the page is the delegate's job: data() is
                    // called while painting and must not start I/O.
                    Component.onCompleted: root.browseModel.ensureLoaded(index)

                    Artwork {
                        id: cover
                        width: parent.width
                        height: parent.width
                        coverId: cell.coverId
                        requestSize: 300
                    }

                    Rectangle {
                        anchors.fill: cover
                        color: theme.accent
                        opacity: hover.hovered ? 0.16 : 0
                        Behavior on opacity { NumberAnimation { duration: theme.animation } }
                    }

                    Column {
                        anchors.top: cover.bottom
                        anchors.topMargin: 4
                        width: parent.width
                        spacing: 0

                        Label {
                            width: parent.width
                            text: cell.loaded ? cell.title : "…"
                            color: theme.textPrimary
                            font.pixelSize: theme.fontNormal
                            elide: Text.ElideRight
                        }
                        Label {
                            width: parent.width
                            text: cell.subtitle
                            visible: text.length > 0
                            color: theme.textMuted
                            font.pixelSize: theme.fontSmall
                            elide: Text.ElideRight
                        }
                    }

                    HoverHandler { id: hover }

                    TapHandler {
                        onTapped: root.activate(cell.index)
                    }
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            rowMenu.selectorFilters = root.browseModel.childFilters(cell.index)
                            rowMenu.subject = cell.title
                            rowMenu.popup()
                        }
                    }
                }

                Keys.onReturnPressed: root.activate(grid.currentIndex)
            }

            // ── Years, in columns.
            //
            // Its own GridView rather than a mode of the artwork one above:
            // that grid's cell is a square cover with two lines under it, and
            // everything about it — cellHeight, the Artwork, the request size
            // — would need a branch. This one is rows in columns.
            //
            // The column count is derived from the width rather than fixed, so
            // a narrow window gets two and a wide one gets eight, and the cell
            // takes the exact remainder: an integer division that discards the
            // remainder leaves a ragged strip down the right-hand edge that
            // looks like a layout bug.
            GridView {
                id: columns
                anchors.fill: parent
                anchors.margins: theme.margin
                visible: root.columnMode
                model: visible ? root.browseModel : null

                // Wide enough for a four-digit year, its hover ground and the
                // gap to the next one, and no wider — a year needs no room to
                // grow, so the only thing extra width buys is fewer columns.
                readonly property int minColumnWidth: theme.compact ? 88 : 108
                readonly property int columnCount:
                    Math.max(1, Math.floor(width / minColumnWidth))
                cellWidth: width / columnCount
                cellHeight: theme.rowHeight

                clip: true
                cacheBuffer: theme.rowHeight * 40
                focus: visible
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: yearCell
                    required property int index
                    required property string title
                    required property bool loaded

                    width: columns.cellWidth
                    height: columns.cellHeight
                    // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                    // padding above and below its content.
                    topPadding: 0
                    bottomPadding: 0

                    Component.onCompleted: root.browseModel.ensureLoaded(index)

                    background: Rectangle {
                        color: yearCell.hovered ? theme.surfaceHover : "transparent"
                        radius: theme.radius
                    }

                    contentItem: Label {
                        text: yearCell.loaded ? yearCell.title : "…"
                        color: theme.textPrimary
                        font.pixelSize: theme.fontNormal
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    onClicked: root.activate(yearCell.index)

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            rowMenu.selectorFilters =
                                root.browseModel.childFilters(yearCell.index)
                            rowMenu.subject = yearCell.title
                            rowMenu.popup()
                        }
                    }
                }

                Keys.onReturnPressed: root.activate(columns.currentIndex)
            }

            ListView {
                id: list
                anchors.fill: parent
                visible: !root.gridMode && !root.columnMode
                model: visible ? root.browseModel : null
                clip: true
                cacheBuffer: theme.rowHeight * 20
                focus: visible
                ScrollBar.vertical: ScrollBar {}

                delegate: ItemDelegate {
                    id: row
                    required property int index
                    required property string title
                    required property string subtitle
                    required property string coverId
                    required property double duration
                    required property bool isFolder
                    required property bool loaded

                    width: ListView.view.width
                    height: root.rowArtwork ? theme.trackHeight + 8 : theme.rowHeight

                    // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                    // padding above and below its content.
                    topPadding: 0
                    bottomPadding: 0

                    Component.onCompleted: root.browseModel.ensureLoaded(index)

                    background: Rectangle {
                        color: row.hovered ? theme.surfaceHover : "transparent"
                    }

                    contentItem: RowLayout {
                        spacing: theme.spacing

                        Artwork {
                            visible: root.rowArtwork
                            Layout.preferredWidth: row.height - 6
                            Layout.preferredHeight: row.height - 6
                            coverId: row.coverId
                            requestSize: 100
                        }

                        Label {
                            visible: row.isFolder
                            text: theme.iconFolder
                            font.family: theme.iconFont
                            color: theme.textMuted
                        }

                        Label {
                            Layout.fillWidth: true
                            text: row.loaded ? row.title : "…"
                            color: theme.textPrimary
                            font.pixelSize: theme.fontNormal
                            elide: Text.ElideRight
                        }

                        Label {
                            text: row.subtitle
                            visible: text.length > 0
                            color: theme.textMuted
                            font.pixelSize: theme.fontSmall
                            elide: Text.ElideRight
                            Layout.maximumWidth: row.width * 0.3
                        }

                        Label {
                            text: theme.duration(row.duration)
                            visible: row.duration > 0
                            color: theme.textMuted
                            font.pixelSize: theme.fontSmall
                        }
                    }

                    onClicked: root.activate(row.index)

                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onTapped: {
                            rowMenu.selectorFilters = root.browseModel.childFilters(row.index)
                            rowMenu.subject = row.title
                            rowMenu.popup()
                        }
                    }
                }

                Keys.onReturnPressed: root.activate(list.currentIndex)
            }
        }
    }
}
