// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

// Getting an audio engine (prd.md FR-2.11).
//
// KvitSqueeze does not ship squeezelite — it is GPLv3 and neither the
// installer nor the portable zip carries it (THIRD-PARTY-NOTICES.md). That is
// a licensing decision the user has no reason to know about, and before this
// panel existed the only thing that explained it was the README. The first
// person to use the app installed the portable zip, browsed a whole library,
// pressed play, and got silence. This is the fix: the app asks, and the app
// fetches.
//
// Three routes out, in the order they are offered, because the first one is
// not always available. That is not hypothetical either — the same user's
// workplace firewall blocks the kind of traffic this download is:
//
//   1. Download it. One button.
//   2. Point at a copy already on this PC. Survives a firewall.
//   3. Open the folder and be told exactly what goes in it.
//
// Nothing here asks anyone to restart the app afterwards. ExternalEngine
// notices the engine arriving however it arrived, and EngineController
// relaunches — see prd.md FR-2.11.
ColumnLayout {
    id: root

    // The dialog draws its own title; the settings section does not.
    property bool showHeading: true

    // Shown once the engine is present, so the same component can say "done"
    // instead of being hidden out from under the user mid-download.
    readonly property bool ready: app.engine.available

    spacing: theme.spacing

    Theme { id: theme }

    Label {
        visible: root.showHeading
        Layout.fillWidth: true
        text: root.ready ? qsTr("The audio engine is installed")
                         : qsTr("One thing left: the audio engine")
        color: theme.textPrimary
        font.pixelSize: theme.fontLarge
        font.bold: true
        wrapMode: Text.WordWrap
    }

    Label {
        Layout.fillWidth: true
        wrapMode: Text.WordWrap
        color: theme.textMuted
        font.pixelSize: theme.fontSmall
        text: root.ready
              ? qsTr("KvitSqueeze plays audio through squeezelite, which is now at "
                     + "%1. Nothing else is needed.").arg(app.engine.enginePath)
              : qsTr("KvitSqueeze plays audio through squeezelite — a separate "
                     + "program under a licence that does not let this app "
                     + "distribute it. So it is not in the download. Fetching it "
                     + "is one button and about 3 MB, once; until then everything "
                     + "here works except sound.")
    }

    // ── 1. Download it.
    RowLayout {
        Layout.fillWidth: true
        spacing: theme.spacing

        Button {
            objectName: "downloadEngineButton"
            visible: !root.ready
            enabled: !app.engine.installing
            text: app.engine.installing ? qsTr("Downloading…")
                                        : qsTr("Download the audio engine")
            onClicked: app.engine.installEngine()
        }
        Button {
            visible: app.engine.installing
            text: qsTr("Cancel")
            onClicked: app.engine.cancelInstall()
        }
        Item { Layout.fillWidth: true }
    }

    ProgressBar {
        Layout.fillWidth: true
        visible: app.engine.installing
        // prd.md FR-2.5's rule reaches even here: a server that announced no
        // length leaves this indeterminate rather than parked at zero, which
        // would read as a download that is not moving.
        indeterminate: app.engine.installProgress < 0
        from: 0
        to: 100
        value: Math.max(0, app.engine.installProgress)
    }

    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: app.engine.installStatus
        color: root.ready ? theme.textMuted : theme.textPrimary
        font.pixelSize: theme.fontSmall
        wrapMode: Text.WordWrap
    }

    Label {
        Layout.fillWidth: true
        visible: text.length > 0
        text: app.engine.installError
        color: theme.danger
        font.pixelSize: theme.fontSmall
        wrapMode: Text.WordWrap
    }

    // The address, once a manifest has been read, so a blocked download is
    // still a lead rather than a dead end. Selectable on purpose: the next
    // thing that user does is paste it somewhere that can reach it.
    TextEdit {
        Layout.fillWidth: true
        visible: app.engine.installError.length > 0
                 && app.engine.installSourceUrl.length > 0
        text: app.engine.installSourceUrl
        readOnly: true
        selectByMouse: true
        wrapMode: TextEdit.WrapAnywhere
        color: theme.textMuted
        font.pixelSize: theme.fontSmall
    }

    // ── 2. A copy already on this PC, and 3. the folder itself.
    RowLayout {
        Layout.fillWidth: true
        spacing: theme.spacing

        Button {
            objectName: "chooseEngineButton"
            visible: !root.ready
            enabled: !app.engine.installing
            text: qsTr("Choose squeezelite.exe…")
            onClicked: enginePicker.open()
        }
        Button {
            text: qsTr("Open the engine folder")
            onClicked: Qt.openUrlExternally("file:///" + app.engine.engineFolder)
        }
        Item { Layout.fillWidth: true }
    }

    Label {
        Layout.fillWidth: true
        visible: !root.ready
        wrapMode: Text.WordWrap
        color: theme.textFaint
        font.pixelSize: theme.fontSmall
        text: qsTr("Doing it by hand works too: put squeezelite.exe at %1, or run "
                   + "fetch-engine.ps1 from the installation folder. Either way "
                   + "KvitSqueeze picks it up within a couple of seconds — there "
                   + "is no need to restart it.").arg(app.engine.enginePath)
    }

    FileDialog {
        id: enginePicker
        title: qsTr("Choose squeezelite.exe")
        nameFilters: [qsTr("Programs (*.exe)"), qsTr("All files (*)")]
        onAccepted: app.engine.useExistingEngine(selectedFile)
    }
}
