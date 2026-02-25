#include "LanguageManager.h"
#include <QApplication>
#include <QDir>
#include <QFile>

LanguageManager::LanguageManager(QObject* parent)
    : QObject(parent)
{}

void LanguageManager::applyLanguage(AppLanguage lang)
{
    m_current = lang;

    // Remove previous translator
    qApp->removeTranslator(&m_translator);

    QString code = toCode(lang);
    QString qmPath = QDir::currentPath() + "/i18n/app_" + code + ".qm";

    // Also try next to the exe (installed scenario)
    if (!QFile::exists(qmPath)) {
        qmPath = QCoreApplication::applicationDirPath() + "/i18n/app_" + code + ".qm";
    }

    if (m_translator.load(qmPath)) {
        qApp->installTranslator(&m_translator);
    }

    // Apply RTL/LTR layout direction
    if (lang == AppLanguage::Arabic) {
        qApp->setLayoutDirection(Qt::RightToLeft);
    } else {
        qApp->setLayoutDirection(Qt::LeftToRight);
    }

    emit languageChanged(lang);
}

QString LanguageManager::languageDisplayName(AppLanguage lang)
{
    switch (lang) {
        case AppLanguage::English:          return QStringLiteral("English");
        case AppLanguage::Arabic:           return QStringLiteral("العربية");
        case AppLanguage::Korean:           return QStringLiteral("한국어");
        case AppLanguage::Spanish:          return QStringLiteral("Español");
        case AppLanguage::French:           return QStringLiteral("Français");
        case AppLanguage::German:           return QStringLiteral("Deutsch");
        case AppLanguage::PortugueseBR:     return QStringLiteral("Português (Brasil)");
        case AppLanguage::ChineseSimplified:return QStringLiteral("中文（简体）");
        case AppLanguage::Japanese:         return QStringLiteral("日本語");
    }
    return {};
}

AppLanguage LanguageManager::fromCode(const QString& code)
{
    if (code == "ar")    return AppLanguage::Arabic;
    if (code == "ko")    return AppLanguage::Korean;
    if (code == "es")    return AppLanguage::Spanish;
    if (code == "fr")    return AppLanguage::French;
    if (code == "de")    return AppLanguage::German;
    if (code == "pt_BR") return AppLanguage::PortugueseBR;
    if (code == "zh_CN") return AppLanguage::ChineseSimplified;
    if (code == "ja")    return AppLanguage::Japanese;
    return AppLanguage::English;
}

QString LanguageManager::toCode(AppLanguage lang)
{
    switch (lang) {
        case AppLanguage::Arabic:           return QStringLiteral("ar");
        case AppLanguage::Korean:           return QStringLiteral("ko");
        case AppLanguage::Spanish:          return QStringLiteral("es");
        case AppLanguage::French:           return QStringLiteral("fr");
        case AppLanguage::German:           return QStringLiteral("de");
        case AppLanguage::PortugueseBR:     return QStringLiteral("pt_BR");
        case AppLanguage::ChineseSimplified:return QStringLiteral("zh_CN");
        case AppLanguage::Japanese:         return QStringLiteral("ja");
        default:                            return QStringLiteral("en");
    }
}
