// SPDX-License-Identifier: MPL-2.0

#pragma once

// The Random Mix — the server's own RandomPlay plugin, driven through its
// typed commands.
//
// prd.md N4 cuts plugins, and that cut stands for everything menu-shaped.
// This is the one named exception (prd.md N4a, FR-3.9), and it is narrow:
// RandomPlay ships with the server, draws only from the scanned local
// library, sits under the server's own `myMusic` node, and is reached through
// six flat verbs that take fixed arguments. Nothing here renders a
// server-supplied menu descriptor and nothing here nests, which is what keeps
// the exception from becoming the generic renderer N4 deleted.
//
// Verified against Lyrion Music Server 9.1.0 with RandomPlay as shipped.

#include <QJsonObject>
#include <QList>
#include <QString>

namespace RandomMix {

// The five mixes the plugin offers. The wire tokens are not the labels —
// `contributors` is "Artist Mix" and `tracks` is "Song Mix" — so the mapping
// lives here instead of being spelled out at each call site.
enum class Type { Songs, Albums, Artists, Years, Works };

// **The token you send is not the token you get back.** The plugin's CLI takes
// the plural forms its own menu uses and maps them onto the singular names it
// keeps internally (`tracks` → `track`, `contributors` → `contributor`), and
// `randomplayisactive` reports that internal singular. Sending `randomplay
// tracks` and then being told the mix is a `track` is correct behaviour, not a
// server quirk to route around.
//
// This cost a wrong label in the shipped UI once: a running Song Mix read as
// the generic "Random Mix" and its button did not light, because only the
// plural was recognised.
QString token(Type type);

// The index of a reported token in Type order, or -1 when the server named a
// mix this build does not know. Accepts both spellings, because the same value
// arrives one way from the menu and the other from a status query. `artists`
// is here too: the plugin's own map accepts it as an alias for `contributors`.
//
// A plugin update that adds a sixth mix must not be silently reported as one
// of the five that are recognised, which is why an unknown token stays -1
// rather than falling back on the first entry.
int indexOfToken(const QString &wireToken);

// Whether a mix is running — as three states, not two.
//
// prd.md FR-2.5's rule applies here as much as it does to the engine: a reply
// that never arrived, or one that could not be parsed, means *unknown*.
// Reporting "no mix is running" because a request failed would put a dead
// indicator over a live mix, and that is the one failure a listener has no way
// to diagnose from the outside.
struct State
{
    enum class Status { Unknown, Inactive, Active };

    Status status = Status::Unknown;

    // The raw server token, non-empty only when Active. Kept raw so a mix type
    // this build does not recognise is still nameable in the UI.
    QString typeToken;

    bool isActive() const { return status == Status::Active; }

    // Parse the `result` object of a `randomplayisactive` reply. The field is
    // JSON null when no mix is running — a definite answer, not a missing one.
    // Anything else that cannot be read stays Unknown.
    static State fromActiveResult(const QJsonObject &result);
};

// One genre, and whether the mix is allowed to draw from it.
struct Genre
{
    QString name;
    bool included = true;
};

// The genre scope, read out of a `randomplaygenrelist` reply.
//
// That reply is menu-shaped, and this function deliberately reads two fields
// out of it — `text` and `checkbox` — and nothing else. It never looks at
// `actions`, never follows a `cmd`, and never recurses. The commands that
// change the scope are the typed ones in LmsCommand, built from a genre name.
// So this is an extraction of a name/flag pair, not the generic renderer
// prd.md N4 rules out.
//
// Rows carrying no `checkbox` field are the plugin's own "Select All" /
// "Select None" actions; they are dropped, because SqeezeAmp offers those as
// its own buttons rather than as rows in a list of genres.
QList<Genre> genresFromListResult(const QJsonObject &result);

} // namespace RandomMix
