#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QTimer>

#include <windows.h>
#include <comdef.h>
#include <taskschd.h>
#include <ole2.h>

#include "MainWindow.h"
#include "RegistryManager.h"
#include "TaskScheduler.h"

// ── Auto-clear mode ───────────────────────────────────────────────────────────
// When the scheduled task fires at boot, it launches:
//   ShutdownTimer.exe --auto-clear
// We handle this before initializing Qt — it's a headless, instant operation.
// Clears the registry startup message, deletes the scheduled task, exits.

static bool handleAutoClear(int argc, char* argv[])
{
    // Require exactly: ShutdownTimer.exe --auto-clear
    // Reject if --auto-clear appears alongside other arguments — prevents the
    // privileged handler from firing unexpectedly on a malformed command line.
    if (argc != 2 || strcmp(argv[1], "--auto-clear") != 0)
        return false;

    {
            // Direct Win32 registry clear — no Qt needed
            HKEY hKey = nullptr;
            LONG r = RegCreateKeyExW(
                HKEY_LOCAL_MACHINE,
                L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon",
                0, nullptr, REG_OPTION_NON_VOLATILE,
                KEY_SET_VALUE | KEY_WOW64_64KEY,
                nullptr, &hKey, nullptr
            );
            if (r == ERROR_SUCCESS) {
                // Write empty strings to clear the legal notice values
                const wchar_t empty[] = L"";
                DWORD bytes = sizeof(wchar_t); // just the null terminator
                RegSetValueExW(hKey, L"LegalNoticeCaption", 0, REG_SZ,
                               reinterpret_cast<const BYTE*>(empty), bytes);
                RegSetValueExW(hKey, L"LegalNoticeText",    0, REG_SZ,
                               reinterpret_cast<const BYTE*>(empty), bytes);
                RegCloseKey(hKey);
            }

            // Delete the scheduled task via COM
            // We init COM minimally here — no Qt, no UI
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            {
                ITaskService* pService = nullptr;
                HRESULT hr = CoCreateInstance(CLSID_TaskScheduler, nullptr,
                                              CLSCTX_INPROC_SERVER,
                                              IID_ITaskService,
                                              reinterpret_cast<void**>(&pService));
                if (SUCCEEDED(hr)) {
                    hr = pService->Connect(_variant_t(), _variant_t(),
                                           _variant_t(), _variant_t());
                    if (SUCCEEDED(hr)) {
                        ITaskFolder* pFolder = nullptr;
                        if (SUCCEEDED(pService->GetFolder(_bstr_t(L"\\"), &pFolder))) {
                            pFolder->DeleteTask(_bstr_t(L"ShutdownTimerAutoClearMsg"), 0);
                            pFolder->Release();
                        }
                    }
                    pService->Release();
                }
            }
            CoUninitialize();

        return true; // signal: exit immediately, do not start Qt
    }
}

// ── Main ──────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    // Handle --auto-clear before any Qt initialization.
    // This keeps the boot-time process tiny and fast.
    if (handleAutoClear(argc, argv)) {
        return 0;
    }

    // Normal app startup
    QApplication app(argc, argv);
    app.setApplicationName("ShutdownTimer");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("ShutdownTimer");

    // Prevent app from quitting when main window is hidden to tray
    app.setQuitOnLastWindowClosed(false);

    MainWindow window;
    window.show();

    // ── Force window icon via Win32 API ───────────────────────────────────────
    // Qt's setWindowIcon() is unreliable on Windows 10/11 — the title bar and
    // taskbar icon come from the exe's embedded IDI_APPICON resource, which
    // Windows loads automatically. We reinforce this by explicitly sending
    // WM_SETICON with both the large (32px, ALT+TAB) and small (16px, title
    // bar) icon handles loaded directly from the exe resource.
    {
        HWND hwnd = reinterpret_cast<HWND>(window.winId());
        HINSTANCE hInst = GetModuleHandleW(nullptr);

        // Icon resource ID 101 as defined in resources.rc
        HICON hIconBig = static_cast<HICON>(
            LoadImageW(hInst, MAKEINTRESOURCEW(101),
                       IMAGE_ICON,
                       GetSystemMetrics(SM_CXICON),    // 32px (or DPI-scaled)
                       GetSystemMetrics(SM_CYICON),
                       LR_DEFAULTCOLOR));

        HICON hIconSmall = static_cast<HICON>(
            LoadImageW(hInst, MAKEINTRESOURCEW(101),
                       IMAGE_ICON,
                       GetSystemMetrics(SM_CXSMICON),  // 16px (or DPI-scaled)
                       GetSystemMetrics(SM_CYSMICON),
                       LR_DEFAULTCOLOR));

        if (hIconBig)
            SendMessageW(hwnd, WM_SETICON, ICON_BIG,
                         reinterpret_cast<LPARAM>(hIconBig));
        if (hIconSmall)
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL,
                         reinterpret_cast<LPARAM>(hIconSmall));
    }

    // Defer showTrayIcon() until the event loop is running — calling
    // QSystemTrayIcon::show() before app.exec() starts means the Windows
    // Shell notification area has not processed the registration yet, causing
    // the icon to silently not appear on Qt6 / Windows 10/11.
    QTimer::singleShot(0, &window, &MainWindow::showTrayIcon);

    return app.exec();
}
