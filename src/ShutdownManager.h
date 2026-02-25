#pragma once

#include <QObject>
#include <QString>
#include <windows.h>
#include <powrprof.h>   // SetSuspendState — link with PowrProf.lib (see CMakeLists)

enum class ShutdownAction {
    Shutdown,
    Restart,
    Hibernate,
    Sleep
};

class ShutdownManager : public QObject
{
    Q_OBJECT
public:
    explicit ShutdownManager(QObject* parent = nullptr);

    // Schedule a shutdown/restart after |seconds| delay,
    // or hibernate/sleep immediately (no delay — timer fires at t=0).
    // force=true kills apps without prompting them to save
    // (ignored for Hibernate and Sleep).
    // Returns true on success, sets lastError() on failure.
    bool scheduleShutdown(ShutdownAction action, DWORD seconds, bool force);

    // Cancel any pending scheduled shutdown.
    bool cancelShutdown();

    // Check if a shutdown is currently pending (best-effort via AbortSystemShutdown side-effect).
    bool isPending() const { return m_pending; }

    // Query the system's power capabilities.
    // Call these once at startup to decide whether to grey out UI options.
    static bool isHibernateAvailable();
    static bool isSleepAvailable();

    QString lastError() const { return m_lastError; }

signals:
    void shutdownScheduled(ShutdownAction action, DWORD seconds);
    void shutdownCancelled();
    void errorOccurred(const QString& message);

private:
    bool acquireShutdownPrivilege();

    bool    m_pending   = false;
    QString m_lastError;
};
