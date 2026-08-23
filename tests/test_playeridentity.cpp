// SPDX-License-Identifier: MPL-2.0

#include "playeridentity.h"

#include <QSet>
#include <QTest>

// prd.md FR-1.4: a stable, persisted, MAC-style id, because the same id across
// restarts is what keeps the queue and the per-player settings server-side.
//
// The generation rules are the interesting part. An id that collides with a
// real network card — or with a real Squeezebox, whose addresses all come out
// of Logitech's 00:04:20 range — would register this player as that one.
class TestPlayerIdentity : public QObject
{
    Q_OBJECT

private slots:
    void generatesALocallyAdministeredUnicastAddress();
    void generatedIdsAreDistinct();
    void validatesTheFormatSqueezeliteAccepts();
    void theProcessIdIsStable();
};

void TestPlayerIdentity::generatesALocallyAdministeredUnicastAddress()
{
    for (int attempt = 0; attempt < 200; ++attempt) {
        const QString mac = PlayerIdentity::generate();
        QVERIFY2(PlayerIdentity::isValid(mac), qPrintable(mac));

        bool ok = false;
        const uint first = mac.left(2).toUInt(&ok, 16);
        QVERIFY(ok);

        // Bit 1 set: locally administered, so it cannot collide with an
        // assigned address. Bit 0 clear: unicast, because a multicast address
        // is not a thing a player can be.
        QVERIFY2((first & 0x02) != 0, qPrintable(mac));
        QVERIFY2((first & 0x01) == 0, qPrintable(mac));
    }
}

void TestPlayerIdentity::generatedIdsAreDistinct()
{
    // Two machines that generated the same id would fight over one player
    // registration on the server.
    QSet<QString> seen;
    for (int attempt = 0; attempt < 500; ++attempt)
        seen.insert(PlayerIdentity::generate());
    QCOMPARE(seen.size(), 500);
}

void TestPlayerIdentity::validatesTheFormatSqueezeliteAccepts()
{
    QVERIFY(PlayerIdentity::isValid(QStringLiteral("aa:bb:cc:dd:ee:ff")));
    QVERIFY(PlayerIdentity::isValid(QStringLiteral("00:04:20:2A:CC:0F")));

    QVERIFY(!PlayerIdentity::isValid(QString()));
    QVERIFY(!PlayerIdentity::isValid(QStringLiteral("aa-bb-cc-dd-ee-ff")));
    QVERIFY(!PlayerIdentity::isValid(QStringLiteral("aa:bb:cc:dd:ee")));
    QVERIFY(!PlayerIdentity::isValid(QStringLiteral("aa:bb:cc:dd:ee:ff:00")));
    QVERIFY(!PlayerIdentity::isValid(QStringLiteral("gg:bb:cc:dd:ee:ff")));
}

void TestPlayerIdentity::theProcessIdIsStable()
{
    // One player per process, permanently (prd.md N6/D3). Regenerating mid-run
    // would register a second player and orphan the first one's queue.
    PlayerIdentity::overrideForTesting(QStringLiteral("aa:bb:cc:dd:ee:ff"));
    QCOMPARE(PlayerIdentity::mac(), QStringLiteral("aa:bb:cc:dd:ee:ff"));
    QCOMPARE(PlayerIdentity::mac(), PlayerIdentity::mac());
}

QTEST_APPLESS_MAIN(TestPlayerIdentity)
#include "test_playeridentity.moc"
