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
    minimumWidth: 760
    minimumHeight: 500
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
    Shortcut {
        sequence: "Esc"
        onActivated: {
            if (searchField.activeFocus)
                searchField.focus = false
            else if (stack.depth > 1)
                stack.pop()
        }
    }

    MiniPlayer { id: miniPlayer; visible: false }

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
        SettingsView {}
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
                Layout.preferredWidth: app.settings.railCollapsed ? theme.rowHeight * 1.6
                                                                  : theme.railWidth
                Layout.fillHeight: true
                color: theme.surfaceRaised

                Behavior on Layout.preferredWidth {
                    NumberAnimation { duration: theme.animation }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 6
                    spacing: 1

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            visible: !app.settings.railCollapsed
                            placeholderText: qsTr("Search…")
                            text: app.search.term
                            onTextEdited: {
                                app.search.term = text
                                if (text.length > 0)
                                    root.selectView("search")
                            }
                        }

                        IconButton {
                            glyph: theme.iconSearch
                            glyphSize: theme.fontNormal
                            tooltip: qsTr("Collapse the sidebar")
                            visible: app.settings.railCollapsed
                            onClicked: {
                                app.settings.railCollapsed = false
                                searchField.forceActiveFocus()
                            }
                        }
                    }

                    Item { Layout.preferredHeight: theme.spacing }

                    Repeater {
                        model: [
                            { "id": "nowplaying", "label": qsTr("Now Playing"), "glyph": theme.iconMusic },
                            { "id": "artists",    "label": qsTr("Artists"),     "glyph": theme.iconMusic },
                            { "id": "albums",     "label": qsTr("Albums"),      "glyph": theme.iconGrid },
                            { "id": "genres",     "label": qsTr("Genres"),      "glyph": theme.iconList },
                            { "id": "years",      "label": qsTr("Years"),       "glyph": theme.iconList },
                            { "id": "playlists",  "label": qsTr("Playlists"),   "glyph": theme.iconQueue },
                            { "id": "folders",    "label": qsTr("Folders"),     "glyph": theme.iconFolder },
                            { "id": "new",        "label": qsTr("New Music"),   "glyph": theme.iconAdd },
                            { "id": "random",     "label": qsTr("Random Album"), "glyph": theme.iconShuffle },
                            { "id": "queue",      "label": qsTr("Queue"),       "glyph": theme.iconQueue }
                        ]

                        delegate: ItemDelegate {
                            id: railItem
                            required property var modelData
                            readonly property bool current: root.currentView === modelData.id

                            Layout.fillWidth: true
                            height: theme.rowHeight
                            // See Theme.qml's metrics: an ItemDelegate hides
                            // 12 px of padding above and below its content.
                            topPadding: 0
                            bottomPadding: 0
                            Accessible.name: modelData.label

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
                                    font.pixelSize: theme.fontNormal
                                    color: railItem.current ? theme.accent : theme.textMuted
                                    Layout.preferredWidth: 18
                                    horizontalAlignment: Text.AlignHCenter
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
                        height: theme.rowHeight
                        // See Theme.qml's metrics: an ItemDelegate hides 12 px
                        // of padding above and below its content.
                        topPadding: 0
                        bottomPadding: 0
                        Accessible.name: qsTr("Settings")

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
                                color: theme.textMuted
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
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

                    ItemDelegate {
                        Layout.fillWidth: true
                        height: theme.rowHeight
                        // See Theme.qml's metrics: an ItemDelegate hides 12 px
                        // of padding above and below its content.
                        topPadding: 0
                        bottomPadding: 0
                        visible: !app.settings.railCollapsed
                        onClicked: app.settings.railCollapsed = true
                        contentItem: Label {
                            text: theme.iconBack
                            font.family: theme.iconFont
                            color: theme.textFaint
                            horizontalAlignment: Text.AlignHCenter
                        }
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

        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: theme.border }

        // ── Bottom bar: always visible, always this app's own playback.
        Rectangle {
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
                    coverId: app.player.coverId
                    requestSize: 150

                    TapHandler { onTapped: root.selectView("nowplaying") }
                }

                ColumnLayout {
                    Layout.preferredWidth: 220
                    Layout.maximumWidth: 320
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

                Label {
                    text: theme.duration(app.player.elapsed)
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    Layout.preferredWidth: 42
                    horizontalAlignment: Text.AlignRight
                }

                SeekBar { Layout.fillWidth: true }

                Label {
                    text: theme.duration(app.player.duration)
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    Layout.preferredWidth: 42
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
                IconButton {
                    glyph: theme.iconQueue
                    tooltip: qsTr("Queue")
                    onClicked: root.selectView("queue")
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
