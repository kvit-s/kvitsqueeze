// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// The shell (prd.md §9.1): left rail, content pane with history, and a bottom
// bar that is always visible and always this app's own playback.
//
// Two things about it are load-bearing rather than stylistic:
//
//   * **No player section anywhere.** The rail lists places in the library and
//     nothing else. There is one player, it is this app, and the chrome spends
//     nothing on saying so (prd.md N5, FR-6.2).
//   * **The rail is the complete set of entry points.** Now Playing, Artists,
//     Albums, Genres, Years, Playlists, Folders, New Music, Random Album,
//     Queue, Search, Settings — that is all of prd.md FR-3.1, and there is no
//     plugin menu, radio or favourites section missing from it (prd.md N4).
ApplicationWindow {
    id: root

    width: 1180
    height: 760

    // Measured against the running app rather than reasoned about, by shrinking
    // it until something broke — 760 × 500 was the figure from before the
    // bottom bar was a bottom bar, and it had stopped being true.
    //
    //   * **Width** is set by the bottom bar, which is the only row that cannot
    //     wrap or scroll. Below 540 the queue button on its right end starts
    //     going under the window edge. That is with the title and the volume
    //     slider already dropped at 980 and the cover thumbnail at 700.
    //   * **Height** is set by the left rail, which is a fixed-length list with
    //     no scroll: below about 400 the Settings entry at its foot is cut in
    //     half by the seam. It cannot grow — prd.md N4 fixes the rail at these
    //     ten destinations — so this number is stable rather than lucky.
    //
    // Both carry ~20 px over the measurement, because the measurement is of
    // one font at one DPI.
    minimumWidth: 560
    minimumHeight: 420
    visible: true
    title: app.player.hasTrack
           ? app.player.title + " — " + app.player.artist + "  ·  SqeezeAmp"
           : qsTr("SqeezeAmp")

    Theme { id: theme }
    color: theme.surface

    // ── Navigation. One stack, pushed onto by every drill-down; the rail
    // replaces the stack rather than adding to it, so "Albums" is always one
    // click from anywhere (prd.md §9.1).
    property string currentView: app.settings.lastView

    function pushBrowse(kind, filters, heading) {
        stack.push(browsePage, { "kind": kind, "filters": filters, "heading": heading })
    }

    function pushAlbum(albumId, title, artist, artistId, coverId, year) {
        stack.push(albumPage, {
            "albumId": albumId, "albumTitle": title, "albumArtist": artist,
            "albumArtistId": artistId, "coverId": coverId, "year": year
        })
    }

    function pushArtist(artistId, name) {
        stack.push(artistPage, { "artistId": artistId, "artistName": name })
    }

    function selectView(name) {
        root.currentView = name
        app.settings.lastView = name
        stack.clear()

        switch (name) {
        case "nowplaying": stack.push(nowPlayingPage); break
        case "queue":      stack.push(queuePage); break
        case "search":     stack.push(searchPage); break
        case "settings":   stack.push(settingsPage); break
        case "diagnostics": stack.push(diagnosticsPage); break
        case "artists":    pushBrowse(Library.Artists, [], qsTr("Artists")); break
        case "albums":     pushBrowse(Library.Albums, [], qsTr("Albums")); break
        case "genres":     pushBrowse(Library.Genres, [], qsTr("Genres")); break
        case "years":      pushBrowse(Library.Years, [], qsTr("Years")); break
        case "playlists":  pushBrowse(Library.Playlists, [], qsTr("Playlists")); break
        case "folders":    pushBrowse(Library.Folder, [], qsTr("Music folder")); break
        case "new":        pushBrowse(Library.Albums, ["sort:new"], qsTr("New music")); break
        case "random":     pushBrowse(Library.Albums, ["sort:random"], qsTr("Random albums")); break
        default:           stack.push(nowPlayingPage); break
        }
    }

    Component.onCompleted: selectView(root.currentView)

    Connections {
        target: shell
        function onSettingsRequested() {
            root.show()
            root.selectView("settings")
        }
    }

    // ── prd.md FR-7.1: closing the window is not quitting. The player keeps
    // running in the tray, which is also what makes FR-1.7 true.
    onClosing: function (close) {
        if (app.settings.closeToTray && shell.trayAvailable) {
            close.accepted = false
            shell.hideWindow()
            shell.notifyStillRunning()
        } else {
            shell.quit()
        }
    }

    // ── prd.md FR-8.1. Every one of these is a window-level shortcut so it
    // works from whichever screen has focus.
    Shortcut {
        sequences: ["Space"]
        // Not while typing in the search box, where Space is a space.
        enabled: !searchField.activeFocus
        onActivated: app.player.playPause()
    }
    Shortcut { sequence: "Left";       onActivated: app.player.seekBy(-5) }
    Shortcut { sequence: "Right";      onActivated: app.player.seekBy(5) }
    Shortcut { sequence: "Ctrl+Left";  onActivated: app.player.previous() }
    Shortcut { sequence: "Ctrl+Right"; onActivated: app.player.next() }
    Shortcut { sequence: "Up";         onActivated: app.player.changeVolume(5) }
    Shortcut { sequence: "Down";       onActivated: app.player.changeVolume(-5) }
    Shortcut { sequence: "Ctrl+F";     onActivated: searchField.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+,";     onActivated: root.selectView("settings") }
    Shortcut { sequence: "Ctrl+U";     onActivated: root.selectView("queue") }
    Shortcut { sequence: "Ctrl+M";     onActivated: miniPlayer.visible = !miniPlayer.visible }

    // prd.md FR-3.9: a mix listener's most-used action deserves a key.
    // Ctrl+R starts a Song Mix; Ctrl+Shift+R stops whatever is running.
    Shortcut { sequence: "Ctrl+R";       onActivated: mixControl.start(Mix.Songs) }
    Shortcut { sequence: "Ctrl+Shift+R"; onActivated: mixControl.stop() }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            // The lyric sheet first: it is drawn over whatever is underneath,
            // so dismissing it is what Esc means while it is up (prd.md
            // FR-5.5).
            if (app.lyrics.open)
                app.lyrics.open = false
            else if (searchField.activeFocus)
                searchField.focus = false
            else if (stack.depth > 1)
                stack.pop()
        }
    }

    MiniPlayer { id: miniPlayer; visible: false }

    // One instance at window level, so the bottom bar and the shortcut share
    // the same menu and the same "replace the queue?" question.
    MixControl { id: mixControl }

    // ── Page components. Instantiated by the stack, one per visit, so a
    // BrowseModel dies with the page that owns it.
    Component {
        id: nowPlayingPage
        NowPlayingView {}
    }
    Component {
        id: queuePage
        QueueView {}
    }
    Component {
        id: settingsPage
        SettingsView {
            onOpenDiagnostics: root.selectView("diagnostics")
        }
    }
    Component {
        id: diagnosticsPage
        DiagnosticsView {}
    }
    Component {
        id: searchPage
        SearchView {
            onOpenAlbum: (albumId, title, artist, artistId, coverId, year) =>
                root.pushAlbum(albumId, title, artist, artistId, coverId, year)
            onOpenArtist: (artistId, name) => root.pushArtist(artistId, name)
        }
    }
    Component {
        id: browsePage
        BrowseView {
            id: page
            property int kind: Library.Albums
            property var filters: []
            browseModel: app.library.browse(kind, filters)
            onOpenBrowse: (childKind, childFilters, childHeading) =>
                root.pushBrowse(childKind, childFilters, childHeading)
            onOpenAlbum: (albumId, title, artist, artistId, coverId, year) =>
                root.pushAlbum(albumId, title, artist, artistId, coverId, year)
            onOpenArtist: (artistId, name) => root.pushArtist(artistId, name)
        }
    }
    Component {
        id: albumPage
        AlbumView {
            onOpenArtist: (artistId, name) => root.pushArtist(artistId, name)
        }
    }
    Component {
        id: artistPage
        ArtistView {
            onOpenAlbum: (albumId, title, artist, artistId, coverId, year) =>
                root.pushAlbum(albumId, title, artist, artistId, coverId, year)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Connection banner (prd.md FR-1.5): non-modal, non-blocking, and
        // present only when there is something to say.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: app.connectionMessage ? theme.rowHeight : 0
            visible: height > 0
            clip: true
            color: theme.warning

            Behavior on Layout.preferredHeight {
                NumberAnimation { duration: theme.animation }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: theme.margin
                anchors.rightMargin: theme.margin
                spacing: theme.spacing

                Label {
                    Layout.fillWidth: true
                    text: app.connectionMessage
                    color: "#101216"
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
                Button {
                    text: qsTr("Settings")
                    flat: true
                    onClicked: root.selectView("settings")
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Left rail.
            Rectangle {
                id: rail

                Layout.preferredWidth: app.settings.railCollapsed ? theme.rowHeight * 1.6
                                                                  : theme.railWidth
                Layout.fillHeight: true
                color: theme.surfaceRaised

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: theme.animation }
                }

                // ── How tall a rail entry is, and how far it sits from the
                // next one. Both are shared out from the height the window
                // happens to have, because the rail is a fixed list with no
                // scroll: eleven entries either fit or they are drawn under
                // the bottom bar, and a constant that fits a 760 px window
                // loses Queue and Settings in a 460 px one.
                //
                // So the height is claimed first, up to theme.rowHeight, and
                // the gap takes what is left up to theme.railGap. In a tall
                // window that is full rows with a full gap between them; as
                // the window shrinks the rows give way first, then the gap,
                // and only a window shorter than the minimum runs out of both.
                // The floor is one glyph plus its bearing — below that a row
                // is not a row.
                readonly property int railRows: 11          // ten destinations, plus Settings
                readonly property int railGaps: 14          // every gap the column draws
                readonly property int railHeadHeight:
                    railHead.implicitHeight + theme.spacing
                    + (app.settings.railCollapsed ? collapsedSearch.implicitHeight
                                                  : theme.rowHeight)
                // Free of the column's own spacing on purpose: this reads the
                // rail's height, which the row above sets, so nothing here
                // depends on the values it is about to produce.
                readonly property int railFree: height - 12 - railHeadHeight
                readonly property int railRowHeight:
                    Math.max(theme.fontLarge + 4,
                             Math.min(theme.rowHeight,
                                      Math.floor(railFree / railRows) - theme.railGap))
                readonly property int railRowGap:
                    Math.max(1, Math.min(theme.railGap,
                                         Math.floor((railFree - railRows * railRowHeight)
                                                    / railGaps)))

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: rail.railRowGap

                    // ── The collapse control sits above the search box rather
                    // than at the foot of the rail. A chevron at the bottom of
                    // a list of destinations reads as an eleventh destination;
                    // one in the head reads as "this panel folds", which is
                    // what it does. Right-aligned so it lands on the edge that
                    // moves.
                    RowLayout {
                        id: railHead

                        Layout.fillWidth: true
                        spacing: 0

                        Item {
                            Layout.fillWidth: true
                            visible: !app.settings.railCollapsed
                        }

                        IconButton {
                            glyph: app.settings.railCollapsed ? theme.chevronExpand
                                                              : theme.chevronCollapse
                            glyphFont: Qt.application.font.family
                            glyphSize: theme.fontLarge
                            baseColor: theme.textFaint
                            tooltip: app.settings.railCollapsed ? qsTr("Expand the sidebar")
                                                                : qsTr("Collapse the sidebar")
                            onClicked: app.settings.railCollapsed = !app.settings.railCollapsed
                        }
                    }

                    // ── Search. Rounded with the magnifier inside it, which is
                    // what a search box looks like everywhere else — a bare
                    // square TextField in the corner of a rail reads as a
                    // filter for whatever is under it.
                    TextField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.preferredHeight: theme.rowHeight
                        visible: !app.settings.railCollapsed
                        placeholderText: qsTr("Search")
                        text: app.search.term
                        leftPadding: theme.rowHeight
                        rightPadding: theme.spacing
                        topPadding: 0
                        bottomPadding: 0
                        verticalAlignment: TextInput.AlignVCenter
                        color: theme.textPrimary
                        placeholderTextColor: theme.textFaint
                        font.pixelSize: theme.fontNormal

                        background: Rectangle {
                            radius: height / 2
                            color: theme.surface
                            border.width: 1
                            border.color: searchField.activeFocus ? theme.accent
                                                                  : theme.border
                        }

                        Label {
                            x: theme.rowHeight / 2 - width / 2
                            anchors.verticalCenter: parent.verticalCenter
                            text: theme.iconSearch
                            font.family: theme.iconFont
                            font.pixelSize: theme.fontNormal
                            color: theme.textFaint
                        }

                        onTextEdited: {
                            app.search.term = text
                            if (text.length > 0)
                                root.selectView("search")
                        }
                    }

                    // Collapsed, the box becomes the thing it had inside it.
                    IconButton {
                        id: collapsedSearch

                        Layout.alignment: Qt.AlignHCenter
                        glyph: theme.iconSearch
                        glyphSize: theme.fontNormal
                        tooltip: qsTr("Search")
                        visible: app.settings.railCollapsed
                        onClicked: {
                            app.settings.railCollapsed = false
                            searchField.forceActiveFocus()
                        }
                    }

                    Item { Layout.preferredHeight: theme.spacing }

                    Repeater {
                        // One distinct glyph each — see Theme.qml on why these
                        // were chosen by rendering the font. Collapsed, the
                        // glyph is the whole control, so two entries sharing
                        // one is two entries that cannot be told apart.
                        model: [
                            { "id": "nowplaying", "label": qsTr("Now Playing"), "glyph": theme.iconNowPlaying },
                            { "id": "artists",    "label": qsTr("Artists"),     "glyph": theme.iconArtists },
                            { "id": "albums",     "label": qsTr("Albums"),      "glyph": theme.iconAlbums },
                            { "id": "genres",     "label": qsTr("Genres"),      "glyph": theme.iconGenres },
                            { "id": "years",      "label": qsTr("Years"),       "glyph": theme.iconYears },
                            { "id": "playlists",  "label": qsTr("Playlists"),   "glyph": theme.iconPlaylists },
                            { "id": "folders",    "label": qsTr("Folders"),     "glyph": theme.iconFolder },
                            { "id": "new",        "label": qsTr("New Music"),   "glyph": theme.iconNew },
                            { "id": "random",     "label": qsTr("Random Album"), "glyph": theme.iconShuffle },
                            { "id": "queue",      "label": qsTr("Queue"),       "glyph": theme.iconQueue }
                        ]

                        delegate: ItemDelegate {
                            id: railItem
                            required property var modelData
                            readonly property bool current: root.currentView === modelData.id

                            Layout.fillWidth: true
                            // `height` here would be ignored: a ColumnLayout
                            // sizes its children itself and reads
                            // Layout.preferredHeight, falling back to the
                            // implicit height — which for one glyph is about
                            // 20 px. That is why the rail drew rows two thirds
                            // of the height this file appeared to ask for.
                            Layout.preferredHeight: rail.railRowHeight
                            // See Theme.qml's metrics: an ItemDelegate hides
                            // 12 px of padding above and below its content.
                            topPadding: 0
                            bottomPadding: 0
                            Accessible.name: modelData.label

                            // Collapsed, the label is gone and the glyph has to
                            // carry the whole meaning — which no glyph does on
                            // its own. Expanded, the name is already on screen
                            // and a tooltip repeating it is noise.
                            ToolTip.visible: railItem.hovered && app.settings.railCollapsed
                            ToolTip.text: railItem.modelData.label
                            ToolTip.delay: 400

                            background: Rectangle {
                                radius: theme.radius
                                color: railItem.current ? theme.surfaceOverlay
                                     : railItem.hovered ? theme.surfaceHover : "transparent"
                            }

                            contentItem: RowLayout {
                                spacing: theme.spacing
                                Label {
                                    text: railItem.modelData.glyph
                                    font.family: theme.iconFont
                                    font.pixelSize: theme.fontLarge
                                    color: railItem.current ? theme.accent : theme.textMuted
                                    Layout.preferredWidth: 22
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                }
                                Label {
                                    Layout.fillWidth: true
                                    visible: !app.settings.railCollapsed
                                    text: railItem.modelData.label
                                    color: railItem.current ? theme.textPrimary : theme.textMuted
                                    font.pixelSize: theme.fontNormal
                                    elide: Text.ElideRight
                                }
                            }

                            onClicked: root.selectView(railItem.modelData.id)
                        }
                    }

                    Item { Layout.fillHeight: true }

                    ItemDelegate {
                        id: settingsItem
                        Layout.fillWidth: true
                        // Layout.preferredHeight, not height — see the rail
                        // delegate above.
                        Layout.preferredHeight: rail.railRowHeight
                        // See Theme.qml's metrics: an ItemDelegate hides 12 px
                        // of padding above and below its content.
                        topPadding: 0
                        bottomPadding: 0
                        Accessible.name: qsTr("Settings")

                        ToolTip.visible: settingsItem.hovered && app.settings.railCollapsed
                        ToolTip.text: qsTr("Settings")
                        ToolTip.delay: 400

                        background: Rectangle {
                            radius: theme.radius
                            color: root.currentView === "settings" ? theme.surfaceOverlay
                                 : settingsItem.hovered ? theme.surfaceHover : "transparent"
                        }
                        contentItem: RowLayout {
                            spacing: theme.spacing
                            Label {
                                text: theme.iconSettings
                                font.family: theme.iconFont
                                font.pixelSize: theme.fontLarge
                                color: theme.textMuted
                                Layout.preferredWidth: 22
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: !app.settings.railCollapsed
                                text: qsTr("Settings")
                                color: theme.textMuted
                                font.pixelSize: theme.fontNormal
                            }
                        }
                        onClicked: root.selectView("settings")
                    }
                }
            }

            Rectangle { Layout.preferredWidth: 1; Layout.fillHeight: true; color: theme.border }

            // ── Content pane with back/forward history.
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                StackView {
                    id: stack
                    anchors.fill: parent

                    // prd.md §9.3: ≤ 150 ms and nothing that delays interaction.
                    pushEnter: Transition {
                        NumberAnimation { property: "opacity"; from: 0; to: 1
                                          duration: theme.animation }
                    }
                    pushExit: Transition {}
                    popEnter: Transition {}
                    popExit: Transition {
                        NumberAnimation { property: "opacity"; from: 1; to: 0
                                          duration: theme.animation }
                    }
                }

                IconButton {
                    anchors.left: parent.left
                    anchors.top: parent.top
                    anchors.margins: 4
                    visible: stack.depth > 1
                    glyph: theme.iconBack
                    tooltip: qsTr("Back")
                    onClicked: stack.pop()
                }
            }
        }

        // ── The seam between the content pane and the bottom bar *is* the
        // progress bar (prd.md FR-5.1/5.2). There is exactly one in the shell
        // and this is it: a 1 px border was going to be drawn here regardless,
        // and it spans the whole window, which makes it both the least
        // intrusive place to put the position and the easiest to hit.
        SeekBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 12
        }

        // ── Bottom bar: always visible, always this app's own playback, and
        // since the transport moved down here it is the only place several of
        // these controls exist. That makes what it drops when the window is
        // narrow a real decision rather than a cosmetic one.
        //
        // A RowLayout that runs out of room shrinks its `fillWidth` children
        // first, and the seek bar is the only one of those — so at 890 px it
        // went to zero width while a track title nobody needed kept its 200.
        // Below `narrow` the two readouts that are duplicated elsewhere give
        // way (the title, which the window title also carries; the volume
        // slider, which Up/Down and the mute button cover) and the seek bar has
        // a floor it cannot be pushed below.
        Rectangle {
            id: bar
            readonly property bool narrow: root.width < 980
            // Second threshold. Below this the cover thumbnail goes too: it is
            // the largest single item in the bar, it is decoration rather than
            // a control, and the only thing it does — click for Now Playing —
            // the rail also does.
            readonly property bool tiny: root.width < 700

            Layout.fillWidth: true
            Layout.preferredHeight: theme.compact ? 58 : 72
            color: theme.surfaceRaised

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: theme.spacing
                anchors.rightMargin: theme.spacing
                spacing: theme.spacing

                Artwork {
                    Layout.preferredWidth: parent.height - 16
                    Layout.preferredHeight: parent.height - 16
                    visible: !bar.tiny
                    coverId: app.player.coverId
                    requestSize: 150

                    TapHandler { onTapped: root.selectView("nowplaying") }
                }

                // First to go when the window is narrow, because the window
                // title and the Now Playing screen both already say it.
                ColumnLayout {
                    Layout.preferredWidth: 200
                    Layout.minimumWidth: 0
                    Layout.maximumWidth: 300
                    visible: !bar.narrow
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: app.player.hasTrack ? app.player.title
                                                  : qsTr("Nothing playing")
                        color: theme.textPrimary
                        font.pixelSize: theme.fontNormal
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        text: app.player.artist ? app.player.artist : "—"
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                        elide: Text.ElideRight
                    }
                }

                // Two equal-weight spacers around the transport, which is what
                // centres it. Before the seek bar moved out to the seam it was
                // the only `fillWidth` child and it absorbed all the slack; now
                // nothing does, and without these the whole bar packs left and
                // leaves a hole on the right.
                Item { Layout.fillWidth: true }

                // The whole transport lives here and nowhere else. Now Playing
                // used to carry a second copy of it, one scroll away from this
                // one and doing the same thing — so shuffle and repeat came
                // down here with the rest rather than being left behind on a
                // screen that no longer has controls.
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
                    tooltip: qsTr("Previous")
                    onClicked: app.player.previous()
                }
                IconButton {
                    glyph: app.player.playing ? theme.iconPause : theme.iconPlay
                    glyphSize: theme.fontLarge
                    tooltip: app.player.playing ? qsTr("Pause") : qsTr("Play")
                    onClicked: app.player.playPause()
                }
                IconButton {
                    glyph: theme.iconNext
                    tooltip: qsTr("Next")
                    onClicked: app.player.next()
                }
                IconButton {
                    glyph: app.player.repeatMode === 1 ? theme.iconRepeatOne
                                                       : theme.iconRepeatAll
                    active: app.player.repeatMode !== 0
                    tooltip: app.player.repeatMode === 0 ? qsTr("Repeat off")
                           : app.player.repeatMode === 1 ? qsTr("Repeat one")
                                                         : qsTr("Repeat all")
                    onClicked: app.player.cycleRepeat()
                }

                Item { Layout.fillWidth: true }

                // The clocks stayed when the bar left. They are the one part of
                // a progress readout that a line on the seam cannot draw, and
                // they are directly under it.
                Label {
                    text: theme.duration(app.player.elapsed) + "  /  "
                          + theme.duration(app.player.duration)
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    horizontalAlignment: Text.AlignHCenter
                    Layout.preferredWidth: 92
                }

                // prd.md FR-2.6: server-side volume, so the level stays in step
                // with every other controller on the network.
                IconButton {
                    glyph: app.player.muted ? theme.iconMute : theme.iconVolume
                    tooltip: app.player.muted ? qsTr("Unmute") : qsTr("Mute")
                    onClicked: app.player.setMuted(!app.player.muted)
                }
                Slider {
                    Layout.preferredWidth: theme.compact ? 80 : 110
                    visible: !bar.narrow
                    from: 0
                    to: 100
                    value: Math.max(0, app.player.volume)
                    enabled: app.player.volume >= 0
                    onMoved: app.player.setVolume(Math.round(value))
                }

                IconButton {
                    glyph: theme.iconPower
                    active: !app.player.powered
                    tooltip: app.player.powered ? qsTr("Power off") : qsTr("Power on")
                    onClicked: app.player.togglePower()
                }
                // prd.md FR-3.9. In the bottom bar rather than the rail
                // because a mix is something you reach for while listening,
                // from whichever screen you happen to be on — and because the
                // rail is the library's entry points, which a mix is not.
                IconButton {
                    glyph: theme.iconMix
                    active: app.mix.active
                    tooltip: app.mix.active
                             ? qsTr("%1 is running").arg(app.mix.mixName)
                             : qsTr("Random mix")
                    onClicked: mixControl.openMenu()
                }
                // A toggle, not a one-way door. Clicking Queue and then
                // clicking it again to get back to the cover is what everyone
                // tries first; without this the only way back is the rail,
                // which is collapsed by default.
                IconButton {
                    glyph: theme.iconQueue
                    active: root.currentView === "queue"
                    tooltip: root.currentView === "queue" ? qsTr("Back to Now Playing")
                                                          : qsTr("Queue")
                    onClicked: root.selectView(root.currentView === "queue"
                                               ? "nowplaying" : "queue")
                }
            }
        }
    }

    // prd.md FR-6.3: a powered-off player is shown as powered off, not as a
    // player that has stopped responding.
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: app.player.powered ? 0 : 0.35
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: theme.animation } }

        MouseArea {
            anchors.fill: parent
            enabled: !app.player.powered
            onClicked: app.player.setPower(true)
        }

        Label {
            anchors.centerIn: parent
            text: qsTr("This player is powered off.\nClick anywhere to turn it on.")
            horizontalAlignment: Text.AlignHCenter
            color: "#ffffff"
            font.pixelSize: theme.fontLarge
        }
    }
}
