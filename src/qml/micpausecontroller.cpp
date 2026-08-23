// SPDX-License-Identifier: MPL-2.0

#include "micpausecontroller.h"

#include "micwatcher.h"
#include "playbackcontroller.h"
#include "settings.h"

#include <QTimer>

#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
// The decision. No Windows, no clock, no player — every case in
// tests/test_micpause.cpp drives this directly.

MicPausePolicy::Action MicPausePolicy::setEnabled(bool enabled)
{
    if (enabled == m_enabled)
        return Action::Nothing;

    m_enabled = enabled;
    if (enabled)
        return Action::Nothing;

    // Switched off while holding a pause: let go of the claim, but do not
    // start the music. The user turned a feature off; they did not ask for
    // playback. Anything else would make a settings checkbox a transport
    // control.
    m_micInUse = false;
    m_holdsPause = false;
    m_heldTrackId.clear();
    return Action::CancelResume;
}

MicPausePolicy::Action MicPausePolicy::micInUseChanged(bool inUse, const Player &player)
{
    if (!m_enabled)
        return Action::Nothing;
    if (inUse == m_micInUse)
        return Action::Nothing;

    m_micInUse = inUse;

    if (inUse) {
        // Re-opened inside the delay: keep the pause we already hold and drop
        // the pending resume. This is the measured 3.1 s case — the panel
        // dismissed itself on a silence and the user pressed Win+H again.
        if (m_holdsPause)
            return Action::CancelResume;

        if (!player.playing)
            return Action::Nothing;

        m_holdsPause = true;
        m_heldTrackId = player.trackId;
        return Action::Pause;
    }

    if (!m_holdsPause)
        return Action::Nothing;

    return Action::ArmResume;
}

MicPausePolicy::Action MicPausePolicy::resumeDelayElapsed(const Player &player)
{
    if (!m_holdsPause)
        return Action::Nothing;

    // The microphone came back while the delay was running and something
    // failed to cancel it. Staying paused is the safe reading.
    if (m_micInUse)
        return Action::Nothing;

    m_holdsPause = false;
    const QString held = std::exchange(m_heldTrackId, QString());

    // Last check before making noise. playerChanged() should already have
    // dropped the claim in every one of these cases, but this is the call that
    // actually starts the music, so it verifies rather than assumes.
    if (!player.paused || player.trackId != held)
        return Action::Nothing;

    return Action::Resume;
}

MicPausePolicy::Action MicPausePolicy::playerChanged(const Player &player)
{
    if (!m_holdsPause)
        return Action::Nothing;

    // Still paused on the same track: this is our own pause looking back at
    // us, including the authoritative snapshot that confirms it.
    if (player.paused && player.trackId == m_heldTrackId)
        return Action::Nothing;

    // Anything else — playing, stopped, or paused on a different track — means
    // the transport moved without us. The claim and the pending resume both go.
    m_holdsPause = false;
    m_heldTrackId.clear();
    return Action::CancelResume;
}

// ─────────────────────────────────────────────────────────────────────────────
// The wiring.

MicPauseController::MicPauseController(PlaybackController *player, Settings *settings,
                                       QObject *parent)
    : QObject(parent)
    , m_player(player)
    , m_settings(settings)
    , m_watcher(new MicWatcher(this))
    , m_resumeDelay(new QTimer(this))
{
    m_resumeDelay->setSingleShot(true);

    connect(m_watcher, &MicWatcher::micInUseChanged,
            this, &MicPauseController::onMicInUseChanged);
    connect(m_resumeDelay, &QTimer::timeout, this, [this] {
        apply(m_policy.resumeDelayElapsed(snapshot()));
    });

    // Both, because a track change while the player sits paused is one of the
    // ways the claim stops being ours.
    connect(m_player, &PlaybackController::stateChanged,
            this, &MicPauseController::onPlayerChanged);
    connect(m_player, &PlaybackController::trackChanged,
            this, &MicPauseController::onPlayerChanged);

    connect(m_settings, &Settings::interfaceChanged,
            this, &MicPauseController::applySettings);
    applySettings();
}

bool MicPauseController::isAvailable() const
{
    return m_watcher->isAvailable();
}

MicPausePolicy::Player MicPauseController::snapshot() const
{
    return { m_player->isPlaying(), m_player->isPaused(), m_player->trackId() };
}

void MicPauseController::applySettings()
{
    const bool wanted = m_settings->pauseWhileMicInUse() && isAvailable();
    if (wanted != m_policy.isEnabled())
        apply(m_policy.setEnabled(wanted));

    m_resumeDelay->setInterval(m_settings->micResumeDelayMs());
    m_watcher->setWatching(wanted);
}

void MicPauseController::onMicInUseChanged(bool inUse)
{
    apply(m_policy.micInUseChanged(inUse, snapshot()));
}

void MicPauseController::onPlayerChanged()
{
    apply(m_policy.playerChanged(snapshot()));
}

void MicPauseController::apply(MicPausePolicy::Action action)
{
    switch (action) {
    case MicPausePolicy::Action::Nothing:
        break;
    case MicPausePolicy::Action::Pause:
        m_resumeDelay->stop();
        m_player->pause();
        break;
    case MicPausePolicy::Action::ArmResume:
        m_resumeDelay->start();
        break;
    case MicPausePolicy::Action::CancelResume:
        m_resumeDelay->stop();
        break;
    case MicPausePolicy::Action::Resume:
        m_resumeDelay->stop();
        m_player->play();
        break;
    }
}
