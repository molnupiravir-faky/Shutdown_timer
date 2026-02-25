#include "MainWindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QMessageBox>
#include <QCloseEvent>
#include <QIcon>
#include <QPixmap>
#include <QFont>
#include <QSettings>
#include <QDir>
#include <QTimer>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>

// ── Build a multi-resolution QIcon ───────────────────────────────────────────
// The .ico is natively supported without any imageformat plugin, so it is
// loaded first as the guaranteed base. Individual PNGs are added on top when
// the imageformats plugin is present so Qt can pick the sharpest size per
// context (title bar, taskbar, tray, Alt+Tab, HiDPI, etc.).
static QIcon buildAppIcon()
{
    QIcon icon(":/assets/icon.ico");   // always works — no plugin needed

    const char* pngPaths[] = {
        ":/assets/shutdown16px.png",
        ":/assets/shutdown24px.png",
        ":/assets/shutdown32px.png",
        ":/assets/shutdown64px.png",
        ":/assets/shutdown128px.png",
        ":/assets/shutdown256px.png",
        ":/assets/shutdown512px.png"
    };
    for (const char* path : pngPaths) {
        QPixmap pm(path);
        if (!pm.isNull())
            icon.addPixmap(pm);
    }
    return icon;
}

// ── Constructor ───────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_timerEngine    = new TimerEngine(this);
    m_shutdownMgr    = new ShutdownManager(this);
    m_registryMgr    = new RegistryManager(this);
    m_taskScheduler  = new TaskScheduler(this);
    m_langMgr        = new LanguageManager(this);

    buildMenuBar();
    buildUI();
    buildTray();
    setupConnections();
    retranslateUI();

    setWindowIcon(buildAppIcon());

    // Restore saved language
    QSettings settings("ShutdownTimer", "ShutdownTimer");
    QString langCode = settings.value("language", "en").toString();
    AppLanguage savedLang = LanguageManager::fromCode(langCode);
    QAction* toCheck = m_langEnAction;
    if (savedLang == AppLanguage::Arabic)           toCheck = m_langArAction;
    if (savedLang == AppLanguage::Korean)           toCheck = m_langKoAction;
    if (savedLang == AppLanguage::Spanish)          toCheck = m_langEsAction;
    if (savedLang == AppLanguage::French)           toCheck = m_langFrAction;
    if (savedLang == AppLanguage::German)           toCheck = m_langDeAction;
    if (savedLang == AppLanguage::PortugueseBR)     toCheck = m_langPtBRAction;
    if (savedLang == AppLanguage::ChineseSimplified)toCheck = m_langZhCNAction;
    if (savedLang == AppLanguage::Japanese)         toCheck = m_langJaAction;
    toCheck->setChecked(true);
    m_langMgr->applyLanguage(savedLang);

    setMinimumSize(480, 520);
    resize(500, 560);

    // Restore last window position/size — Windows apps are expected to
    // remember geometry between sessions. restoreGeometry() handles DPI
    // scaling automatically via Qt's geometry state format.
    QSettings geoSettings("ShutdownTimer", "ShutdownTimer");
    QByteArray savedGeometry = geoSettings.value("windowGeometry").toByteArray();
    if (!savedGeometry.isEmpty())
        restoreGeometry(savedGeometry);
}

MainWindow::~MainWindow() {}

// ── Build menu bar ────────────────────────────────────────────────────────────
// NOTE: On Windows, adding a raw QAction directly to the menu bar (bar->addAction)
// interferes with adjacent menu hit-testing and makes neighbouring menus
// unclickable in some Qt versions. The safe pattern is to always use QMenu.
// Exit therefore lives at the bottom of Settings, separated by a divider —
// which is also the standard Windows convention (File > Exit, Settings > Exit).

