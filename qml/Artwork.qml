pragma ComponentBehavior: Bound
import QtQuick

// One album cover, with the placeholder that stands in until it arrives.
//
// `asynchronous` plus the C++ image provider is what keeps a grid fling off
// the GUI thread (prd.md FR-3.3, NFR-2): this element never decodes anything,
// it asks `image://artwork/...` and draws whatever comes back.
//
// The requested size is part of the URL rather than a scaling hint, because it
// decides which rendition the server produces and therefore which cache entry
// is being asked for.
//
// Covers are drawn square-cornered on purpose. Rounding an Image needs either
// a mask effect or a shader source, and neither is worth a 4 px radius on an
// element that appears two hundred times in a grid.
Item {
    id: root

    property string coverId
    property int requestSize: 300

    Theme { id: theme }

    Rectangle {
        anchors.fill: parent
        color: theme.placeholder
        visible: cover.status !== Image.Ready

        Text {
            anchors.centerIn: parent
            text: theme.iconMusic
            font.family: theme.iconFont
            font.pixelSize: Math.max(12, Math.min(parent.width, parent.height) * 0.34)
            color: theme.textFaint
        }
    }

    Image {
        id: cover
        anchors.fill: parent
        asynchronous: true
        // ArtworkCache is the cache, in memory and on disk. Qt's own pixmap
        // cache on top of it would be a third copy of every cover.
        cache: false
        fillMode: Image.PreserveAspectCrop
        smooth: true
        source: root.coverId ? app.artworkSource(root.coverId, root.requestSize) : ""

        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: theme.animation }
        }
    }
}
