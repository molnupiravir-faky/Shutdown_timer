#include "TaskScheduler.h"

#include <windows.h>
#include <comdef.h>
#include <taskschd.h>
#include <QString>
#include <QDir>
#include <QCoreApplication>

// Note: taskschd and ole32 are linked via CMakeLists.txt — no #pragma comment needed

TaskScheduler::TaskScheduler(QObject* parent)
    : QObject(parent)
{
    initCOM();
}

TaskScheduler::~TaskScheduler()
{
    uninitCOM();
}

bool TaskScheduler::initCOM()
{
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        m_comInitialized = true;
        return true;
    }
    m_lastError = QString("CoInitializeEx failed: 0x%1").arg((DWORD)hr, 8, 16, QChar('0'));
    return false;
}

void TaskScheduler::uninitCOM()
{
    if (m_comInitialized) {
        CoUninitialize();
        m_comInitialized = false;
    }
}

// ── Helper: connect to Task Scheduler service ─────────────────────────────────

static HRESULT connectTaskService(ITaskService** ppService)
{
    HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_ITaskService,
                                  reinterpret_cast<void**>(ppService));
    if (FAILED(hr)) return hr;

    hr = (*ppService)->Connect(_variant_t(), _variant_t(),
                               _variant_t(), _variant_t());
    if (FAILED(hr)) {
        (*ppService)->Release();
        *ppService = nullptr;
    }
    return hr;
}

// ── createAutoClearTask ───────────────────────────────────────────────────────