void MainWindow::buildMenuBar()
{
    QMenuBar* bar = menuBar();

    // ── Settings (Language + separator + Exit) ────────────────────────────────
    m_settingsMenu = bar->addMenu(tr("Settings"));

    m_langSubMenu     = m_settingsMenu->addMenu(tr("Language"));
    m_langActionGroup = new QActionGroup(this);
    m_langActionGroup->setExclusive(true);

    m_langEnAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::English));
    m_langEnAction->setCheckable(true);
    m_langEnAction->setChecked(true);
    m_langActionGroup->addAction(m_langEnAction);

    m_langArAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::Arabic));
    m_langArAction->setCheckable(true);
    m_langActionGroup->addAction(m_langArAction);

    m_langKoAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::Korean));
    m_langKoAction->setCheckable(true);
    m_langActionGroup->addAction(m_langKoAction);

    m_langEsAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::Spanish));
    m_langEsAction->setCheckable(true);
    m_langActionGroup->addAction(m_langEsAction);

    m_langFrAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::French));
    m_langFrAction->setCheckable(true);
    m_langActionGroup->addAction(m_langFrAction);

    m_langDeAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::German));
    m_langDeAction->setCheckable(true);
    m_langActionGroup->addAction(m_langDeAction);

    m_langPtBRAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::PortugueseBR));
    m_langPtBRAction->setCheckable(true);
    m_langActionGroup->addAction(m_langPtBRAction);

    m_langZhCNAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::ChineseSimplified));
    m_langZhCNAction->setCheckable(true);
    m_langActionGroup->addAction(m_langZhCNAction);

    m_langJaAction = m_langSubMenu->addAction(
        LanguageManager::languageDisplayName(AppLanguage::Japanese));
    m_langJaAction->setCheckable(true);
    m_langActionGroup->addAction(m_langJaAction);

    m_settingsMenu->addSeparator();
    m_exitAction = m_settingsMenu->addAction(tr("Exit"));

    // ── Help (About + separator + View Releases) ──────────────────────────────
    m_helpMenu = bar->addMenu(tr("Help"));

    m_aboutAction        = m_helpMenu->addAction(tr("About"));
    m_helpMenu->addSeparator();
    m_viewReleasesAction = m_helpMenu->addAction(tr("View Releases"));
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void MainWindow::buildUI()
{
    QWidget* central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(12, 12, 12, 8);
    mainLayout->setSpacing(8);

    m_tabs = new QTabWidget(this);
    mainLayout->addWidget(m_tabs);

    buildTimerTab();
    buildMessageTab();
}

