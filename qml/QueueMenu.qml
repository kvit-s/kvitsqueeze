pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Sqz

// prd.md FR-4.2: play now / play next / add to end, from every browse context.
//
// One menu, opened over whatever row was right-clicked, carrying that row's
// selector. Because `playlistcontrol` takes any library selector, the same
// three items work for an album, an artist, a genre, a year, a playlist, a
// folder or a single track — which is why no browse screen needs queueing code
// of its own.
Menu {
    id: root

    // e.g. "album_id" and "5254", or the accumulated filter set of a screen.
    property string selectorKey
    property string selectorValue
    property var selectorFilters: []
    property string subject

    function enqueue(action) {
        if (root.selectorFilters.length > 0)
            app.library.enqueueFiltered(root.selectorFilters, action)
        else
            app.library.enqueue(root.selectorKey, root.selectorValue, action)
    }

    MenuItem {
        text: qsTr("Play now")
        onTriggered: root.enqueue(Library.PlayNow)
    }
    MenuItem {
        text: qsTr("Play next")
        onTriggered: root.enqueue(Library.PlayNext)
    }
    MenuItem {
        text: qsTr("Add to end of queue")
        onTriggered: root.enqueue(Library.AddToEnd)
    }
}
