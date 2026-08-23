// SPDX-License-Identifier: MPL-2.0

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Sqz

// Everything that starting a random mix has to decide, in one non-visual
// place: the menu, the "this will replace your queue" question, and the genre
// scope dialog (prd.md FR-3.9).
//
// It is a component rather than code in each view because two screens offer
// the same action — the now-playing panel and the bottom bar — and the
// question of when to ask before replacing a queue has exactly one right
// answer that neither of them should be spelling out for itself.
//
// Starting a mix *always* replaces the queue: the plugin loads rather than
// appends. So the only question worth asking the user is whether the queue it
// is about to throw away was theirs. A queue that is already a mix is the
// mix's own, and re-rolling it is what the button is for — asking there would
// be a prompt in front of the most ordinary action in the app.
Item {
    id: root

    width: 0
    height: 0

    function start(type) {
        if (app.mix.active || app.queue.count === 0) {
            app.mix.start(type)
            return
        }
        confirm.pendingType = type
        confirm.open()
    }

    function stop() {
        app.mix.stop()
    }

    function openMenu() {
        menu.popup()
    }

    function openGenres() {
        genreDialog.open()
    }

    Theme { id: theme }

    // This menu is now the only place a mix is started. The now-playing screen
    // used to carry the five types as buttons; it carried a second transport
    // and the engine's format badge too, and all of it came off together when
    // that screen went back to being a cover and a seek bar. So everything the
    // panel said has to be sayable here: which mix is running (FR-3.9.4) and
    // what it is scoped to (FR-3.9.8).
    Menu {
        id: menu

        // The scope is a *server* pref shared with every other controller and
        // it survives restarts, so a narrowing done weeks ago is still in force
        // — unguessable from the mix itself, which just quietly plays less
        // music. Reading it costs a request, so it is asked for when a menu
        // that shows it opens rather than kept warm for one that never does.
        onAboutToShow: app.mix.refreshGenres()

        // Written out rather than generated: a Repeater's delegates need an
        // Item to be parented to, and a Menu's contentItem is not one it will
        // hand out. Five lines of duplication beats a menu that is empty at
        // runtime and compiles clean.
        //
        // The running mix is ticked, so re-rolling it and starting a different
        // one look different before the click rather than after it. Not a
        // toggle: clicking the ticked one re-rolls it.
        MenuItem {
            text: qsTr("Song Mix")
            checkable: true
            checked: app.mix.active && app.mix.mixType === Mix.Songs
            onTriggered: root.start(Mix.Songs)
        }
        MenuItem {
            text: qsTr("Album Mix")
            checkable: true
            checked: app.mix.active && app.mix.mixType === Mix.Albums
            onTriggered: root.start(Mix.Albums)
        }
        MenuItem {
            text: qsTr("Artist Mix")
            checkable: true
            checked: app.mix.active && app.mix.mixType === Mix.Artists
            onTriggered: root.start(Mix.Artists)
        }
        MenuItem {
            text: qsTr("Year Mix")
            checkable: true
            checked: app.mix.active && app.mix.mixType === Mix.Years
            onTriggered: root.start(Mix.Years)
        }
        MenuItem {
            text: qsTr("Work Mix")
            checkable: true
            checked: app.mix.active && app.mix.mixType === Mix.Works
            onTriggered: root.start(Mix.Works)
        }

        MenuSeparator {}

        MenuItem {
            text: qsTr("Stop the mix")
            enabled: app.mix.active
            onTriggered: root.stop()
        }
        MenuItem {
            text: qsTr("Choose genres…")
            onTriggered: root.openGenres()
        }

        // Shown only when the scope is narrowed. "Every genre" would be a line
        // that is always there and never worth reading; this is the state that
        // is actually worth interrupting for.
        MenuItem {
            text: app.mix.genreSummary
            visible: app.mix.genres.loaded && app.mix.genres.narrowed
            height: visible ? implicitHeight : 0
            enabled: false
            font.pixelSize: theme.fontSmall
        }
    }

    Dialog {
        id: confirm

        // Which mix the user asked for before the question interrupted them.
        property int pendingType: Mix.Songs

        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        title: qsTr("Replace the queue?")
        standardButtons: Dialog.Ok | Dialog.Cancel

        // A wrapping Label derives its height from its width; a Dialog derives
        // its width from its content. Left to negotiate they loop — first on
        // implicitWidth, and then on implicitHeight once only the width was
        // pinned, because the label was still reading its width back down from
        // the dialog.
        //
        // Cut by making nothing in the message read the dialog's size at all.
        // The text wraps at a constant width, the dialog is that width plus its
        // own padding, and the height reported upward is the painted height of
        // text whose width was never in question.
        readonly property int messageWidth: 380

        width: confirm.messageWidth + confirm.leftPadding + confirm.rightPadding

        onAccepted: app.mix.start(confirm.pendingType)

        contentItem: Item {
            implicitHeight: message.contentHeight

            Label {
                id: message
                width: confirm.messageWidth
                wrapMode: Text.WordWrap
                color: theme.textPrimary
                text: qsTr("Starting the %1 clears the %n track(s) in the queue "
                           + "and fills it with a fresh selection. The server "
                           + "does this for every controller, not just "
                           + "SqeezeAmp.", "", app.queue.count)
                      .arg(app.mix.nameForType(confirm.pendingType))
            }
        }
    }

    MixGenreDialog {
        id: genreDialog
        mix: app.mix
    }
}
