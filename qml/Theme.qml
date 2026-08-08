pragma ComponentBehavior: Bound
import QtQuick

// The single theme (prd.md §9.3, N8).
//
// One built-in look with a dark/light switch and a density option. There is no
// skin API, no theme pack and no runtime loading — prd.md N8 rules all of it
// out permanently, which is exactly why every colour in the app may be a
// hard-coded constant in one file instead of a lookup through an indirection
// that exists for a feature the product does not want.
//
// Deliberately *not* a QML singleton. A singleton needs a qmldir entry, and
// resources.qrc is this project's only component list (CLAUDE.md); a file that
// has to appear in two places is a file that will eventually appear in one.
// Instantiating this is a handful of bindings, and every instance follows the
// same settings object, so they cannot drift.
QtObject {
    id: theme

    // 0 = follow the Windows setting, 1 = dark, 2 = light (Settings::Theme).
    readonly property bool dark: app.settings.theme === 0
        ? Application.styleHints.colorScheme !== Qt.ColorScheme.Light
        : app.settings.theme === 1

    readonly property bool compact: app.settings.compactDensity

    // ── Surfaces
    readonly property color surface:        dark ? "#14161a" : "#f5f6f8"
    readonly property color surfaceRaised:  dark ? "#1c1f25" : "#ffffff"
    readonly property color surfaceOverlay: dark ? "#252932" : "#e9ecf1"
    readonly property color surfaceHover:   dark ? "#2a2f38" : "#dfe3ea"
    readonly property color border:         dark ? "#2c313a" : "#d5dae1"
    readonly property color placeholder:    dark ? "#2a2e35" : "#dde1e7"

    // ── Text
    readonly property color textPrimary: dark ? "#e8eaed" : "#16181c"
    readonly property color textMuted:   dark ? "#9aa0a6" : "#5f6368"
    readonly property color textFaint:   dark ? "#6b7178" : "#8a9099"

    // ── One accent colour, plus the two states the shell has to shout about.
    readonly property color accent: dark ? "#4a9eff" : "#1a73e8"

    // Not "onAccent": a property whose name begins with `on` followed by a
    // capital is parsed as a signal handler, and QML rejects the file with
    // "Cannot assign a value to a signal" rather than telling you why.
    readonly property color accentText: "#ffffff"

    readonly property color warning: dark ? "#e0a53a" : "#a05a00"
    readonly property color danger:  dark ? "#e0605c" : "#c5221f"

    // ── Metrics. prd.md §9.3: rows ≥ 32 px comfortable, ≥ 24 px compact.
    //
    // These are the height of the whole row, and every list delegate that uses
    // one must set `topPadding: 0; bottomPadding: 0`. A Basic ItemDelegate
    // brings **12 px of padding above and below** its content, which is not
    // visible in the QML and is not subtracted from anything: a 40 px row
    // leaves 16 px for content, a 34 px row leaves 10, and a compact 26 px row
    // leaves 2. Nothing clips it, so the surplus is drawn over the neighbouring
    // rows — the queue's two-line rows overlapped by about 17 px.
    //
    // Measured rather than reasoned: one line of fontNormal is 18 px, and the
    // queue's title-over-artist column is 33 px comfortable and 31 compact.
    // That last number is why compact rows are 34 and not 30 — two lines of
    // text do not fit in 30 px at any padding, and the queue is the only view
    // that stacks two.
    readonly property int rowHeight:   compact ? 26 : 34
    readonly property int trackHeight: compact ? 34 : 40
    readonly property int spacing:     compact ? 6 : 10
    readonly property int margin:      compact ? 8 : 14
    readonly property int radius:      4
    readonly property int gridCell:    compact ? 150 : 184
    readonly property int railWidth:   compact ? 168 : 190

    readonly property int fontSmall:  compact ? 11 : 12
    readonly property int fontNormal: compact ? 12 : 13
    readonly property int fontLarge:  compact ? 15 : 17
    readonly property int fontHuge:   compact ? 22 : 26

    // prd.md §9.3: ≤ 150 ms, and nothing that delays interaction.
    readonly property int animation: 130

    // Windows ships this font with the shell, so the transport glyphs are the
    // ones the OS draws in its own media UI and the tree needs no icon assets.
    // prd.md N1 makes Windows the only target, so there is no second platform
    // for this to be wrong on.
    readonly property string iconFont: "Segoe MDL2 Assets"

    // Spelled as code points rather than pasted glyphs: these characters live
    // in a private use area, where a copy-paste through anything that does not
    // preserve the encoding turns them into replacement boxes that still
    // compile and still run.
    readonly property string iconPlay:      String.fromCharCode(0xE768)
    readonly property string iconPause:     String.fromCharCode(0xE769)
    readonly property string iconStop:      String.fromCharCode(0xE71A)
    readonly property string iconPrevious:  String.fromCharCode(0xE892)
    readonly property string iconNext:      String.fromCharCode(0xE893)
    readonly property string iconShuffle:   String.fromCharCode(0xE8B1)
    readonly property string iconRepeatAll: String.fromCharCode(0xE8EE)
    readonly property string iconRepeatOne: String.fromCharCode(0xE8ED)
    readonly property string iconVolume:    String.fromCharCode(0xE767)
    readonly property string iconMute:      String.fromCharCode(0xE74F)
    readonly property string iconQueue:     String.fromCharCode(0xE8FD)
    readonly property string iconSearch:    String.fromCharCode(0xE721)
    readonly property string iconSettings:  String.fromCharCode(0xE713)
    readonly property string iconBack:      String.fromCharCode(0xE72B)
    readonly property string iconPower:     String.fromCharCode(0xE7E8)
    readonly property string iconAdd:       String.fromCharCode(0xE710)
    readonly property string iconRemove:    String.fromCharCode(0xE74D)
    readonly property string iconFolder:    String.fromCharCode(0xE8B7)
    readonly property string iconMusic:     String.fromCharCode(0xE8D6)
    readonly property string iconGrid:      String.fromCharCode(0xE8A9)
    readonly property string iconList:      String.fromCharCode(0xE8FD)
    readonly property string iconPin:       String.fromCharCode(0xE840)
    readonly property string iconSync:      String.fromCharCode(0xE895)
    readonly property string iconFilter:    String.fromCharCode(0xE71C)

    // The random mix borrows the shuffle glyph. It is the same idea — the order
    // is not yours — and Segoe MDL2 has nothing closer. The two never appear as
    // bare icons beside each other: the mix control always carries its name.
    readonly property string iconMix:       String.fromCharCode(0xE8B1)

    // ── Formatting, kept with the theme because it is presentation.
    //
    // The unknown rule from prd.md FR-2.5 applies to metadata too: a negative
    // duration is "the server did not say", and an em dash says that honestly
    // where "0:00" would claim the track is empty.
    function duration(seconds) {
        if (seconds === undefined || seconds === null || seconds < 0)
            return "—"
        var total = Math.floor(seconds)
        var hours = Math.floor(total / 3600)
        var minutes = Math.floor((total % 3600) / 60)
        var secs = total % 60
        var pad = function (n) { return n < 10 ? "0" + n : "" + n }
        return hours > 0 ? hours + ":" + pad(minutes) + ":" + pad(secs)
                         : minutes + ":" + pad(secs)
    }

    function longDuration(seconds) {
        if (seconds === undefined || seconds === null || seconds <= 0)
            return ""
        var minutes = Math.round(seconds / 60)
        if (minutes < 60)
            return qsTr("%n minute(s)", "", minutes)
        var hours = Math.floor(minutes / 60)
        return qsTr("%1 h %2 min").arg(hours).arg(minutes % 60)
    }
}