bool TaskScheduler::createAutoClearTask()
{
    // Full path to this exe
    QString exePath = QCoreApplication::applicationFilePath();
    exePath = QDir::toNativeSeparators(exePath);

    // ── Security: refuse to create a privileged task if the exe lives outside
    // Program Files. A user-writable path + a high-privilege scheduled task is
    // a privilege escalation risk, especially for an open source app where the
    // task name and argument are publicly known.
    {
        wchar_t progFiles[MAX_PATH]   = {};
        wchar_t progFilesX86[MAX_PATH] = {};
        GetEnvironmentVariableW(L"ProgramFiles",   progFiles,    MAX_PATH);
        GetEnvironmentVariableW(L"ProgramFiles(x86)", progFilesX86, MAX_PATH);

        QString pf    = QString::fromWCharArray(progFiles).toLower();
        QString pfx86 = QString::fromWCharArray(progFilesX86).toLower();
        QString exeLower = exePath.toLower();

        if (!exeLower.startsWith(pf) && !exeLower.startsWith(pfx86)) {
            m_lastError = tr(
                "Auto-clear task refused:\n"
                "The application must be installed in Program Files\n"
                "to create a privileged scheduled task.\n\n"
                "Current path:\n%1").arg(exePath);
            return false;
        }
    }

    std::wstring wsExe   = exePath.toStdWString();
    std::wstring wsArgs  = L"--auto-clear";

    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) {
        m_lastError = QString("ITaskService::Connect failed: 0x%1").arg((DWORD)hr, 8, 16, QChar('0'));
        return false;
    }

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        m_lastError = "GetFolder failed";
        pService->Release();
        return false;
    }

    ITaskDefinition* pTask = nullptr;
    hr = pService->NewTask(0, &pTask);
    if (FAILED(hr)) {
        m_lastError = "NewTask failed";
        pRootFolder->Release();
        pService->Release();
        return false;
    }

    // ── Registration info ─────────────────────────────────────────────────────
    IRegistrationInfo* pRegInfo = nullptr;
    pTask->get_RegistrationInfo(&pRegInfo);
    if (pRegInfo) {
        pRegInfo->put_Author(_bstr_t(L"Shutdown Timer"));
        pRegInfo->put_Description(_bstr_t(L"Clears the startup message after next login."));
        pRegInfo->Release();
    }

    // ── Principal: run as the current logged-in user at highest privilege ───────
    // We deliberately avoid SYSTEM here — the task only needs to clear two
    // registry values and delete itself. Running as SYSTEM is overkill and
    // creates unnecessary risk if the exe is ever replaced. Administrator
    // privilege (TASK_RUNLEVEL_HIGHEST) is sufficient for HKLM registry writes.
    IPrincipal* pPrincipal = nullptr;
    pTask->get_Principal(&pPrincipal);
    if (pPrincipal) {
        // Empty UserId = runs as the user who triggers the logon
        pPrincipal->put_UserId(_bstr_t(L""));
        pPrincipal->put_LogonType(TASK_LOGON_INTERACTIVE_TOKEN);
        pPrincipal->put_RunLevel(TASK_RUNLEVEL_HIGHEST);
        pPrincipal->Release();
    }

    // ── Settings ──────────────────────────────────────────────────────────────
    ITaskSettings* pSettings = nullptr;
    pTask->get_Settings(&pSettings);
    if (pSettings) {
        pSettings->put_StartWhenAvailable(VARIANT_TRUE);
        pSettings->put_DisallowStartIfOnBatteries(VARIANT_FALSE);
        pSettings->put_StopIfGoingOnBatteries(VARIANT_FALSE);
        pSettings->put_ExecutionTimeLimit(_bstr_t(L"PT5M"));
        // Note: put_DeleteExpiredTaskAfter requires a trigger expiry date to be set.
        // We skip it — the task deletes itself by calling schtasks /delete inside
        // the --auto-clear handler instead.
        pSettings->Release();
    }

    // ── Trigger: ONLOGON ──────────────────────────────────────────────────────
    // Note: ILogonTrigger is not available in MinGW's taskschd.h.
    // ITrigger is sufficient — TASK_TRIGGER_LOGON type with no UserId set
    // fires for any user login, which is exactly what we need.
    ITriggerCollection* pTriggers = nullptr;
    pTask->get_Triggers(&pTriggers);
    if (pTriggers) {
        ITrigger* pTrigger = nullptr;
        pTriggers->Create(TASK_TRIGGER_LOGON, &pTrigger);
        if (pTrigger) {
            pTrigger->put_Id(_bstr_t(L"LogonTrigger"));
            // No UserId set = fires for ANY user login
            pTrigger->Release();
        }
        pTriggers->Release();
    }

    // ── Action: run registry_helper.exe --auto-clear ──────────────────────────
    IActionCollection* pActions = nullptr;
    pTask->get_Actions(&pActions);
    if (pActions) {
        IAction* pAction = nullptr;
        pActions->Create(TASK_ACTION_EXEC, &pAction);
        if (pAction) {
            IExecAction* pExecAction = nullptr;
            pAction->QueryInterface(IID_IExecAction,
                                    reinterpret_cast<void**>(&pExecAction));
            if (pExecAction) {
                pExecAction->put_Path(_bstr_t(wsExe.c_str()));
                pExecAction->put_Arguments(_bstr_t(wsArgs.c_str()));
                pExecAction->Release();
            }
            pAction->Release();
        }
        pActions->Release();
    }

    // ── Register the task ─────────────────────────────────────────────────────
    IRegisteredTask* pRegisteredTask = nullptr;
    hr = pRootFolder->RegisterTaskDefinition(
        _bstr_t(kTaskName),
        pTask,
        TASK_CREATE_OR_UPDATE,
        _variant_t(),                   // no explicit user — uses principal above
        _variant_t(),                   // no password
        TASK_LOGON_INTERACTIVE_TOKEN,
        _variant_t(L""),
        &pRegisteredTask
    );

    bool ok = SUCCEEDED(hr);
    if (!ok) {
        m_lastError = QString("RegisterTaskDefinition failed: 0x%1")
                      .arg((DWORD)hr, 8, 16, QChar('0'));
    }

    if (pRegisteredTask) pRegisteredTask->Release();
    pTask->Release();
    pRootFolder->Release();
    pService->Release();
    return ok;
}

// ── deleteAutoClearTask ───────────────────────────────────────────────────────

bool TaskScheduler::deleteAutoClearTask()
{
    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) return true; // Nothing to delete

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        return true;
    }

    hr = pRootFolder->DeleteTask(_bstr_t(kTaskName), 0);
    bool ok = SUCCEEDED(hr) || hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);

    if (!ok) {
        m_lastError = QString("DeleteTask failed: 0x%1").arg((DWORD)hr, 8, 16, QChar('0'));
    }

    pRootFolder->Release();
    pService->Release();
    return ok;
}

// ── autoClearTaskExists ───────────────────────────────────────────────────────

bool TaskScheduler::autoClearTaskExists()
{
    ITaskService* pService = nullptr;
    HRESULT hr = connectTaskService(&pService);
    if (FAILED(hr)) return false;

    ITaskFolder* pRootFolder = nullptr;
    hr = pService->GetFolder(_bstr_t(kTaskFolder), &pRootFolder);
    if (FAILED(hr)) {
        pService->Release();
        return false;
    }

    IRegisteredTask* pTask = nullptr;
    hr = pRootFolder->GetTask(_bstr_t(kTaskName), &pTask);
    bool exists = SUCCEEDED(hr);

    if (pTask) pTask->Release();
    pRootFolder->Release();
    pService->Release();
    return exists;
}