void MainWindow::buildTimerTab()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    // ── Mode group ────────────────────────────────────────────────────────────
    m_modeGroup = new QGroupBox(tr("Timer Mode"), page);
    QHBoxLayout* modeLayout = new QHBoxLayout(m_modeGroup);
    m_radioCountdown = new QRadioButton(tr("Countdown"), m_modeGroup);
    m_radioScheduled = new QRadioButton(tr("Scheduled Time"), m_modeGroup);
    m_radioCountdown->setChecked(true);
    modeLayout->addWidget(m_radioCountdown);
    modeLayout->addWidget(m_radioScheduled);
    modeLayout->addStretch();
    layout->addWidget(m_modeGroup);

    // ── Input stack ───────────────────────────────────────────────────────────
    m_inputStack = new QStackedWidget(page);

    // Page 0: Countdown HH:MM:SS + preset buttons
    QWidget* countdownPage = new QWidget();
    QVBoxLayout* cdLayout = new QVBoxLayout(countdownPage);
    cdLayout->setContentsMargins(0, 4, 0, 4);
    cdLayout->setSpacing(8);

    m_countdownEdit = new QTimeEdit(QTime(0, 0, 0), countdownPage);
    m_countdownEdit->setDisplayFormat("HH : mm : ss");
    m_countdownEdit->setFixedHeight(48);
    m_countdownEdit->setFixedWidth(200);
    QFont teFont = m_countdownEdit->font();
    teFont.setPointSize(16);
    m_countdownEdit->setFont(teFont);
    m_countdownEdit->setAlignment(Qt::AlignCenter);

    QHBoxLayout* timeRow = new QHBoxLayout();
    timeRow->addStretch();
    timeRow->addWidget(m_countdownEdit);
    timeRow->addStretch();
    cdLayout->addLayout(timeRow);

    // ── Preset buttons ────────────────────────────────────────────────────────
    m_preset15m = new QPushButton(tr("15 min"), countdownPage);
    m_preset30m = new QPushButton(tr("30 min"), countdownPage);
    m_preset1h  = new QPushButton(tr("1 hour"), countdownPage);
    m_preset2h  = new QPushButton(tr("2 hours"), countdownPage);

    for (QPushButton* btn : {m_preset15m, m_preset30m, m_preset1h, m_preset2h}) {
        btn->setFixedHeight(28);
        btn->setFlat(true);
        btn->setCursor(Qt::PointingHandCursor);
    }

    QHBoxLayout* presetRow = new QHBoxLayout();
    presetRow->addStretch();
    presetRow->addWidget(m_preset15m);
    presetRow->addWidget(m_preset30m);
    presetRow->addWidget(m_preset1h);
    presetRow->addWidget(m_preset2h);
    presetRow->addStretch();
    cdLayout->addLayout(presetRow);

    m_inputStack->addWidget(countdownPage);

    // Page 1: Scheduled date/time
    QWidget* scheduledPage = new QWidget();
    QHBoxLayout* scLayout = new QHBoxLayout(scheduledPage);
    scLayout->setContentsMargins(0, 0, 0, 0);
    m_scheduledEdit = new QDateTimeEdit(
        QDateTime::currentDateTime().addSecs(3600), scheduledPage);
    m_scheduledEdit->setDisplayFormat("yyyy-MM-dd  HH:mm:ss");
    m_scheduledEdit->setCalendarPopup(true);
    m_scheduledEdit->setMinimumDateTime(QDateTime::currentDateTime());
    scLayout->addStretch();
    scLayout->addWidget(m_scheduledEdit);
    scLayout->addStretch();
    m_inputStack->addWidget(scheduledPage);

    layout->addWidget(m_inputStack);

    // ── Action group ──────────────────────────────────────────────────────────
    m_actionGroup = new QGroupBox(tr("Action"), page);
    QVBoxLayout* actionLayout = new QVBoxLayout(m_actionGroup);
    QHBoxLayout* actionRow = new QHBoxLayout();
    m_radioShutdown  = new QRadioButton(tr("Shutdown"),  m_actionGroup);
    m_radioRestart   = new QRadioButton(tr("Restart"),   m_actionGroup);
    m_radioHibernate = new QRadioButton(tr("Hibernate"), m_actionGroup);
    m_radioSleep     = new QRadioButton(tr("Sleep"),     m_actionGroup);
    m_radioShutdown->setChecked(true);
    actionRow->addWidget(m_radioShutdown);
    actionRow->addWidget(m_radioRestart);
    actionRow->addWidget(m_radioHibernate);
    actionRow->addWidget(m_radioSleep);
    actionRow->addStretch();

    // Grey out Hibernate / Sleep if the machine doesn't support them.
    // We check once at build time — hardware capabilities don't change at runtime.
    // A tooltip explains why the option is disabled so the user isn't confused.
    if (!ShutdownManager::isHibernateAvailable()) {
        m_radioHibernate->setEnabled(false);
        m_radioHibernate->setToolTip(
            tr("Hibernate is not available on this machine.\n"
               "It may be disabled via: powercfg /h on"));
    }
    if (!ShutdownManager::isSleepAvailable()) {
        m_radioSleep->setEnabled(false);
        m_radioSleep->setToolTip(
            tr("Sleep is not available on this machine."));
    }

    m_forceCheck = new QCheckBox(
        tr("Force (don't wait for apps to close)"), m_actionGroup);
    actionLayout->addLayout(actionRow);
    actionLayout->addWidget(m_forceCheck);
    layout->addWidget(m_actionGroup);

    // ── Countdown display ─────────────────────────────────────────────────────
    m_countdownLabel = new QLabel("--:--:--", page);
    QFont f = m_countdownLabel->font();
    f.setPointSize(32);
    f.setBold(true);
    m_countdownLabel->setFont(f);
    m_countdownLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_countdownLabel);

    m_statusLabel = new QLabel("", page);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_statusLabel);

    // ── Buttons ───────────────────────────────────────────────────────────────
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_startBtn  = new QPushButton(tr("Start"), page);
    m_cancelBtn = new QPushButton(tr("Cancel"), page);
    m_startBtn->setFixedHeight(36);
    m_cancelBtn->setFixedHeight(36);
    m_cancelBtn->setEnabled(false);
    btnLayout->addWidget(m_startBtn);
    btnLayout->addWidget(m_cancelBtn);
    layout->addLayout(btnLayout);

    layout->addStretch();
    m_tabs->addTab(page, tr("Timer"));
}

