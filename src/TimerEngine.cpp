#include "TimerEngine.h"

TimerEngine::TimerEngine(QObject* parent)
    : QObject(parent)
{
    // Poll at 100 ms so we catch every second boundary cleanly even if the
    // OS delays a tick by a few ms — far more reliable than a 1 s interval.
    m_timer.setInterval(100);
    connect(&m_timer, &QTimer::timeout, this, &TimerEngine::onTick);
}

void TimerEngine::startCountdown(int totalSeconds)
{
    if (totalSeconds <= 0) {
        emit triggered();
        return;
    }
    m_mode      = TimerMode::Countdown;
    m_remaining = totalSeconds;
    m_target    = QDateTime::currentDateTime().addSecs(totalSeconds);
    m_lastEmitted = totalSeconds;
    m_timer.start();
    emit tick(m_remaining);
}

void TimerEngine::startScheduled(const QDateTime& target)
{
    qint64 msLeft = QDateTime::currentDateTime().msecsTo(target);
    if (msLeft <= 0) {
        emit invalidTarget();
        return;
    }
    m_mode        = TimerMode::Scheduled;
    m_target      = target;
    m_remaining   = static_cast<int>(msLeft / 1000);
    m_lastEmitted = m_remaining;
    m_timer.start();
    emit tick(m_remaining);
}

void TimerEngine::stop()
{
    m_timer.stop();
    m_remaining   = 0;
    m_lastEmitted = -1;
}

void TimerEngine::onTick()
{
    // Compute remaining seconds by flooring ms remaining.
    // We poll at 100 ms, so we'll hit each second boundary within ~100 ms —
    // never skipping one, never double-firing it.
    qint64 msLeft   = QDateTime::currentDateTime().msecsTo(m_target);
    int    secs     = static_cast<int>(msLeft / 1000);
    m_remaining     = qMax(0, secs);

    // Only emit tick() when the displayed second actually changes.
    // This prevents redundant repaints on the 9 out of 10 polls that
    // land inside the same second.
    if (m_remaining != m_lastEmitted) {
        m_lastEmitted = m_remaining;
        emit tick(m_remaining);
    }

    if (m_remaining == 0) {
        m_timer.stop();
        emit triggered();
    }
}
