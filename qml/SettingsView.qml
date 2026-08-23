// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Sqz

// prd.md §9.2 "Settings": Server, Player, Interface, Shortcuts, Diagnostics,
// About/licences.
//
// There is no player *selection* anywhere on this screen, and there never will
// be. Every control here configures the one player this process is
// (prd.md N5/FR-6.2), which is why "Player" is a section of settings rather
// than a thing to pick.
Item {
    id: root

    // Diagnostics is a screen rather than a section: it is a live 2000-row ring
    // buffer with its own pause and clear, and nesting that inside this
    // Flickable would put a scroller inside a scroller. The section below is
    // the way in, which is what §9.2 is actually asking for.
    signal openDiagnostics()

    Theme { id: theme }

    component SectionHeader: Label {
        Layout.fillWidth: true
        Layout.topMargin: theme.margin
        color: theme.accent
        font.pixelSize: theme.fontNormal
        font.bold: true
    }

    component Hint: Label {
        Layout.fillWidth: true
        color: theme.textMuted
        font.pixelSize: theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: form.implicitHeight + theme.margin * 4
        clip: true
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: form
            x: theme.margin * 2
            width: Math.min(parent.width - theme.margin * 4, 720)
            spacing: theme.spacing

            Label {
                text: qsTr("Settings")
                color: theme.textPrimary
                font.pixelSize: theme.fontHuge
                font.bold: true
                Layout.topMargin: theme.margin
            }

            // ── Server (prd.md FR-1.1, FR-1.2, FR-1.3)
            SectionHeader { text: qsTr("Server") }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: theme.spacing
                rowSpacing: theme.spacing

                Label { text: qsTr("Host"); color: theme.textPrimary }
                TextField {
                    id: hostField
                    Layout.fillWidth: true
                    text: app.settings.serverHost
                    placeholderText: qsTr("hostname or IP address")
                    onEditingFinished: app.settings.serverHost = text
                }

                Label { text: qsTr("Port"); color: theme.textPrimary }
                SpinBox {
                    from: 1
                    to: 65535
                    editable: true
                    value: app.settings.serverPort
                    onValueModified: app.settings.serverPort = value

                    // A port is not a quantity. The default formatter applies
                    // the locale's grouping separator and offers the user
                    // "9,000".
                    textFromValue: (value) => String(value)
                    valueFromText: (text) => parseInt(text, 10)
                }

                Label { text: qsTr("Username"); color: theme.textPrimary }
                TextField {
                    id: userField
                    Layout.fillWidth: true
                    text: app.settings.serverUser
                    placeholderText: qsTr("only if the server requires it")
                }

                Label { text: qsTr("Password"); color: theme.textPrimary }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: passwordField
                        Layout.fillWidth: true
                        echoMode: TextInput.Password
                        placeholderText: app.settings.hasPassword
                                         ? qsTr("stored in Windows Credential Manager")
                                         : qsTr("not set")
                    }
                    Button {
                        text: qsTr("Save")
                        onClicked: {
                            app.settings.setCredentials(userField.text, passwordField.text)
                            passwordField.text = ""
                        }
                    }
                    Button {
                        text: qsTr("Clear")
                        enabled: app.settings.serverUser.length > 0
                        onClicked: {
                            app.settings.clearCredentials()
                            userField.text = ""
                            passwordField.text = ""
                        }
                    }
                }
            }

            Hint {
                text: app.settings.credentialStoreAvailable
                      ? qsTr("The password is stored in the Windows Credential Manager, "
                             + "never in the settings file.")
                      : qsTr("No credential store is available on this system, so the "
                             + "password cannot be saved.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    text: app.scanning ? qsTr("Scanning…") : qsTr("Find servers")
                    enabled: !app.scanning
                    onClicked: app.scanForServers()
                }
                Label {
                    Layout.fillWidth: true
                    text: app.connectionMessage ? app.connectionMessage
                                                : qsTr("Connected to %1 (Lyrion %2)")
                                                    .arg(app.settings.serverHost)
                                                    .arg(app.library.serverVersion)
                    color: app.connected ? theme.textMuted : theme.warning
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Repeater {
                model: app.discoveredServers
                delegate: ItemDelegate {
                    id: found
                    required property var modelData
                    Layout.fillWidth: true
                    height: theme.rowHeight
                    // See Theme.qml's metrics: an ItemDelegate hides 12 px of
                    // padding above and below its content.
                    topPadding: 0
                    bottomPadding: 0
                    text: modelData.name + "  ·  " + modelData.address
                          + ":" + modelData.port + "  ·  " + modelData.version
                    onClicked: {
                        app.useServer(found.modelData.address, found.modelData.port)
                        hostField.text = found.modelData.address
                    }
                }
            }

            Hint {
                visible: !app.scanning && app.discoveredServers.length === 0
                text: qsTr("Discovery uses a UDP broadcast, which does not cross a "
                           + "router. A server on another subnet — a Home Assistant "
                           + "add-on, for instance — has to be typed in above.")
            }

            // ── Player (prd.md FR-1.4, FR-2.3, FR-2.4, FR-2.8, FR-6.3)
            SectionHeader { text: qsTr("Player") }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: theme.spacing
                rowSpacing: theme.spacing

                Label { text: qsTr("Name on the server"); color: theme.textPrimary }
                TextField {
                    Layout.fillWidth: true
                    text: app.settings.playerName
                    onEditingFinished: app.settings.playerName = text
                }

                Label { text: qsTr("Output device"); color: theme.textPrimary }
                RowLayout {
                    Layout.fillWidth: true
                    ComboBox {
                        id: deviceBox
                        Layout.fillWidth: true
                        model: [qsTr("System default")].concat(app.engine.deviceNames)
                        currentIndex: {
                            const index = app.engine.deviceNames.indexOf(
                                            app.settings.outputDevice)
                            return index < 0 ? 0 : index + 1
                        }
                        onActivated: app.settings.outputDevice =
                                     index === 0 ? "" : app.engine.deviceNames[index - 1]
                    }
                    Button {
                        text: qsTr("Rescan")
                        onClicked: app.engine.refreshDevices()
                    }
                }

                Label { text: qsTr("Resampling"); color: theme.textPrimary }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Off"), qsTr("Fast"), qsTr("Balanced"),
                            qsTr("High"), qsTr("Very high")]
                    currentIndex: app.settings.resampleQuality
                    onActivated: app.settings.resampleQuality = index
                }
            }

            CheckBox {
                text: qsTr("Exclusive output (WASAPI)")
                checked: app.settings.exclusiveOutput
                onToggled: app.settings.exclusiveOutput = checked
            }
            Hint {
                text: qsTr("SqeezeAmp shares the audio device by default, so other "
                           + "applications stay audible and Windows handles any "
                           + "sample-rate conversion. Exclusive mode takes the device "
                           + "over completely and silences every other application "
                           + "on this PC.")
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing
                Button {
                    text: app.player.powered ? qsTr("Power off the player")
                                             : qsTr("Power on the player")
                    onClicked: app.player.togglePower()
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Engine: %1").arg(app.engine.stateText)
                          + (app.engine.lastError ? " — " + app.engine.lastError : "")
                    color: app.engine.lastError ? theme.danger : theme.textMuted
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Hint {
                visible: !app.engine.available
                text: qsTr("No audio engine was found. SqeezeAmp plays through "
                           + "squeezelite.exe, which the installer stages in the "
                           + "engine folder next to the application.")
            }

            // ── Interface (prd.md §9.3, FR-7.1, FR-7.6)
            SectionHeader { text: qsTr("Interface") }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: theme.spacing
                rowSpacing: theme.spacing

                Label { text: qsTr("Theme"); color: theme.textPrimary }
                ComboBox {
                    Layout.fillWidth: true
                    model: [qsTr("Follow Windows"), qsTr("Dark"), qsTr("Light")]
                    currentIndex: app.settings.theme
                    onActivated: app.settings.theme = index
                }
            }

            CheckBox {
                text: qsTr("Compact density")
                checked: app.settings.compactDensity
                onToggled: app.settings.compactDensity = checked
            }
            CheckBox {
                text: qsTr("Closing the window keeps playing in the tray")
                checked: app.settings.closeToTray
                onToggled: app.settings.closeToTray = checked
            }
            CheckBox {
                text: qsTr("Start with Windows")
                checked: app.settings.startWithWindows
                onToggled: app.settings.startWithWindows = checked
            }
            CheckBox {
                text: qsTr("Start hidden in the tray")
                checked: app.settings.startMinimized
                onToggled: app.settings.startMinimized = checked
            }

            Hint {
                visible: !shell.mediaKeysHeld
                text: qsTr("Windows gave at least one media key to another "
                           + "application. Close it and restart SqeezeAmp to take "
                           + "the keys back.")
            }

            // ── Pause while the microphone is in use (prd.md FR-7.11)
            //
            // Under Interface rather than Player: it is a preference about
            // when the app gets out of the way, not about how it plays.
            CheckBox {
                id: micPauseBox
                objectName: "micPauseCheckBox"
                enabled: shell.micWatchAvailable
                text: qsTr("Pause while the microphone is in use")
                checked: app.settings.pauseWhileMicInUse
                onToggled: app.settings.pauseWhileMicInUse = checked
            }

            Hint {
                visible: !shell.micWatchAvailable
                text: qsTr("Unavailable: Windows is not reporting any recording "
                           + "device on this machine.")
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: theme.margin
                visible: micPauseBox.checked && shell.micWatchAvailable
                spacing: theme.spacing

                Label {
                    text: qsTr("Wait before resuming")
                    color: theme.textPrimary
                }
                SpinBox {
                    from: 0
                    to: 15000
                    stepSize: 500
                    editable: true
                    value: app.settings.micResumeDelayMs
                    onValueModified: app.settings.micResumeDelayMs = value

                    // Seconds, because nobody thinks about this delay in
                    // milliseconds. Stored in milliseconds so the setting does
                    // not have to change shape if it ever needs finer steps.
                    textFromValue: (value) => qsTr("%1 s").arg((value / 1000).toFixed(1))
                    valueFromText: (text) => Math.round(parseFloat(text) * 1000)
                }
            }

            Hint {
                visible: micPauseBox.checked && shell.micWatchAvailable
                text: qsTr("Any application that opens the microphone pauses "
                           + "playback — Windows voice typing (Win+H), a call, a "
                           + "meeting. Playback resumes after the wait above, and "
                           + "only if nothing else has touched the player in the "
                           + "meantime.")
            }

            // ── Shortcuts (prd.md FR-8.1)
            SectionHeader { text: qsTr("Keyboard") }

            Repeater {
                model: [
                    { "what": qsTr("Play / pause"),             "keys": "Space" },
                    { "what": qsTr("Seek back / forward 5 s"),  "keys": "← / →" },
                    { "what": qsTr("Previous / next track"),    "keys": "Ctrl+← / Ctrl+→" },
                    { "what": qsTr("Volume up / down"),         "keys": "↑ / ↓" },
                    { "what": qsTr("Search"),                   "keys": "Ctrl+F" },
                    { "what": qsTr("Settings"),                 "keys": "Ctrl+," },
                    { "what": qsTr("Queue"),                    "keys": "Ctrl+U" },
                    { "what": qsTr("Mini player"),              "keys": "Ctrl+M" },
                    { "what": qsTr("Back"),                     "keys": "Esc" }
                ]
                delegate: RowLayout {
                    id: shortcutRow
                    required property var modelData
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: shortcutRow.modelData.what
                        color: theme.textPrimary
                        font.pixelSize: theme.fontSmall
                    }
                    Label {
                        text: shortcutRow.modelData.keys
                        color: theme.textMuted
                        font.pixelSize: theme.fontSmall
                    }
                }
            }

            // ── Library maintenance (prd.md FR-9.3, N3)
            SectionHeader { text: qsTr("Library") }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    text: qsTr("Rescan new and changed")
                    enabled: !app.library.rescanning
                    onClicked: app.library.rescan(false)
                }
                Button {
                    text: qsTr("Rescan playlists")
                    enabled: !app.library.rescanning
                    onClicked: app.library.rescan(true)
                }
                Label {
                    Layout.fillWidth: true
                    text: app.library.rescanning
                          ? qsTr("Scanning: %1").arg(app.library.rescanDetail)
                          : qsTr("%1 artists · %2 albums · %3 tracks")
                                .arg(app.library.artistCount)
                                .arg(app.library.albumCount)
                                .arg(app.library.trackCount)
                    color: theme.textMuted
                    font.pixelSize: theme.fontSmall
                    elide: Text.ElideRight
                }
            }

            Hint {
                text: qsTr("Scanning is the server's job. SqeezeAmp only asks it to "
                           + "start; tag editing and file management are not part of "
                           + "this application.")
            }

            // prd.md FR-5.5. The server serves the file's plain lyric tag and
            // reads no `.lrc` beside it, so a sheet that can be followed line
            // by line has to be read by this app — which needs to be told where
            // the same music is visible from here. Optional: left empty, the
            // pane still shows whatever the server has.
            GridLayout {
                Layout.fillWidth: true
                Layout.topMargin: theme.spacing
                columns: 2
                columnSpacing: theme.spacing
                rowSpacing: theme.spacing

                Label { text: qsTr("Music folder on this PC"); color: theme.textPrimary }
                TextField {
                    Layout.fillWidth: true
                    text: app.settings.localMusicFolder
                    placeholderText: qsTr("\\\\Server\\music  ·  optional")
                    onEditingFinished: app.settings.localMusicFolder = text
                }
            }

            Hint {
                text: qsTr("Only used to find timed lyrics: a `.lrc` file named after "
                           + "the track, next to it. The server's own path is matched "
                           + "against this one from the end, so pointing at the music "
                           + "root is enough. Nothing else is read from here and "
                           + "nothing is written.")
            }

            // ── Diagnostics (prd.md FR-9.2, and §9.2's list of what Settings
            // holds). The panel had been built in full and left with nothing
            // routing to it — no rail entry, no button, no shortcut — so it had
            // never been opened by anyone. See D17.
            SectionHeader { text: qsTr("Diagnostics") }

            RowLayout {
                Layout.fillWidth: true
                spacing: theme.spacing

                Button {
                    objectName: "openDiagnosticsButton"
                    text: qsTr("Open diagnostics")
                    onClicked: root.openDiagnostics()
                }
                Button {
                    text: qsTr("Open log folder")
                    onClicked: Qt.openUrlExternally("file:///" + app.logDirectory)
                }
            }

            Hint {
                text: qsTr("Every control message, every line the audio engine "
                           + "prints, and what the engine could not determine — "
                           + "shown as unknown rather than as zero. The copy "
                           + "button there is what a bug report should carry.")
            }

            // ── About (prd.md §11.4 — the licences page)
            SectionHeader { text: qsTr("About") }

            Label {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                color: theme.textMuted
                font.pixelSize: theme.fontSmall
                textFormat: Text.PlainText
                text: qsTr("SqeezeAmp %1 — a native player for Lyrion Music Server.\n\n"
                           + "SqeezeAmp is free software under the Mozilla Public "
                           + "License v2, whose text is in the licenses folder next "
                           + "to the application.\n\n"
                           + "Audio is played by squeezelite, a separate program "
                           + "distributed under the GNU General Public License v3. "
                           + "Its licence text and a written offer for its source are "
                           + "in the licenses folder next to the application.\n\n"
                           + "Qt is used under the LGPL v3; the Qt libraries ship as "
                           + "separate DLLs so they can be replaced with your own "
                           + "build.\n\nLogs: %2")
                        .arg(app.version).arg(app.logDirectory)
            }
        }
    }
}