void MainWindow::buildMessageTab()
{
    QWidget* page = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(page);
    layout->setSpacing(10);

    m_msgInfoLabel = new QLabel(
        tr("This message will be shown on the Windows login screen before the user logs in."),
        page);
    m_msgInfoLabel->setWordWrap(true);
    layout->addWidget(m_msgInfoLabel);

    // Title
    QFormLayout* form = new QFormLayout();
    m_msgTitleLabel = new QLabel(tr("Title:"), page);
    m_msgTitleEdit  = new QLineEdit(page);
    m_msgTitleEdit->setMaxLength(80);
    m_msgTitleEdit->setPlaceholderText(tr("e.g. Important Notice"));
    form->addRow(m_msgTitleLabel, m_msgTitleEdit);
    layout->addLayout(form);

    // Body
    m_msgBodyLabel = new QLabel(tr("Message:"), page);
    layout->addWidget(m_msgBodyLabel);
    m_msgBodyEdit = new QTextEdit(page);
    m_msgBodyEdit->setAcceptRichText(false);
    m_msgBodyEdit->setMaximumHeight(120);
    m_msgBodyEdit->setPlaceholderText(tr("Enter your message here... (max 500 characters)"));
    layout->addWidget(m_msgBodyEdit);

    // Auto-clear
    m_autoClearCheck = new QCheckBox(tr("Auto-clear after next login"), page);
    m_autoClearCheck->setChecked(true);
    m_autoClearCheck->setToolTip(
        tr("If checked, the message will be automatically removed\n"
           "after the user logs in once."));
    layout->addWidget(m_autoClearCheck);

    // Buttons
    QHBoxLayout* btnRow = new QHBoxLayout();
    m_loadMsgBtn  = new QPushButton(tr("Load Current"), page);
    m_saveMsgBtn  = new QPushButton(tr("Save"), page);
    m_clearMsgBtn = new QPushButton(tr("Clear"), page);
    m_saveMsgBtn->setFixedHeight(32);
    m_clearMsgBtn->setFixedHeight(32);
    m_loadMsgBtn->setFixedHeight(32);
    btnRow->addWidget(m_loadMsgBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_saveMsgBtn);
    btnRow->addWidget(m_clearMsgBtn);
    layout->addLayout(btnRow);

    m_msgStatusLabel = new QLabel("", page);
    m_msgStatusLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_msgStatusLabel);

    layout->addStretch();
    m_tabs->addTab(page, tr("Startup Message"));
}

void MainWindow::buildTray()
{
    // Use the ICO resource as the tray icon — ICO is natively supported on
    // Windows without requiring the imageformats plugin.
    QIcon trayIcon(":/assets/icon.ico");
    if (trayIcon.isNull()) {
        // Last resort: file next to exe
        trayIcon = QIcon(QCoreApplication::applicationDirPath() + "/icon.ico");
    }

    m_trayIcon = new QSystemTrayIcon(trayIcon, this);
    m_trayIcon->setToolTip(tr("Shutdown Timer"));

    m_trayMenu = new QMenu(this);
    m_trayShowHideAction = m_trayMenu->addAction(tr("Show / Hide"));
    m_trayCancelAction   = m_trayMenu->addAction(tr("Cancel Shutdown"));
    m_trayMenu->addSeparator();
    m_trayQuitAction = m_trayMenu->addAction(tr("Quit"));

    m_trayCancelAction->setEnabled(false);

    m_trayIcon->setContextMenu(m_trayMenu);
    // NOTE: show() is NOT called here — it must be called after the main
    // window has been shown (window.show() in main.cpp) otherwise
    // QSystemTrayIcon::show() silently fails on some Windows configurations.
}

void MainWindow::showTrayIcon()
{
    if (m_trayIcon)
        m_trayIcon->show();
}

// ── Connections ───────────────────────────────────────────────────────────────

void MainWindow::setupConnections()
{
    // Timer tab
    connect(m_startBtn,       &QPushButton::clicked,  this, &MainWindow::onStartClicked);
    connect(m_cancelBtn,      &QPushButton::clicked,  this, &MainWindow::onCancelClicked);
    connect(m_radioCountdown, &QRadioButton::toggled, this, &MainWindow::onModeToggled);
    connect(m_radioScheduled, &QRadioButton::toggled, this, &MainWindow::onModeToggled);

    // Preset buttons
    connect(m_preset15m, &QPushButton::clicked, this, [this]{ onPresetClicked(15 * 60); });
    connect(m_preset30m, &QPushButton::clicked, this, [this]{ onPresetClicked(30 * 60); });
    connect(m_preset1h,  &QPushButton::clicked, this, [this]{ onPresetClicked(60 * 60); });
    connect(m_preset2h,  &QPushButton::clicked, this, [this]{ onPresetClicked(120 * 60); });

    // Action radio buttons — disable Force when Hibernate or Sleep is selected
    connect(m_radioShutdown,  &QRadioButton::toggled, this, &MainWindow::onActionToggled);
    connect(m_radioRestart,   &QRadioButton::toggled, this, &MainWindow::onActionToggled);
    connect(m_radioHibernate, &QRadioButton::toggled, this, &MainWindow::onActionToggled);
    connect(m_radioSleep,     &QRadioButton::toggled, this, &MainWindow::onActionToggled);

    // Timer engine
    connect(m_timerEngine, &TimerEngine::tick,          this, &MainWindow::onTimerTick);
    connect(m_timerEngine, &TimerEngine::triggered,     this, &MainWindow::onTimerTriggered);
    connect(m_timerEngine, &TimerEngine::invalidTarget, this, &MainWindow::onTimerInvalidTarget);

    // Shutdown manager
    connect(m_shutdownMgr, &ShutdownManager::errorOccurred, this, &MainWindow::showError);

    // Message tab
    connect(m_saveMsgBtn,  &QPushButton::clicked, this, &MainWindow::onSaveMessageClicked);
    connect(m_clearMsgBtn, &QPushButton::clicked, this, &MainWindow::onClearMessageClicked);
    connect(m_loadMsgBtn,  &QPushButton::clicked, this, &MainWindow::onLoadMessageClicked);

    // Menu bar
    connect(m_exitAction,         &QAction::triggered, this, &MainWindow::onMenuExit);
    connect(m_aboutAction,        &QAction::triggered, this, &MainWindow::onMenuAbout);
    connect(m_viewReleasesAction, &QAction::triggered, this, &MainWindow::onMenuViewReleases);
    connect(m_langActionGroup,    &QActionGroup::triggered,
            this, &MainWindow::onMenuLanguageTriggered);

    // Tray
    connect(m_trayIcon,           &QSystemTrayIcon::activated,
            this, &MainWindow::onTrayIconActivated);
    connect(m_trayShowHideAction, &QAction::triggered, this, &MainWindow::onTrayShowHide);
    connect(m_trayQuitAction,     &QAction::triggered, this, &MainWindow::onTrayQuit);
    connect(m_trayCancelAction,   &QAction::triggered, this, &MainWindow::onCancelClicked);

    // Language
    connect(m_langMgr, &LanguageManager::languageChanged,
            this, &MainWindow::onLanguageApplied);
}

