#pragma once

#include <QMainWindow>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QLabel>
#include <QTabWidget>
#include <QPushButton>
#include <QTimeEdit>
#include <QDateTimeEdit>
#include <QRadioButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QStackedWidget>

#include "TimerEngine.h"
#include "ShutdownManager.h"
#include "RegistryManager.h"
#include "TaskScheduler.h"
#include "LanguageManager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();
    void showTrayIcon();

protected:
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;

private slots:
    // Timer tab
    void onStartClicked();
    void onCancelClicked();
    void onTimerTick(int remainingSeconds);
    void onTimerTriggered();
    void onTimerInvalidTarget();
    void onModeToggled();
    void onActionToggled();
    void onPresetClicked(int seconds);

    // Message tab
    void onSaveMessageClicked();
    void onClearMessageClicked();
    void onLoadMessageClicked();

    // Menu bar
    void onMenuExit();
    void onMenuAbout();
    void onMenuViewReleases();
    void onMenuLanguageTriggered(QAction* action);

    // Tray
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShowHide();
    void onTrayQuit();

    // Language
    void onLanguageApplied(AppLanguage lang);

private:
    void buildUI();
    void buildTimerTab();
    void buildMessageTab();
    void buildMenuBar();
    void buildTray();
    void setupConnections();
    void retranslateUI();

    void updateCountdownDisplay(int seconds);
    void setTimerRunning(bool running);
    QString formatDuration(int seconds) const;

    void showError(const QString& msg);

    // ── Core components ───────────────────────────────────────────────────────
    TimerEngine*     m_timerEngine   = nullptr;
    ShutdownManager* m_shutdownMgr   = nullptr;
    RegistryManager* m_registryMgr   = nullptr;
    TaskScheduler*   m_taskScheduler = nullptr;
    LanguageManager* m_langMgr       = nullptr;

    // ── Menu bar ──────────────────────────────────────────────────────────────
    QMenu*        m_settingsMenu       = nullptr;
    QMenu*        m_langSubMenu        = nullptr;
    QActionGroup* m_langActionGroup    = nullptr;
    QAction*      m_langEnAction       = nullptr;
    QAction*      m_langArAction       = nullptr;
    QAction*      m_langKoAction       = nullptr;
    QAction*      m_langEsAction       = nullptr;
    QAction*      m_langFrAction       = nullptr;
    QAction*      m_langDeAction       = nullptr;
    QAction*      m_langPtBRAction     = nullptr;
    QAction*      m_langZhCNAction     = nullptr;
    QAction*      m_langJaAction       = nullptr;
    QMenu*        m_helpMenu           = nullptr;
    QAction*      m_aboutAction        = nullptr;
    QAction*      m_viewReleasesAction = nullptr;
    QAction*      m_exitAction         = nullptr;

    // ── Tray ──────────────────────────────────────────────────────────────────
    QSystemTrayIcon* m_trayIcon           = nullptr;
    QMenu*           m_trayMenu           = nullptr;
    QAction*         m_trayShowHideAction = nullptr;
    QAction*         m_trayQuitAction     = nullptr;
    QAction*         m_trayCancelAction   = nullptr;

    // ── Main layout ───────────────────────────────────────────────────────────
    QTabWidget* m_tabs = nullptr;

    // ── Timer tab ─────────────────────────────────────────────────────────────
    QGroupBox*    m_modeGroup      = nullptr;
    QGroupBox*    m_actionGroup    = nullptr;
    QRadioButton* m_radioCountdown = nullptr;
    QRadioButton* m_radioScheduled = nullptr;

    // Countdown inputs
    QTimeEdit*   m_countdownEdit  = nullptr;

    // Preset buttons
    QPushButton* m_preset15m  = nullptr;
    QPushButton* m_preset30m  = nullptr;
    QPushButton* m_preset1h   = nullptr;
    QPushButton* m_preset2h   = nullptr;

    // Scheduled input
    QDateTimeEdit* m_scheduledEdit = nullptr;

    QStackedWidget* m_inputStack = nullptr;

    // Action
    QRadioButton* m_radioShutdown  = nullptr;
    QRadioButton* m_radioRestart   = nullptr;
    QRadioButton* m_radioHibernate = nullptr;
    QRadioButton* m_radioSleep     = nullptr;
    QCheckBox*    m_forceCheck     = nullptr;

    // Controls
    QPushButton* m_startBtn  = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    // Status
    QLabel* m_countdownLabel = nullptr;
    QLabel* m_statusLabel    = nullptr;

    // ── Message tab ───────────────────────────────────────────────────────────
    QLabel*      m_msgInfoLabel   = nullptr;
    QLabel*      m_msgTitleLabel  = nullptr;
    QLabel*      m_msgBodyLabel   = nullptr;
    QLineEdit*   m_msgTitleEdit   = nullptr;
    QTextEdit*   m_msgBodyEdit    = nullptr;
    QCheckBox*   m_autoClearCheck = nullptr;
    QPushButton* m_saveMsgBtn     = nullptr;
    QPushButton* m_clearMsgBtn    = nullptr;
    QPushButton* m_loadMsgBtn     = nullptr;
    QLabel*      m_msgStatusLabel = nullptr;

    bool m_forceQuit = false;
};
