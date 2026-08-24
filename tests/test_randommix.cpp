// SPDX-License-Identifier: MPL-2.0

#include "lmscommands.h"
#include "randommix.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QTest>

// The Random Mix's wire format (prd.md FR-3.9).
//
// Every fixture below is a real reply from Lyrion Music Server 9.1.0 with
// RandomPlay as shipped, trimmed but not reshaped. That matters most for the
// two cases that decide whether the indicator can lie:
//
//   * `_randomplayisactive` is **JSON null** when no mix is running. Null is a
//     definite no; a missing field is "the server did not answer". Collapsing
//     those two into one boolean is how a dead indicator ends up over a live
//     mix, and prd.md FR-2.5's rule exists to stop exactly that.
//
//   * The genre reply's `checkbox` field carries the *state*, and the two
//     action rows carry no `checkbox` at all. The presence of the field is
//     therefore the discriminator, not its value.
class TestRandomMix : public QObject
{
    Q_OBJECT

private slots:
    void tokensMatchTheServersVocabulary();
    void theReportedSpellingIsNotTheSentSpelling();
    void anUnknownMixTypeIsNotGuessedAt();

    void aNullAnswerMeansNoMixIsRunning();
    void aMissingAnswerMeansNobodyKnows();
    void aTypeTokenMeansThatMixIsRunning();
    void anOlderPluginsBooleanIsStillAnAnswer();

    void theGenreScopeReadsNamesAndFlags();
    void theActionRowsAreNotGenres();
    void aCheckboxDeliveredAsAStringStillCounts();

    void commandsAreTheFlatVerbsAndNotAMenuWalk();

private:
    static QJsonObject object(const char *json)
    {
        return QJsonDocument::fromJson(QByteArray(json)).object();
    }
};

void TestRandomMix::tokensMatchTheServersVocabulary()
{
    // The labels and the wire tokens disagree on three of the five, which is
    // the whole reason this mapping is not just a lowercase of the name.
    QCOMPARE(RandomMix::token(RandomMix::Type::Songs),   QStringLiteral("tracks"));
    QCOMPARE(RandomMix::token(RandomMix::Type::Artists), QStringLiteral("contributors"));
    QCOMPARE(RandomMix::token(RandomMix::Type::Years),   QStringLiteral("year"));
    QCOMPARE(RandomMix::token(RandomMix::Type::Albums),  QStringLiteral("albums"));
    QCOMPARE(RandomMix::token(RandomMix::Type::Works),   QStringLiteral("works"));

    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("tracks")),
             int(RandomMix::Type::Songs));
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("contributors")),
             int(RandomMix::Type::Artists));
}

void TestRandomMix::theReportedSpellingIsNotTheSentSpelling()
{
    // The regression this file exists for. The CLI takes the plural forms the
    // plugin's own menu uses and maps them onto the singular names it keeps
    // internally, and `randomplayisactive` reports that singular. A running
    // Song Mix started with `randomplay tracks` answers "track".
    //
    // Recognising only the sent spelling shipped once: the indicator read
    // "Random Mix" over a Song Mix and its button did not light.
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("track")),
             int(RandomMix::Type::Songs));
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("album")),
             int(RandomMix::Type::Albums));
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("contributor")),
             int(RandomMix::Type::Artists));
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("work")),
             int(RandomMix::Type::Works));

    // `year` is spelled the same both ways — the coincidence that makes a
    // single-column table look correct until a mix is actually running.
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("year")),
             int(RandomMix::Type::Years));

    // `artists` is the plugin's own alias for `contributors`.
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("artists")),
             int(RandomMix::Type::Artists));

    // The plugin lowercases what it is handed before storing it.
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("Track")),
             int(RandomMix::Type::Songs));
}

void TestRandomMix::anUnknownMixTypeIsNotGuessedAt()
{
    // A plugin update may add a sixth mix. Mapping it onto one of the five
    // would put a confident wrong name in the UI; -1 lets the caller say
    // "a mix, and not one I know".
    QCOMPARE(RandomMix::indexOfToken(QStringLiteral("decades")), -1);
    QCOMPARE(RandomMix::indexOfToken(QString()), -1);
}

void TestRandomMix::aNullAnswerMeansNoMixIsRunning()
{
    const RandomMix::State state = RandomMix::State::fromActiveResult(
        object(R"({"_randomplayisactive": null})"));

    QCOMPARE(state.status, RandomMix::State::Status::Inactive);
    QVERIFY(!state.isActive());
    QVERIFY(state.typeToken.isEmpty());
}

void TestRandomMix::aMissingAnswerMeansNobodyKnows()
{
    // A reply that carries no answer is not a "no". This is the case a failed
    // or truncated request produces, and treating it as Inactive is what would
    // switch the indicator off while a mix kept playing.
    const RandomMix::State state = RandomMix::State::fromActiveResult(object("{}"));

    QCOMPARE(state.status, RandomMix::State::Status::Unknown);
    QVERIFY(!state.isActive());
}