// ── Timer tab slots ───────────────────────────────────────────────────────────

void MainWindow::onPresetClicked(int seconds)
{
    m_countdownEdit->setTime(QTime(seconds / 3600, (seconds % 3600) / 60, seconds % 60));
}

void MainWindow::onModeToggled()
{
    m_inputStack->setCurrentIndex(m_radioScheduled->isChecked() ? 1 : 0);
}

void MainWindow::onActionToggled()
{
    // Force-close is only meaningful for Shutdown and Restart.
    // Hibernate and Sleep always suspend all activity immediately regardless
    // of open apps — so the option doesn't apply. Disable and uncheck it
    // to avoid confusion.
    bool isSuspend = m_radioHibernate->isChecked() || m_radioSleep->isChecked();
    m_forceCheck->setEnabled(!isSuspend);
    if (isSuspend)
        m_forceCheck->setChecked(false);
}

void MainWindow::onStartClicked()
{
    if (m_radioCountdown->isChecked()) {
        QTime t = m_countdownEdit->time();
        int totalSeconds = t.hour() * 3600 + t.minute() * 60 + t.second();
        if (totalSeconds <= 0) {
            showError(tr("\nPlease set a time greater than zero."));
            return;
        }
        m_timerEngine->startCountdown(totalSeconds);
    } else {
        QDateTime target = m_scheduledEdit->dateTime();
        m_timerEngine->startScheduled(target);
    }

    setTimerRunning(true);
    m_statusLabel->setText(tr("Timer started."));
}

void MainWindow::onCancelClicked()
{
    m_timerEngine->stop();
    m_shutdownMgr->cancelShutdown();
    setTimerRunning(false);
    m_countdownLabel->setText("--:--:--");
    m_statusLabel->setText(tr("Cancelled."));
    m_trayCancelAction->setEnabled(false);
}

void MainWindow::onTimerTick(int remainingSeconds)
{
    updateCountdownDisplay(remainingSeconds);
    m_trayIcon->setToolTip(
        tr("Shutdown Timer — %1").arg(formatDuration(remainingSeconds)));
}

void MainWindow::onTimerTriggered()
{
    ShutdownAction action = ShutdownAction::Shutdown;
    if (m_radioRestart->isChecked())   action = ShutdownAction::Restart;
    if (m_radioHibernate->isChecked()) action = ShutdownAction::Hibernate;
    if (m_radioSleep->isChecked())     action = ShutdownAction::Sleep;

    bool force = m_forceCheck->isChecked(); // always false for Hibernate/Sleep (checkbox disabled)

    bool ok = m_shutdownMgr->scheduleShutdown(action, 0, force);
    if (ok) {
        if (action == ShutdownAction::Shutdown)
            m_statusLabel->setText(tr("Shutting down..."));
        else if (action == ShutdownAction::Restart)
            m_statusLabel->setText(tr("Restarting..."));
        else if (action == ShutdownAction::Hibernate)
            m_statusLabel->setText(tr("Hibernating..."));
        else
            m_statusLabel->setText(tr("Sleeping..."));
    }
    setTimerRunning(false);
}

void MainWindow::onTimerInvalidTarget()
{
    showError(tr("The scheduled time is in the past. Please choose a future time."));
    setTimerRunning(false);
}

// ── Message tab slots ─────────────────────────────────────────────────────────

