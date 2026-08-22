#include "lrcsheet.h"

#include <QRegularExpression>

#include <algorithm>

namespace {

// One timestamp: [mm:ss], [mm:ss.xx] or [mm:ss.xxx]. Minutes are not bounded
// to two digits — a 100-minute mix is rare but nothing about it is malformed.
const QRegularExpression &stampPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(\[(\d{1,3}):([0-5]?\d)(?:[.:](\d{1,3}))?\])"));
    return pattern;
}

// [offset:+250] shifts every line by that many milliseconds. Positive means
// the lyrics are *late* in the file and should be pulled earlier, which is the
// opposite of what the sign reads like, and is the convention every player
// implements.
const QRegularExpression &offsetPattern()
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^\s*\[offset:\s*([+-]?\d+)\s*\]\s*$)"),
        QRegularExpression::CaseInsensitiveOption);
    return pattern;
}

double fractionOf(const QString &digits)
{
    if (digits.isEmpty())
        return 0.0;

    // "5" is five tenths, "05" is five hundredths, "005" five thousandths —
    // reading it as an integer would make a two-digit and a three-digit file
    // disagree by a factor of ten.
    bool ok = false;
    const double value = digits.toDouble(&ok);
    if (!ok)
        return 0.0;
    return value / std::pow(10.0, digits.size());
}

} // namespace

QStringList LrcSheet::texts() const
{
    QStringList out;
    out.reserve(int(lines.size()));
    for (const Line &line : lines)
        out.append(line.text);
    return out;
}

int LrcSheet::lineAt(double seconds) const
{
    if (lines.isEmpty())
        return -1;

    // The last line whose time has passed. std::upper_bound gives the first
    // one that has not, so the answer is the element before it — and -1 when
    // the track is still ahead of the first line, which is the intro and is
    // not a line.
    const auto it = std::upper_bound(lines.cbegin(), lines.cend(), seconds,
                                     [](double value, const Line &line) {
                                         return value < line.seconds;
                                     });
    return int(it - lines.cbegin()) - 1;
}

bool LrcSheet::looksTimed(const QString &text)
{
    return stampPattern().match(text).hasMatch();
}

LrcSheet LrcSheet::parse(const QString &text)
{
    LrcSheet sheet;
    double offsetSeconds = 0.0;

    const QStringList rows = text.split(QRegularExpression(QStringLiteral("\r\n|\n|\r")));

    for (const QString &row : rows) {
        const QRegularExpressionMatch offset = offsetPattern().match(row);
        if (offset.hasMatch()) {
            offsetSeconds = offset.captured(1).toDouble() / 1000.0;
            continue;
        }

        // Every timestamp on this row: a refrain is written once and timed as
        // often as it is sung, and dropping the repeats would leave the
        // highlight stuck on the verse before it.
        QList<double> stamps;
        int textStart = 0;
        auto matches = stampPattern().globalMatch(row);
        while (matches.hasNext()) {
            const QRegularExpressionMatch match = matches.next();

            // Only a run of timestamps at the head of the row counts. A "[2:00]"
            // inside the line itself is part of the lyric, not a cue.
            if (match.capturedStart() != textStart)
                break;
            textStart = match.capturedEnd();

            stamps.append(match.captured(1).toDouble() * 60.0
                          + match.captured(2).toDouble()
                          + fractionOf(match.captured(3)));
        }

        if (stamps.isEmpty())
            continue;   // metadata, a blank row, or prose: not a timed line

        const QString body = row.mid(textStart).trimmed();
        for (const double stamp : stamps)
            sheet.lines.append({ std::max(0.0, stamp - offsetSeconds), body });
    }

    // A file may time its refrains out of order, and a view that draws lines in
    // file order would jump backwards. Stable, so two lines sharing a
    // timestamp keep the order they were written in.
    std::stable_sort(sheet.lines.begin(), sheet.lines.end(),
                     [](const Line &a, const Line &b) { return a.seconds < b.seconds; });

    return sheet;
}
