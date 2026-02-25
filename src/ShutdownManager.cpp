#include "ShutdownManager.h"
#include <QDebug>

ShutdownManager::ShutdownManager(QObject* parent)
    : QObject(parent)
{}

// ── Power capability queries ──────────────────────────────────────────────────
// GetPwrCapabilities() fills a SYSTEM_POWER_CAPABILITIES struct that tells us
// exactly what the hardware + OS configuration supports. We call this once at
// startup so the UI can grey out unavailable options before the user picks one.

bool ShutdownManager::isHibernateAvailable()
{
    SYSTEM_POWER_CAPABILITIES caps{};
    if (!GetPwrCapabilities(&caps))
        return false;
    // HiberFilePresent: the hibernation file (hiberfil.sys) exists and is valid.
    // SystemS4:         the hardware supports the S4 (hibernate) sleep state.
    // Both must be true — S4 hardware support alone isn't enough if hibernation
    // has been disabled via "powercfg /h off" (which removes the hiberfile).
    return caps.HiberFilePresent && caps.SystemS4;
}

bool ShutdownManager::isSleepAvailable()
{
    SYSTEM_POWER_CAPABILITIES caps{};
    if (!GetPwrCapabilities(&caps))
        return false;
    // SystemS3: the hardware supports the S3 (suspend-to-RAM / sleep) state.
    // Note: Modern Standby (S0ix) machines may report SystemS3=FALSE but still
    // support SetSuspendState — however detecting that requires a newer SDK than
    // MinGW provides (AoAc field). In practice, if SystemS3 is false on a
    // desktop/laptop, sleep genuinely isn't available, so this is safe.
    return caps.SystemS3 != FALSE;
}

// ── Acquire SE_SHUTDOWN_NAME privilege ────────────────────────────────────────
// Required before calling InitiateSystemShutdownEx.
// Since we run with requireAdministrator, this always succeeds.
bool ShutdownManager::acquireShutdownPrivilege()
{
    HANDLE hToken;
    if (!OpenProcessToken(GetCurrentProcess(),
                          TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY,
                          &hToken))
    {
        m_lastError = QString("OpenProcessToken failed: %1").arg(GetLastError());
        return false;
    }

    TOKEN_PRIVILEGES tp{};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME,
                               &tp.Privileges[0].Luid))
    {
        m_lastError = QString("LookupPrivilegeValue failed: %1").arg(GetLastError());
        CloseHandle(hToken);
        return false;
    }

    BOOL ok = AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    DWORD adjustErr = GetLastError(); // save immediately — CloseHandle will overwrite it
    CloseHandle(hToken);

    if (!ok || adjustErr == ERROR_NOT_ALL_ASSIGNED) {
        m_lastError = QString("AdjustTokenPrivileges failed: %1").arg(adjustErr);
        return false;
    }
    return true;
}

bool ShutdownManager::scheduleShutdown(ShutdownAction action, DWORD seconds, bool force)
{
    // ── Hibernate and Sleep are handled separately ────────────────────────────
    // SetSuspendState() is immediate — no delay parameter exists.
    // The timer in TimerEngine fires us at t=0, so we just call it directly.
    // "force" is ignored for both: the system always saves state and suspends
    // regardless of open applications — the concept doesn't apply.
    if (action == ShutdownAction::Hibernate || action == ShutdownAction::Sleep) {
        // SetSuspendState(hibernate, forceCritical, disableWakeEvent)
        //   hibernate=TRUE  → S4 hibernate (RAM written to disk, power cut)
        //   hibernate=FALSE → S3 sleep     (RAM kept powered, fast resume)
        BOOL hibernate = (action == ShutdownAction::Hibernate) ? TRUE : FALSE;
        BOOL ok = SetSuspendState(hibernate, FALSE, FALSE);
        if (!ok) {
            DWORD err = GetLastError();
            m_lastError = QString("SetSuspendState(%1) failed: %2")
                          .arg(hibernate ? "hibernate" : "sleep")
                          .arg(err);
            emit errorOccurred(m_lastError);
            return false;
        }
        m_pending = false; // instantaneous, nothing to cancel
        emit shutdownScheduled(action, 0);
        return true;
    }

    if (!acquireShutdownPrivilege()) {
        emit errorOccurred(m_lastError);
        return false;
    }

    // Reason code: planned, hardware maintenance (0x80040001)
    // This prevents the "unexpected shutdown" entry in Event Log.
    const DWORD reason = SHTDN_REASON_MAJOR_HARDWARE |
                         SHTDN_REASON_MINOR_MAINTENANCE |
                         SHTDN_REASON_FLAG_PLANNED;

    BOOL restart = (action == ShutdownAction::Restart) ? TRUE : FALSE;

    // force=true: EWX_FORCEIFHUNG or EWX_FORCE
    // force=false: apps get a chance to save / block shutdown
    //
    // InitiateSystemShutdownEx is preferred over ExitWindowsEx for
    // delayed shutdown — it shows the countdown dialog natively.
    BOOL ok = InitiateSystemShutdownExW(
        nullptr,               // local machine
        nullptr,               // no message (we manage our own UI)
        seconds,               // delay in seconds
        force ? TRUE : FALSE,  // bForceAppsClosed
        restart,               // bRebootAfterShutdown
        reason
    );

    if (!ok) {
        DWORD err = GetLastError();
        m_lastError = QString("InitiateSystemShutdownEx failed: %1").arg(err);
        emit errorOccurred(m_lastError);
        return false;
    }

    m_pending = true;
    emit shutdownScheduled(action, seconds);
    return true;
}

bool ShutdownManager::cancelShutdown()
{
    if (!acquireShutdownPrivilege()) {
        emit errorOccurred(m_lastError);
        return false;
    }

    // AbortSystemShutdown cancels a pending InitiateSystemShutdownEx.
    // Returns FALSE if nothing was pending — we treat that as success too.
    AbortSystemShutdown(nullptr);

    m_pending = false;
    emit shutdownCancelled();
    return true;
}
