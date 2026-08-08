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

    Menu {
        id: menu

        MenuItem {
            text: qsTr("Song Mix")
            onTriggered: root.start(Mix.Songs)
        }
        MenuItem {
            text: qsTr("Album Mix")
            onTriggered: root.start(Mix.Albums)
        }
        MenuItem {
            text: qsTr("Artist Mix")
            onTriggered: root.start(Mix.Artists)
        }
        MenuItem {
            text: qsTr("Year Mix")
            onTriggered: root.start(Mix.Years)
        }
        MenuItem {
            text: qsTr("Work Mix")
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

        onAccepted: app.mix.start(confirm.pendingType)

        Label {
            width: 360
            wrapMode: Text.WordWrap
            color: theme.textPrimary
            text: qsTr("Starting the %1 clears the %n track(s) in the queue and "
                       + "fills it with a fresh selection. The server does this "
                       + "for every controller, not just SqeezeAmp.", "",
                       app.queue.count)
                  .arg(app.mix.nameForType(confirm.pendingType))
        }
    }

    MixGenreDialog {
        id: genreDialog
        mix: app.mix
    }
}
