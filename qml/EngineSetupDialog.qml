// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The first-run way in to EngineSetup (prd.md FR-2.11).
//
// Shown once per run when there is no audio engine, because the alternative —
// a warning in a settings screen — is exactly what the first user of this app
// did not find. It is dismissable rather than blocking: browsing a library
// with no engine is a perfectly reasonable thing to be doing, and an app that
// will not let go of the screen until you have downloaded something is worse
// than one that will not play.
//
// The banner in Main.qml is what stays behind after it is dismissed.
Dialog {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(parent ? parent.width - 80 : 520, 560)

    modal: true
    closePolicy: Popup.CloseOnEscape
    title: qsTr("Set up playback")

    Theme { id: theme }

    // Closes itself the moment an engine appears — by this download, by
    // fetch-engine.ps1 in another window, or by a file dropped into the
    // folder. Delayed a moment so the "installed" line is actually seen.
    Connections {
        target: app.engine
        function onStatusChanged() {
            if (app.engine.available && root.visible)
                closeWhenReady.restart()
        }
    }

    Timer {
        id: closeWhenReady
        interval: 1800
        onTriggered: root.close()
    }

    // A single button, because "Not now" and "Close" are the same action and
    // the panel below already carries every verb that does something.
    footer: DialogButtonBox {
        Button {
            text: app.engine.available ? qsTr("Close") : qsTr("Not now")
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    contentItem: EngineSetup {
        showHeading: false
    }
}