void MainWindow::onSaveMessageClicked()
{
    StartupMessage msg;
    msg.title = m_msgTitleEdit->text().trimmed();
    msg.body  = m_msgBodyEdit->toPlainText().trimmed();

    // Enforce a safe length limit on the body — LegalNoticeText is rendered
    // by winlogon.exe at the login screen. Extremely long strings can cause
    // display issues. 500 characters is generous for any legitimate notice.
    if (msg.body.length() > 500) {
        showError(tr("Message body must not exceed 500 characters."));
        return;
    }

    if (msg.title.isEmpty() && msg.body.isEmpty()) {
        showError(tr("Please enter a title or message body before saving."));
        return;
    }

    bool ok = m_registryMgr->writeStartupMessage(msg);
    if (!ok) {
        showError(tr("Failed to save message:\n%1").arg(m_registryMgr->lastError()));
        return;
    }

    if (m_autoClearCheck->isChecked()) {
        ok = m_taskScheduler->createAutoClearTask();
        if (!ok) {
            m_msgStatusLabel->setText(
                tr("Message saved, but auto-clear task failed: %1")
                .arg(m_taskScheduler->lastError()));
            return;
        }
        m_msgStatusLabel->setText(tr("Message saved. Will auto-clear after next login."));
    } else {
        m_taskScheduler->deleteAutoClearTask();
        m_msgStatusLabel->setText(tr("Message saved (persistent)."));
    }
}

