#pragma once

#include <QObject>
#include <QString>

class TaskScheduler : public QObject
{
    Q_OBJECT
public:
    explicit TaskScheduler(QObject* parent = nullptr);
    ~TaskScheduler();

    // Create a one-shot ONLOGON task that runs:
    //   <thisExe> --auto-clear
    // as SYSTEM at next login, then deletes itself via the --auto-clear handler.
    bool createAutoClearTask();

    // Delete the auto-clear task if it exists.
    bool deleteAutoClearTask();

    // Check if the task currently exists.
    bool autoClearTaskExists();

    QString lastError() const { return m_lastError; }

private:
    bool initCOM();
    void uninitCOM();

    bool   m_comInitialized = false;
    QString m_lastError;

    static constexpr wchar_t kTaskName[] = L"ShutdownTimerAutoClearMsg";
    static constexpr wchar_t kTaskFolder[] = L"\\";
};
