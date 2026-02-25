#pragma once

#include <QObject>
#include <QString>
#include <windows.h>

struct StartupMessage {
    QString title;
    QString body;
};

class RegistryManager : public QObject
{
    Q_OBJECT
public:
    explicit RegistryManager(QObject* parent = nullptr);

    // Read current LegalNotice values from HKLM\Winlogon
    bool readStartupMessage(StartupMessage& out);

    // Write title + body to HKLM\Winlogon
    bool writeStartupMessage(const StartupMessage& msg);

    // Clear both values (set to empty strings)
    bool clearStartupMessage();

    QString lastError() const { return m_lastError; }

private:
    bool openKey(HKEY& outKey, REGSAM access);

    QString m_lastError;

    // Registry path and value names
    static constexpr wchar_t kRegPath[]  =
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon";
    static constexpr wchar_t kRegTitle[] = L"LegalNoticeCaption";
    static constexpr wchar_t kRegBody[]  = L"LegalNoticeText";
};