void MainWindow::onClearMessageClicked()
{
    auto reply = QMessageBox::question(
        this, tr("Clear Startup Message"),
        tr("Are you sure you want to clear the startup message?"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    bool ok = m_registryMgr->clearStartupMessage();
    if (!ok) {
        showError(tr("Failed to clear message:\n%1").arg(m_registryMgr->lastError()));
        return;
    }

    m_taskScheduler->deleteAutoClearTask();
    m_msgTitleEdit->clear();
    m_msgBodyEdit->clear();
    m_msgStatusLabel->setText(tr("Startup message cleared."));
}

void MainWindow::onLoadMessageClicked()
{
    StartupMessage msg;
    if (!m_registryMgr->readStartupMessage(msg)) {
        showError(tr("Failed to read registry:\n%1").arg(m_registryMgr->lastError()));
        return;
    }
    m_msgTitleEdit->setText(msg.title);
    m_msgBodyEdit->setPlainText(msg.body);
    m_autoClearCheck->setChecked(m_taskScheduler->autoClearTaskExists());

    if (msg.title.isEmpty() && msg.body.isEmpty()) {
        m_msgStatusLabel->setText(tr("No startup message currently set."));
    } else {
        m_msgStatusLabel->setText(tr("Loaded current startup message."));
    }
}

// ── Menu bar slots ────────────────────────────────────────────────────────────

void MainWindow::onMenuExit()
{
    // If a timer is running, warn the user that exiting will cancel the pending
    // OS shutdown — they may not have intended that.
    if (m_timerEngine->isRunning()) {
        auto reply = QMessageBox::question(
            this, tr("Exit"),
            tr("A shutdown timer is running. Exit anyway?\n"
               "This will cancel the pending shutdown."),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    // Stop the countdown and abort any OS-level shutdown already initiated
    m_timerEngine->stop();
    if (m_shutdownMgr->isPending())
        m_shutdownMgr->cancelShutdown();

    // Hide the tray icon before quitting — a visible QSystemTrayIcon can
    // keep the process alive on some Windows/Qt configurations.
    if (m_trayIcon)
        m_trayIcon->hide();

    // Close the process completely
    m_forceQuit = true;
    qApp->quit();
}

void MainWindow::onMenuAbout()
{
    // QMessageBox::about() only supports a single OK button, so we build the
    // dialog manually to add a "View License" button alongside it.
    QMessageBox box(this);
    box.setWindowTitle(tr("About Shutdown Timer"));
    box.setText(
        tr("Shutdown Timer — version 1.0.0\n\n"
           "A Windows utility for scheduling system shutdowns and restarts, "
           "and setting a message on the login screen before users log in.\n\n"
           "© 2026 Shutdown Timer\nmolnupiravir-faky"));
    box.setIconPixmap(
        QPixmap(":/assets/shutdown64px.png"));

    QPushButton* licenseBtn = box.addButton(tr("View License"), QMessageBox::HelpRole);
    box.addButton(QMessageBox::Ok);
    box.setDefaultButton(QMessageBox::Ok);

    box.exec();

    // HelpRole buttons don't close the dialog automatically — check after exec()
    if (box.clickedButton() == licenseBtn) {
        // Look for LICENSE.txt next to the exe (installed scenario) or in the
        // current working directory (development/build scenario).
        QString licensePath = QCoreApplication::applicationDirPath() + "/LICENSE.txt";
        if (!QFile::exists(licensePath))
            licensePath = QDir::currentPath() + "/LICENSE.txt";

        if (QFile::exists(licensePath)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(licensePath));
        } else {
            QMessageBox::information(this, tr("License"),
                tr("LICENSE.txt was not found.\n"
                   "You can read the full GNU GPL v3 at:\n"
                   "https://www.gnu.org/licenses/gpl-3.0.html"));
        }
    }
}

void MainWindow::onMenuViewReleases()
{
    QDesktopServices::openUrl(
        QUrl("https://github.com/molnupiravir-faky/Shutdown_timer/releases"));
}

void MainWindow::onMenuLanguageTriggered(QAction* action)
{
    AppLanguage lang = AppLanguage::English;
    if (action == m_langArAction)   lang = AppLanguage::Arabic;
    if (action == m_langKoAction)   lang = AppLanguage::Korean;
    if (action == m_langEsAction)   lang = AppLanguage::Spanish;
    if (action == m_langFrAction)   lang = AppLanguage::French;
    if (action == m_langDeAction)   lang = AppLanguage::German;
    if (action == m_langPtBRAction) lang = AppLanguage::PortugueseBR;
    if (action == m_langZhCNAction) lang = AppLanguage::ChineseSimplified;
    if (action == m_langJaAction)   lang = AppLanguage::Japanese;

    m_langMgr->applyLanguage(lang);

    QSettings settings("ShutdownTimer", "ShutdownTimer");
    settings.setValue("language", LanguageManager::toCode(lang));
}

// ── Tray slots ────────────────────────────────────────────────────────────────

void MainWindow::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    // Trigger on either single left-click or double-click — Windows convention
    // is that left-clicking a tray icon shows/hides the window.
    if (reason == QSystemTrayIcon::Trigger ||
        reason == QSystemTrayIcon::DoubleClick)
        onTrayShowHide();
}

void MainWindow::onTrayShowHide()
{
    if (isVisible()) {
        hide();
    } else {
        show();
        raise();
        activateWindow();
    }
}

void MainWindow::onTrayQuit()
{
    // Tray Quit = actually close the process.
    // Warn only if a timer is running since OS shutdown would still proceed.
    if (m_timerEngine->isRunning()) {
        auto reply = QMessageBox::question(
            this, tr("Quit"),
            tr("A shutdown timer is running. Quit anyway?\n"
               "(The OS shutdown will still proceed if already initiated.)"),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }
    if (m_trayIcon)
        m_trayIcon->hide();

    m_forceQuit = true;
    qApp->quit();
}

// ── Language slot ─────────────────────────────────────────────────────────────

void MainWindow::onLanguageApplied(AppLanguage /*lang*/)
{
    retranslateUI();
}

// ── Close / minimize ──────────────────────────────────────────────────────────

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!m_forceQuit) {
        // Save geometry before hiding so it's restored on next show
        QSettings settings("ShutdownTimer", "ShutdownTimer");
        settings.setValue("windowGeometry", saveGeometry());

        hide();
        m_trayIcon->showMessage(
            tr("Shutdown Timer"),
            tr("Running in the system tray. Double-click the icon to restore."),
            QSystemTrayIcon::Information, 3000);
        event->ignore();
    } else {
        // Save geometry on real exit too
        QSettings settings("ShutdownTimer", "ShutdownTimer");
        settings.setValue("windowGeometry", saveGeometry());
        event->accept();
    }
}

void MainWindow::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::WindowStateChange) {
        if (isMinimized())
            QTimer::singleShot(0, this, &QWidget::hide);
    } else if (event->type() == QEvent::LanguageChange) {
        retranslateUI();
    }
    QMainWindow::changeEvent(event);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void MainWindow::updateCountdownDisplay(int seconds)
{
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    m_countdownLabel->setText(
        QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0')));
}

void MainWindow::setTimerRunning(bool running)
{
    m_startBtn->setEnabled(!running);
    m_cancelBtn->setEnabled(running);
    m_radioCountdown->setEnabled(!running);
    m_radioScheduled->setEnabled(!running);
    m_countdownEdit->setEnabled(!running);
    m_preset15m->setEnabled(!running);
    m_preset30m->setEnabled(!running);
    m_preset1h->setEnabled(!running);
    m_preset2h->setEnabled(!running);
    m_scheduledEdit->setEnabled(!running);
    m_radioShutdown->setEnabled(!running);
    m_radioRestart->setEnabled(!running);
    // Only re-enable Hibernate/Sleep radios if they were available at startup
    if (ShutdownManager::isHibernateAvailable())
        m_radioHibernate->setEnabled(!running);
    if (ShutdownManager::isSleepAvailable())
        m_radioSleep->setEnabled(!running);
    // Force only re-enables if not running AND a suspend action isn't selected
    bool isSuspend = m_radioHibernate->isChecked() || m_radioSleep->isChecked();
    m_forceCheck->setEnabled(!running && !isSuspend);
    m_trayCancelAction->setEnabled(running);
}

QString MainWindow::formatDuration(int seconds) const
{
    int h = seconds / 3600;
    int m = (seconds % 3600) / 60;
    int s = seconds % 60;
    if (h > 0) return QString("%1h %2m %3s").arg(h).arg(m).arg(s);
    if (m > 0) return QString("%1m %2s").arg(m).arg(s);
    return QString("%1s").arg(s);
}

void MainWindow::showError(const QString& msg)
{
    QMessageBox::critical(this, tr("Error"), msg);
}


void MainWindow::retranslateUI()
{
    setWindowTitle(tr("Shutdown Timer"));

    // ── Menu bar ──────────────────────────────────────────────────────────────
    if (m_settingsMenu)       m_settingsMenu->setTitle(tr("Settings"));
    if (m_langSubMenu)        m_langSubMenu->setTitle(tr("Language"));
    if (m_exitAction)         m_exitAction->setText(tr("Exit"));
    if (m_helpMenu)           m_helpMenu->setTitle(tr("Help"));
    if (m_aboutAction)        m_aboutAction->setText(tr("About"));
    if (m_viewReleasesAction) m_viewReleasesAction->setText(tr("View Releases"));

    // Language action labels stay in their own script — not translated

    // ── Tab titles ────────────────────────────────────────────────────────────
    if (m_tabs && m_tabs->count() >= 2) {
        m_tabs->setTabText(0, tr("Timer"));
        m_tabs->setTabText(1, tr("Startup Message"));
    }

    // ── Tray ──────────────────────────────────────────────────────────────────
    if (m_trayShowHideAction) m_trayShowHideAction->setText(tr("Show / Hide"));
    if (m_trayCancelAction)   m_trayCancelAction->setText(tr("Cancel Shutdown"));
    if (m_trayQuitAction)     m_trayQuitAction->setText(tr("Quit"));
    if (m_trayIcon)           m_trayIcon->setToolTip(tr("Shutdown Timer"));

    // ── Timer tab ─────────────────────────────────────────────────────────────
    if (m_modeGroup)      m_modeGroup->setTitle(tr("Timer Mode"));
    if (m_radioCountdown) m_radioCountdown->setText(tr("Countdown"));
    if (m_radioScheduled) m_radioScheduled->setText(tr("Scheduled Time"));
    if (m_actionGroup)    m_actionGroup->setTitle(tr("Action"));
    if (m_radioShutdown)  m_radioShutdown->setText(tr("Shutdown"));
    if (m_radioRestart)   m_radioRestart->setText(tr("Restart"));
    if (m_radioHibernate) m_radioHibernate->setText(tr("Hibernate"));
    if (m_radioSleep)     m_radioSleep->setText(tr("Sleep"));
    if (m_forceCheck)     m_forceCheck->setText(tr("Force (don't wait for apps to close)"));
    if (m_startBtn)       m_startBtn->setText(tr("Start"));
    if (m_cancelBtn)      m_cancelBtn->setText(tr("Cancel"));


    // ── Message tab ───────────────────────────────────────────────────────────
    if (m_msgInfoLabel)
        m_msgInfoLabel->setText(tr("This message will be shown on the Windows login screen before the user logs in."));
    if (m_msgTitleLabel)  m_msgTitleLabel->setText(tr("Title:"));
    if (m_msgBodyLabel)   m_msgBodyLabel->setText(tr("Message:"));
    if (m_msgTitleEdit)   m_msgTitleEdit->setPlaceholderText(tr("e.g. Important Notice"));
    if (m_msgBodyEdit)    m_msgBodyEdit->setPlaceholderText(tr("Enter your message here... (max 500 characters)"));
    if (m_autoClearCheck) {
        m_autoClearCheck->setText(tr("Auto-clear after next login"));
        m_autoClearCheck->setToolTip(
            tr("If checked, the message will be automatically removed\n"
               "after the user logs in once."));
    }
    if (m_saveMsgBtn)  m_saveMsgBtn->setText(tr("Save"));
    if (m_clearMsgBtn) m_clearMsgBtn->setText(tr("Clear"));
    if (m_loadMsgBtn)  m_loadMsgBtn->setText(tr("Load Current"));
}
