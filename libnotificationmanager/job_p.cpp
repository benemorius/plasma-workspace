/*
 * Copyright 2019 Kai Uwe Broulik <kde@privat.broulik.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) version 3, or any
 * later version accepted by the membership of KDE e.V. (or its
 * successor approved by the membership of KDE e.V.), which shall
 * act as a proxy defined in Section 6 of version 3 of the license.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "job_p.h"
#include "job.h"

#include "debug.h"

#include <QDebug>
#include <QDBusConnection>

#include <KFilePlacesModel>
#include <KService>
#include <KLocalizedString>

#include <kio/global.h>

#include "notifications.h"

#include "jobviewv2adaptor.h"
#include "jobviewv3adaptor.h"

#include <QDebug>

using namespace NotificationManager;

JobPrivate::JobPrivate(uint id, QObject *parent)
    : QObject(parent)
    , m_id(id)
    , m_placesModel(createPlacesModel())
{
    m_objectPath.setPath(QStringLiteral("/org/kde/notificationmanager/jobs/JobView_%1").arg(id));

    // TODO also v1? it's identical to V2 except it doesn't have setError method so supporting it should be easy
    new JobViewV2Adaptor(this);
    new JobViewV3Adaptor(this);

    QDBusConnection::sessionBus().registerObject(m_objectPath.path(), this);
}

JobPrivate::~JobPrivate() = default;

QDBusObjectPath JobPrivate::objectPath() const
{
    return m_objectPath;
}

QSharedPointer<KFilePlacesModel> JobPrivate::createPlacesModel()
{
    static QWeakPointer<KFilePlacesModel> s_instance;
    if (!s_instance) {
        QSharedPointer<KFilePlacesModel> ptr(new KFilePlacesModel());
        s_instance = ptr.toWeakRef();
        return ptr;
    }
    return s_instance.toStrongRef();
}

QUrl JobPrivate::localFileOrUrl(const QString &urlString)
{
    QUrl url(urlString);
    if (url.scheme().isEmpty()) {
        url = QUrl::fromLocalFile(urlString);
    }
    return url;
}

// Tries to return a more user-friendly displayed destination
// - if it is a place, show the name, e.g. "Downloads"
// - if it is inside home, abbreviate that to tilde ~/foo
// - otherwise print URL (without password)
QString JobPrivate::prettyDestUrl() const
{
    QUrl url = m_destUrl;
    // In case of a single file and no destUrl, try using the second label (most likely "Destination")...
    if (!url.isValid() && m_totalFiles == 1) {
        url = localFileOrUrl(m_descriptionValue2).adjusted(QUrl::RemoveFilename);
    }

    if (!url.isValid()) {
        return QString();
    }

    if (!m_placesModel) {
        m_placesModel = createPlacesModel();
    }

    // If we copy into a "place", show its pretty name instead of a URL/path
    for (int row = 0; row < m_placesModel->rowCount(); ++row) {
        const QModelIndex idx = m_placesModel->index(row, 0);
        if (m_placesModel->isHidden(idx)) {
            continue;
        }

        if (m_placesModel->url(idx).matches(url, QUrl::StripTrailingSlash)) {
            return m_placesModel->text(idx);
        }
    }

    if (url.isLocalFile()) {
        QString destUrlString = url.toLocalFile();

        const QString homePath = QDir::homePath();
        if (destUrlString.startsWith(homePath)) {
            destUrlString = QLatin1String("~") + destUrlString.mid(homePath.length());
        }

        return destUrlString;
    }

    return url.toDisplayString(QUrl::RemoveUserInfo);
}

void JobPrivate::updateHasDetails()
{
    const bool hasDetails = m_totalBytes > 0
        || m_totalFiles > 0
        || m_totalDirectories > 0
        || m_processedBytes > 0
        || m_processedFiles > 0
        || m_processedDirectories > 0
        || !m_descriptionLabel1.isEmpty()
        || !m_descriptionLabel2.isEmpty()
        || !m_descriptionValue1.isEmpty()
        || !m_descriptionValue2.isEmpty()
        || m_speed > 0;

    if (m_hasDetails != hasDetails) {
        m_hasDetails = hasDetails;
        emit static_cast<Job *>(parent())->hasDetailsChanged();
    }
}

QString JobPrivate::text() const
{
    if (!m_errorText.isEmpty()) {
        return m_errorText;
    }

    if (!m_infoMessage.isEmpty()) {
        return m_infoMessage;
    }

    const QString currentFileName = descriptionUrl().fileName();
    const QString destUrlString = prettyDestUrl();

    if (m_totalFiles == 0) {
        if (!destUrlString.isEmpty()) {
            if (m_processedFiles > 0) {
                return i18ncp("Copying n files to location", "%1 file to %2", "%1 files to %2",
                              m_processedFiles, destUrlString);
            }
            return i18nc("Copying unknown amount of files to location", "to %1", destUrlString);
        } else if (m_processedFiles > 0) {
            return i18ncp("Copying n files", "%1 file", "%1 files", m_processedFiles);
        }
    } else if (m_totalFiles == 1 && !currentFileName.isEmpty()) {
        if (!destUrlString.isEmpty()) {
            return i18nc("Copying file to location", "%1 to %2", currentFileName, destUrlString);
        }

        return currentFileName;
    } else if (m_totalFiles > 1) {
        if (!destUrlString.isEmpty()) {
            if (m_processedFiles > 0 && m_processedFiles <= m_totalFiles) {
                return i18ncp("Copying n of m files to locaton", "%2 of %1 file to %3", "%2 of %1 files to %3",
                              m_totalFiles, m_processedFiles, destUrlString);
            }
            return i18ncp("Copying n files to location", "%1 file to %2", "%1 files to %2",
                          m_processedFiles > 0 ? m_processedFiles : m_totalFiles, destUrlString);
        }

        if (m_processedFiles > 0 && m_processedFiles <= m_totalFiles) {
            return i18ncp("Copying n of m files", "%2 of %1 file", "%2 of %1 files",
                          m_totalFiles, m_processedFiles);
        }

        return i18ncp("Copying n files", "%1 file", "%1 files", m_processedFiles > 0 ? m_processedFiles : m_totalFiles);
    }

    qCInfo(NOTIFICATIONMANAGER) << "Failed to generate job text for job with following properties:";
    qCInfo(NOTIFICATIONMANAGER) << "  processedFiles =" << m_processedFiles << ", totalFiles =" << m_totalFiles
                                << ", current file name =" << currentFileName << ", destination url string =" << destUrlString;
    qCInfo(NOTIFICATIONMANAGER) << "label1 =" << m_descriptionLabel1 << ", value1 =" << m_descriptionValue1
                                << ", label2 =" << m_descriptionLabel2 << ", value2 =" << m_descriptionValue2;

    return QString();
}

QUrl JobPrivate::descriptionUrl() const
{
    QUrl url = localFileOrUrl(m_descriptionValue2);
    if (!url.isValid()) {
        url = localFileOrUrl(m_descriptionValue1);
    }
    return url;
}

void JobPrivate::finish()
{
    // Unregister the dbus service since the client is done with it
    QDBusConnection::sessionBus().unregisterObject(m_objectPath.path());

    // When user canceled transfer, remove it without notice
    if (m_error == KIO::ERR_USER_CANCELED) {
        emit closed();
        return;
    }

    Job *job = static_cast<Job *>(parent());
    // update timestamp
    job->resetUpdated();
    // when it was hidden in history, bring it up again
    job->setDismissed(false);
}

// JobViewV2
void JobPrivate::terminate(const QString &errorMessage)
{
    Job *job = static_cast<Job *>(parent());
    job->setErrorText(errorMessage);
    job->setState(Notifications::JobStateStopped);
    finish();
}

void JobPrivate::setSuspended(bool suspended)
{
    Job *job = static_cast<Job *>(parent());
    if (suspended) {
        job->setState(Notifications::JobStateSuspended);
    } else {
        job->setState(Notifications::JobStateRunning);
    }
}

void JobPrivate::setTotalAmount(quint64 amount, const QString &unit)
{
    if (unit == QLatin1String("bytes")) {
        updateField(amount, m_totalBytes, &Job::totalBytesChanged);
    } else if (unit == QLatin1String("files")) {
        updateField(amount, m_totalFiles, &Job::totalFilesChanged);
    } else if (unit == QLatin1String("dirs")) {
        updateField(amount, m_totalDirectories, &Job::totalDirectoriesChanged);
    }
    updateHasDetails();
}

void JobPrivate::setProcessedAmount(quint64 amount, const QString &unit)
{
    if (unit == QLatin1String("bytes")) {
        updateField(amount, m_processedBytes, &Job::processedBytesChanged);
    } else if (unit == QLatin1String("files")) {
        updateField(amount, m_processedFiles, &Job::processedFilesChanged);
    } else if (unit == QLatin1String("dirs")) {
        updateField(amount, m_processedDirectories, &Job::processedDirectoriesChanged);
    }
    updateHasDetails();
}

void JobPrivate::setPercent(uint percent)
{
    const int percentage = static_cast<int>(percent);
    if (m_percentage != percentage) {
        m_percentage = percentage;
        emit static_cast<Job *>(parent())->percentageChanged(percentage);
    }
}

void JobPrivate::setSpeed(quint64 bytesPerSecond)
{
    updateField(bytesPerSecond, m_speed, &Job::speedChanged);
    updateHasDetails();
}

// NOTE infoMessage isn't supposed to be the "Copying..." heading but e.g. a "Connecting to server..." status message
// JobViewV1/V2 got that wrong but JobView3 uses "title" and "infoMessage" correctly respectively.
void JobPrivate::setInfoMessage(const QString &infoMessage)
{
    updateField(infoMessage, m_summary, &Job::summaryChanged);
}

bool JobPrivate::setDescriptionField(uint number, const QString &name, const QString &value)
{
    bool dirty = false;
    if (number == 0) {
        dirty |= updateField(name, m_descriptionLabel1, &Job::descriptionLabel1Changed);
        dirty |= updateField(value, m_descriptionValue1, &Job::descriptionValue1Changed);
    } else if (number == 1) {
        dirty |= updateField(name, m_descriptionLabel2, &Job::descriptionLabel2Changed);
        dirty |= updateField(value, m_descriptionValue2, &Job::descriptionValue2Changed);
    }
    if (dirty) {
        emit static_cast<Job*>(parent())->descriptionUrlChanged();
        updateHasDetails();
    }

    return false;
}

void JobPrivate::clearDescriptionField(uint number)
{
    setDescriptionField(number, QString(), QString());
}

void JobPrivate::setDestUrl(const QDBusVariant &urlVariant)
{
    const QUrl destUrl = QUrl(urlVariant.variant().toUrl().adjusted(QUrl::StripTrailingSlash)); // urgh
    updateField(destUrl, m_destUrl, &Job::destUrlChanged);
}

void JobPrivate::setError(uint errorCode)
{
    static_cast<Job *>(parent())->setError(errorCode);
}

// JobViewV3
void JobPrivate::terminate(uint errorCode, const QString &errorMessage, const QVariantMap &hints)
{
    Q_UNUSED(hints) // reserved for future extension

    Job *job = static_cast<Job *>(parent());
    job->setError(errorCode);
    job->setErrorText(errorMessage);
    job->setState(Notifications::JobStateStopped);
    finish();
}

void JobPrivate::update(const QVariantMap &properties)
{
    auto end = properties.end();

    auto it = properties.find(QStringLiteral("title"));
    if (it != end) {
        updateField(it->toString(), m_summary, &Job::summaryChanged);
    }

    it = properties.find(QStringLiteral("infoMessage"));
    if (it != end) {
        // InfoMessage is exposed via text()/BodyRole, not via public API, hence no public signal
        const QString infoMessage = it->toString();
        if (m_infoMessage != infoMessage) {
            m_infoMessage = it->toString();
            emit infoMessageChanged();
        }
    }

    it = properties.find(QStringLiteral("percent"));
    if (it != end) {
        setPercent(it->toUInt());
    }

    it = properties.find(QStringLiteral("destUrl"));
    if (it != end) {
        const QUrl destUrl = QUrl(it->toUrl().adjusted(QUrl::StripTrailingSlash)); // urgh
        updateField(destUrl, m_destUrl, &Job::destUrlChanged);
    }

    it = properties.find(QStringLiteral("speed"));
    if (it != end) {
        setSpeed(it->value<qulonglong>());
    }

    updateFieldFromProperties(properties, QStringLiteral("processedFiles"), m_processedFiles, &Job::processedFilesChanged);
    updateFieldFromProperties(properties, QStringLiteral("processedBytes"), m_processedBytes, &Job::processedBytesChanged);
    updateFieldFromProperties(properties, QStringLiteral("processedDirectories"), m_processedDirectories, &Job::processedDirectoriesChanged);

    updateFieldFromProperties(properties, QStringLiteral("totalFiles"), m_totalFiles, &Job::totalFilesChanged);
    updateFieldFromProperties(properties, QStringLiteral("totalBytes"), m_totalBytes, &Job::totalBytesChanged);
    updateFieldFromProperties(properties, QStringLiteral("totalDirectories"), m_totalDirectories, &Job::totalDirectoriesChanged);

    updateFieldFromProperties(properties, QStringLiteral("descriptionLabel1"), m_descriptionLabel1, &Job::descriptionLabel1Changed);
    updateFieldFromProperties(properties, QStringLiteral("descriptionValue1"), m_descriptionValue1, &Job::descriptionValue1Changed);
    updateFieldFromProperties(properties, QStringLiteral("descriptionLabel2"), m_descriptionLabel2, &Job::descriptionLabel2Changed);
    updateFieldFromProperties(properties, QStringLiteral("descriptionValue2"), m_descriptionValue2, &Job::descriptionValue2Changed);

    it = properties.find(QStringLiteral("suspended"));
    if (it != end) {
        setSuspended(it->toBool());
    }

    updateHasDetails();
}