void TestRandomMix::aTypeTokenMeansThatMixIsRunning()
{
    // Verbatim from the running server while a Song Mix was playing — note the
    // singular, which is what the plugin stores and reports.
    const RandomMix::State state = RandomMix::State::fromActiveResult(
        object(R"({"_randomplayisactive": "track"})"));

    QCOMPARE(state.status, RandomMix::State::Status::Active);
    QVERIFY(state.isActive());
    QCOMPARE(state.typeToken, QStringLiteral("track"));
    QCOMPARE(RandomMix::indexOfToken(state.typeToken), int(RandomMix::Type::Songs));
}

void TestRandomMix::anOlderPluginsBooleanIsStillAnAnswer()
{
    const RandomMix::State off = RandomMix::State::fromActiveResult(
        object(R"({"_randomplayisactive": 0})"));
    QCOMPARE(off.status, RandomMix::State::Status::Inactive);

    const RandomMix::State on = RandomMix::State::fromActiveResult(
        object(R"({"_randomplayisactive": 1})"));
    QCOMPARE(on.status, RandomMix::State::Status::Active);

    // Active, but it did not say which mix — so nothing may claim it did.
    QVERIFY(on.typeToken.isEmpty());
    QCOMPARE(RandomMix::indexOfToken(on.typeToken), -1);
}

void TestRandomMix::theGenreScopeReadsNamesAndFlags()
{
    const QList<RandomMix::Genre> genres = RandomMix::genresFromListResult(object(R"({
        "count": 3,
        "item_loop": [
            {"checkbox": 1, "text": "Acoustic"},
            {"checkbox": 0, "text": "No Genre"},
            {"checkbox": 1, "text": "Adult Album Alternative"}
        ]
    })"));

    QCOMPARE(genres.size(), 3);
    QCOMPARE(genres.at(0).name, QStringLiteral("Acoustic"));
    QVERIFY(genres.at(0).included);
    QCOMPARE(genres.at(1).name, QStringLiteral("No Genre"));
    QVERIFY(!genres.at(1).included);
    QVERIFY(genres.at(2).included);
}

void TestRandomMix::theActionRowsAreNotGenres()
{
    // "Select All" and "Select None" arrive in the same loop and carry no
    // `checkbox`. KvitSqueeze offers those as its own buttons; letting them
    // through would put two fake genres at the top of the list.
    const QList<RandomMix::Genre> genres = RandomMix::genresFromListResult(object(R"({
        "count": 4,
        "item_loop": [
            {"text": "Select All",  "actions": {"go": {"cmd": ["randomplaygenreselectall", 1]}}},
            {"text": "Select None", "actions": {"go": {"cmd": ["randomplaygenreselectall", 0]}}},
            {"checkbox": 1, "text": "Rock"},
            {"checkbox": 1, "text": "Jazz"}
        ]
    })"));

    QCOMPARE(genres.size(), 2);
    QCOMPARE(genres.at(0).name, QStringLiteral("Rock"));
    QCOMPARE(genres.at(1).name, QStringLiteral("Jazz"));
}

void TestRandomMix::aCheckboxDeliveredAsAStringStillCounts()
{
    // LMS is not consistent about whether a small integer arrives quoted; the
    // same session has been seen sending `"offset":"0"` beside `"count":144`.
    const QList<RandomMix::Genre> genres = RandomMix::genresFromListResult(object(R"({
        "item_loop": [
            {"checkbox": "1", "text": "Rock"},
            {"checkbox": "0", "text": "Spoken Word"}
        ]
    })"));

    QCOMPARE(genres.size(), 2);
    QVERIFY(genres.at(0).included);
    QVERIFY(!genres.at(1).included);
}

void TestRandomMix::commandsAreTheFlatVerbsAndNotAMenuWalk()
{
    // This is the shape that keeps prd.md N4 intact: fixed verbs with fixed
    // arguments, no descriptor to render and nothing to recurse into. A
    // command here that grew a menu parameter would be the first step of the
    // change N4 says not to make.
    QCOMPARE(LmsCommand::randomMixStart(RandomMix::Type::Songs),
             (QStringList{ QStringLiteral("randomplay"), QStringLiteral("tracks") }));
    QCOMPARE(LmsCommand::randomMixStart(RandomMix::Type::Artists),
             (QStringList{ QStringLiteral("randomplay"), QStringLiteral("contributors") }));

    QCOMPARE(LmsCommand::randomMixStop(),
             (QStringList{ QStringLiteral("randomplay"), QStringLiteral("disable") }));

    QCOMPARE(LmsCommand::randomMixActive(),
             (QStringList{ QStringLiteral("randomplayisactive") }));

    QCOMPARE(LmsCommand::randomMixGenres(0, 1000),
             (QStringList{ QStringLiteral("randomplaygenrelist"),
                           QStringLiteral("0"), QStringLiteral("1000") }));

    // The genre travels as its name, not an id — the plugin keys its pref on
    // the string the server displays, spaces and all.
    QCOMPARE(LmsCommand::randomMixChooseGenre(QStringLiteral("Adult Album Alternative"), false),
             (QStringList{ QStringLiteral("randomplaychoosegenre"),
                           QStringLiteral("Adult Album Alternative"),
                           QStringLiteral("0") }));

    QCOMPARE(LmsCommand::randomMixAllGenres(true),
             (QStringList{ QStringLiteral("randomplaygenreselectall"),
                           QStringLiteral("1") }));
}

QTEST_MAIN(TestRandomMix)
#include "test_randommix.moc"
