/*
    SPDX-FileCopyrightText: 2015 Eike Hein <hein@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "fileentry.h"
#include "actionlist.h"

#include <KFileItem>
#include <KIO/OpenUrlJob>
#include <KIO/StatJob>

FileEntry::FileEntry(AbstractModel *owner, const QUrl &url, const QString &mimeType)
    : AbstractEntry(owner)
    , m_fileItem(nullptr)
{
    if (url.isValid()) {
        if (url.isLocalFile()) {
            m_fileItem = new KFileItem(url, mimeType);
            m_fileItem->determineMimeType();
        } else {
            KIO::StatJob *job = KIO::statDetails(url, KIO::StatJob::SourceSide, KIO::StatBasic, KIO::JobFlag::HideProgressInfo);
            if (job->exec()) {
                m_fileItem = new KFileItem(job->statResult(), url);
            }
        }
    }
}

FileEntry::~FileEntry()
{
    delete m_fileItem;
}

bool FileEntry::isValid() const
{
    return m_fileItem && (m_fileItem->isFile() || m_fileItem->isDir());
}

QIcon FileEntry::icon() const
{
    if (m_fileItem) {
        return QIcon::fromTheme(m_fileItem->iconName(), QIcon::fromTheme(QStringLiteral("unknown")));
    }

    return QIcon::fromTheme(QStringLiteral("unknown"));
}

QString FileEntry::name() const
{
    if (m_fileItem) {
        return m_fileItem->text();
    }

    return QString();
}

QString FileEntry::description() const
{
    if (m_fileItem) {
        return m_fileItem->url().toString(QUrl::PreferLocalFile);
    }

    return QString();
}

QString FileEntry::id() const
{
    if (m_fileItem) {
        return m_fileItem->url().toString();
    }

    return QString();
}

QUrl FileEntry::url() const
{
    if (m_fileItem) {
        return m_fileItem->url();
    }

    return QUrl();
}

bool FileEntry::hasActions() const
{
    return m_fileItem && m_fileItem->isFile();
}

QVariantList FileEntry::actions() const
{
    if (m_fileItem) {
        return Kicker::createActionListForFileItem(*m_fileItem);
    }

    return QVariantList();
}

bool FileEntry::run(const QString &actionId, const QVariant &argument)
{
    if (!m_fileItem) {
        return false;
    }

    if (actionId.isEmpty()) {
        auto job = new KIO::OpenUrlJob(m_fileItem->url());
        job->start();

        return true;
    } else {
        bool close = false;

        if (Kicker::handleFileItemAction(*m_fileItem, actionId, argument, &close)) {
            return close;
        }
    }

    return false;
}
