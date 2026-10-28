#include "vmcontroller.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QImage>
#include <QImageReader>
#include <QDateTime>
#include <QUrl>
#include <QProcess>
#include <QProcessEnvironment>
#include <QVector>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>
#include <QDebug>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusReply>
#include <QDBusVariant>
#include <QDBusArgument>
#include <QRegExp>
#include <QUuid>
#include <QVariantMap>
#include <cerrno>
#include <cstring>
#include <vector>
#include <spawn.h>
#include <fcntl.h>
#include <csignal>
#include <unistd.h>

// Launch QEMU without a shell: Sailjail's private-bin normally hides /bin/sh.
// POSIX spawn file actions preserve the QEMU PID and stderr log while avoiding
// fork() from a potentially multithreaded Qt/Wayland frontend.
static bool spawnQemu(const QString &program, const QStringList &arguments,
                      const QString &logPath, qint64 *pid, QString *error)
{
    if (pid) *pid = 0;
    const QByteArray binary = QFile::encodeName(program);
    const QByteArray log = QFile::encodeName(logPath);
    QVector<QByteArray> argvBytes;
    argvBytes.reserve(arguments.size() + 1);
    argvBytes.append(binary);
    for (const QString &argument : arguments) argvBytes.append(argument.toLocal8Bit());
    std::vector<char *> argv;
    argv.reserve(static_cast<size_t>(argvBytes.size()) + 1);
    for (QByteArray &value : argvBytes) argv.push_back(value.data());
    argv.push_back(nullptr);

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("SDL_VIDEODRIVER"), QStringLiteral("wayland"));
    const QStringList keys = environment.keys();
    QVector<QByteArray> envBytes;
    envBytes.reserve(keys.size());
    for (const QString &key : keys)
        envBytes.append((key + QLatin1Char('=') + environment.value(key)).toLocal8Bit());
    std::vector<char *> envp;
    envp.reserve(static_cast<size_t>(envBytes.size()) + 1);
    for (QByteArray &value : envBytes) envp.push_back(value.data());
    envp.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    int result = posix_spawn_file_actions_init(&actions);
    const bool actionsInitialised = (result == 0);
    if (!result) {
        result = posix_spawn_file_actions_addopen(&actions, STDOUT_FILENO,
                                                  log.constData(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (!result)
            result = posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);
    }
    pid_t child = -1;
    if (!result)
        result = posix_spawn(&child, binary.constData(), &actions, nullptr,
                             argv.data(), envp.data());
    if (actionsInitialised) posix_spawn_file_actions_destroy(&actions);
    if (result || child <= 0) {
        if (error)
            *error = QStringLiteral("Unable to execute QEMU directly: %1 (errno %2). "
                                    "Check Sailjail executable access and shared libraries.")
                         .arg(QString::fromLocal8Bit(std::strerror(result ? result : ECHILD)))
                         .arg(result ? result : ECHILD);
        return false;
    }
    if (pid) *pid = static_cast<qint64>(child);
    return true;
}

static QString expandHome(const QString &input)
{
    const QString path = input.trimmed();
    if (path == QLatin1String("~"))
        return QDir::homePath();
    if (path.startsWith(QLatin1String("~/")))
        return QDir::homePath() + path.mid(1);
    return path;
}

VmController::VmController(QObject *parent)
    : QObject(parent)
{
    const QString cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(cache);
    QFile::setPermissions(cache, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    // A single QMP socket is deliberate: M2 allows multiple saved profiles,
    // but only one running VM. Keep the M1 path for seamless reconnection.
    m_socketPath = cache + QStringLiteral("/qmp.sock");
    m_logPath = cache + QStringLiteral("/qemu.log");
    m_guestPreviewPpmPath = cache + QStringLiteral("/guest-preview.ppm");
    m_guestPreviewPngPath = cache + QStringLiteral("/guest-preview.png");
    // /tmp is a tmpfs on the G7 Power; use private, unique temporary storage
    // when it exists. Fallback to the established cache path if creation fails.
    const QString liveDir = m_liveFrameDir.isValid() ? m_liveFrameDir.path() : cache;
    m_livePngPath = liveDir + QStringLiteral("/dbus-live-frame.png");
    m_guestCursorPngPath = m_livePngPath + QStringLiteral(".cursor.png");
    m_guestCursorStatePath = m_livePngPath + QStringLiteral(".cursor-state");
    // Device feedback showed no perceived improvement from 75 ms polling,
    // with a possible Windows XP startup regression. Restore proven pacing;
    // keep unchanged-update suppression and measured frame-file diagnostics.
    m_liveTimer.setInterval(200);
    connect(&m_liveTimer, &QTimer::timeout, this, &VmController::updateLiveDisplay);
    connect(&m_liveHelper, &QProcess::readyReadStandardOutput, this, [this]() {
        m_liveRecentLog += QString::fromLocal8Bit(m_liveHelper.readAllStandardOutput());
        if (m_liveRecentLog.size() > 1024)
            m_liveRecentLog = m_liveRecentLog.right(1024);
    });
    m_liveHelper.setProcessChannelMode(QProcess::MergedChannels);
    m_liveTimer.start();
    m_keyTimer.setInterval(130);
    connect(&m_keyTimer, &QTimer::timeout, this, &VmController::dispatchNextKey);

    connect(&m_qmp, &QLocalSocket::readyRead, this, &VmController::readQmp);
    connect(&m_qmp, &QLocalSocket::disconnected, this, [this]() {
        m_connected = false;
        m_qmpNameConfirmed = false;
        m_qmpBuffer.clear();
        // Reconnection must re-authenticate before any more D-Bus input.
        setDbusStatus(QStringLiteral("QMP disconnected; re-verifying VM identity..."));
        resetMouse();
        clearGuestInput();
        emit stateChanged();
    });
    connect(&m_pollTimer, &QTimer::timeout, this, &VmController::poll);
    m_pollTimer.setInterval(1300);
    load();
    m_pollTimer.start();
    QTimer::singleShot(0, this, &VmController::poll);
}

QString VmController::configFile() const
{
    const QString folder = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir().mkpath(folder);
    return folder + QStringLiteral("/profiles.ini");
}

QString VmController::legacyFile() const
{
    return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation)
        + QStringLiteral("/vm.ini");
}

QString VmController::socketFile() const { return m_socketPath; }

QString VmController::newId() const
{
    // QUuid::WithoutBraces was introduced in Qt 5.11; Sailfish Qt is older.
    // toString() has always returned the canonical UUID enclosed in braces.
    QString id = QUuid::createUuid().toString();
    id.remove(QLatin1Char('{'));
    id.remove(QLatin1Char('}'));
    return id;
}

QString VmController::defaultQemuPath() const
{
    // New profiles use the independently packaged, working QEMU 10.2.4.
    // Existing saved profile paths are not modified.
    return QStringLiteral("/opt/sailqemu/qemu-10.2.4/bin/qemu-system-x86_64");
}

int VmController::profileIndex(const QString &id) const
{
    for (int i = 0; i < m_profiles.size(); ++i) {
        if (m_profiles.at(i).id == id)
            return i;
    }
    return -1;
}

bool VmController::nameAvailable(const QString &name, const QString &excludingId) const
{
    if (name.trimmed().isEmpty() || name.trimmed().size() > 80)
        return false;
    for (const VmProfile &profile : m_profiles) {
        if (profile.id != excludingId && profile.name.compare(name.trimmed(), Qt::CaseInsensitive) == 0)
            return false;
    }
    return true;
}

void VmController::copyActiveToProfile()
{
    const int index = profileIndex(m_activeId);
    if (index < 0) return;
    VmProfile &profile = m_profiles[index];
    profile.qemuPath = m_qemuPath;
    profile.diskPath = m_diskPath;
    profile.diskFormat = m_diskFormat;
    profile.secondaryDisks = m_secondaryDisks;
    profile.secondaryFormats = m_secondaryFormats;
    profile.isoFormat = m_isoFormat;
    profile.bootPrimary = m_bootPrimary;
    profile.bootSecondary = m_bootSecondary;
    profile.isoPath = m_isoPath;
    profile.floppyPath = m_floppyPath;
    profile.memoryMiB = m_memoryMiB;
    profile.cpuModel = m_cpuModel;
    profile.cpuCount = m_cpuCount;
    profile.videoAdapter = m_videoAdapter;
    profile.acpiEnabled = m_acpiEnabled;
    profile.usbTabletEnabled = m_usbTabletEnabled;
    profile.fullscreen = m_fullscreen;
    profile.isoBootFirst = m_isoBootFirst;
    profile.floppyBootFirst = m_floppyBootFirst;
    profile.networkEnabled = m_networkEnabled;
    profile.mouseOffsetX = m_mouseOffsetX;
    profile.mouseOffsetY = m_mouseOffsetY;
}

void VmController::loadActiveFields()
{
    const int index = profileIndex(m_activeId);
    if (index < 0) return;
    const VmProfile &profile = m_profiles.at(index);
    m_qemuPath = profile.qemuPath;
    m_diskPath = profile.diskPath;
    m_diskFormat = profile.diskFormat;
    m_secondaryDisks = profile.secondaryDisks;
    m_secondaryFormats = profile.secondaryFormats;
    m_isoFormat = profile.isoFormat;
    m_bootPrimary = profile.bootPrimary;
    m_bootSecondary = profile.bootSecondary;
    m_isoPath = profile.isoPath;
    m_floppyPath = profile.floppyPath;
    m_memoryMiB = profile.memoryMiB;
    m_cpuModel = profile.cpuModel;
    m_cpuCount = profile.cpuCount;
    m_videoAdapter = profile.videoAdapter;
    m_acpiEnabled = profile.acpiEnabled;
    m_usbTabletEnabled = profile.usbTabletEnabled;
    m_fullscreen = profile.fullscreen;
    m_isoBootFirst = profile.isoBootFirst;
    m_floppyBootFirst = profile.floppyBootFirst;
    m_networkEnabled = profile.networkEnabled;
    m_mouseOffsetX = profile.mouseOffsetX;
    m_mouseOffsetY = profile.mouseOffsetY;
    emit configChanged();
    emit profilesChanged();
}

QString VmController::activeProfileName() const
{
    const int index = profileIndex(m_activeId);
    return index >= 0 ? m_profiles.at(index).name : QString();
}

QVariantList VmController::profiles() const
{
    QVariantList result;
    for (const VmProfile &profile : m_profiles) {
        QVariantMap entry;
        entry.insert(QStringLiteral("id"), profile.id);
        entry.insert(QStringLiteral("name"), profile.name);
        entry.insert(QStringLiteral("disk"), profile.diskPath);
        entry.insert(QStringLiteral("ram"), profile.memoryMiB);
        entry.insert(QStringLiteral("selected"), profile.id == m_activeId);
        result.append(entry);
    }
    return result;
}

void VmController::migrateLegacy()
{
    VmProfile first;
    first.id = newId();
    first.name = QStringLiteral("Default VM");
    first.qemuPath = defaultQemuPath();

    if (QFile::exists(legacyFile())) {
        QSettings legacy(legacyFile(), QSettings::IniFormat);
        first.qemuPath = legacy.value(QStringLiteral("vm/qemuPath"), first.qemuPath).toString();
        first.diskPath = legacy.value(QStringLiteral("vm/diskPath")).toString();
        first.diskFormat = legacy.value(QStringLiteral("vm/diskFormat"), QStringLiteral("raw")).toString();
        first.isoPath = legacy.value(QStringLiteral("vm/isoPath")).toString();
        if (first.isoPath.endsWith(QLatin1String(".fd"), Qt::CaseInsensitive)
            || first.isoPath.endsWith(QLatin1String(".ima"), Qt::CaseInsensitive)) {
            first.floppyPath = first.isoPath;
            first.isoPath.clear();
        }
        first.memoryMiB = legacy.value(QStringLiteral("vm/memoryMiB"), 128).toInt();
        first.fullscreen = legacy.value(QStringLiteral("vm/fullscreen"), false).toBool();
        // No M1/M2 keys exist for these options: preserve old behaviour.
        first.isoBootFirst = true;
        first.networkEnabled = false;
        m_pid = legacy.value(QStringLiteral("runtime/pid"), 0).toLongLong();
    }
    m_profiles.append(first);
    m_activeId = first.id;
    m_runningProfileId = m_pid > 0 ? first.id : QString();
    loadActiveFields();
    // Do not remove or overwrite M1's vm.ini. It remains a migration backup.
    writeProfiles();
}

void VmController::load()
{
    if (!QFile::exists(configFile())) {
        migrateLegacy();
    } else {
        QSettings settings(configFile(), QSettings::IniFormat);
        bool migratedLegacyFloppy = false;
        const QStringList ids = settings.value(QStringLiteral("meta/order")).toStringList();
        for (const QString &id : ids) {
            if (id.isEmpty() || profileIndex(id) >= 0) continue;
            settings.beginGroup(QStringLiteral("profiles/") + id);
            VmProfile p;
            p.id = id;
            p.name = settings.value(QStringLiteral("name")).toString().trimmed();
            p.qemuPath = settings.value(QStringLiteral("qemuPath"), defaultQemuPath()).toString();
            p.diskPath = settings.value(QStringLiteral("diskPath")).toString();
            p.diskFormat = settings.value(QStringLiteral("diskFormat"), QStringLiteral("raw")).toString();
            p.secondaryDisks = settings.value(QStringLiteral("secondaryDisks")).toStringList();
            p.secondaryFormats = settings.value(QStringLiteral("secondaryFormats")).toStringList();
            while (p.secondaryFormats.size() < p.secondaryDisks.size())
                p.secondaryFormats.append(QStringLiteral("raw"));
            // A PC IDE controller has four slots. Reserve 0 for primary HDD
            // and 2 for CD-ROM, leaving 1 and 3 for two additional HDDs.
            if (p.secondaryDisks.size() > 2) p.secondaryDisks = p.secondaryDisks.mid(0, 2);
            if (p.secondaryFormats.size() > 2) p.secondaryFormats = p.secondaryFormats.mid(0, 2);
            p.isoFormat = settings.value(QStringLiteral("isoFormat"), QStringLiteral("raw")).toString();
            p.bootPrimary = settings.value(QStringLiteral("bootPrimary")).toString();
            p.bootSecondary = settings.value(QStringLiteral("bootSecondary")).toString();
            p.isoPath = settings.value(QStringLiteral("isoPath")).toString();
            p.floppyPath = settings.value(QStringLiteral("floppyPath")).toString();
            // M3 treated floppy files as optical media. Correct only unambiguous
            // legacy extensions; .img can represent either medium.
            if (p.floppyPath.isEmpty()
                && (p.isoPath.endsWith(QLatin1String(".fd"), Qt::CaseInsensitive)
                    || p.isoPath.endsWith(QLatin1String(".ima"), Qt::CaseInsensitive))) {
                p.floppyPath = p.isoPath;
                p.isoPath.clear();
                migratedLegacyFloppy = true;
            }
            p.memoryMiB = settings.value(QStringLiteral("memoryMiB"), 128).toInt();
            // M13 fields are optional in older profile files. A missing value
            // must retain the exact CPU/display configuration of M12r2.
            p.cpuModel = settings.value(QStringLiteral("cpuModel"), QStringLiteral("default")).toString();
            if (!QStringList{QStringLiteral("default"), QStringLiteral("486"),
                             QStringLiteral("pentium"), QStringLiteral("pentium2"),
                             QStringLiteral("pentium3"), QStringLiteral("qemu32"),
                             QStringLiteral("qemu64")}.contains(p.cpuModel))
                p.cpuModel = QStringLiteral("default");
            p.cpuCount = settings.value(QStringLiteral("cpuCount"), 1).toInt();
            if (p.cpuCount != 1 && p.cpuCount != 2 && p.cpuCount != 4)
                p.cpuCount = 1;
            p.videoAdapter = settings.value(QStringLiteral("videoAdapter"), QStringLiteral("std")).toString();
            if (!QStringList{QStringLiteral("std"), QStringLiteral("cirrus"),
                             QStringLiteral("vmware")}.contains(p.videoAdapter))
                p.videoAdapter = QStringLiteral("std");
            p.acpiEnabled = settings.value(QStringLiteral("acpiEnabled"), true).toBool();
            p.usbTabletEnabled = settings.value(QStringLiteral("usbTabletEnabled"), true).toBool();
            p.fullscreen = settings.value(QStringLiteral("fullscreen"), false).toBool();
            p.isoBootFirst = settings.value(QStringLiteral("isoBootFirst"), true).toBool();
            p.floppyBootFirst = settings.value(QStringLiteral("floppyBootFirst"), true).toBool();
            p.networkEnabled = settings.value(QStringLiteral("networkEnabled"), false).toBool();
            p.mouseOffsetX = qBound(-128, settings.value(QStringLiteral("mouseOffsetX"), 0).toInt(), 128);
            p.mouseOffsetY = qBound(-128, settings.value(QStringLiteral("mouseOffsetY"), 0).toInt(), 128);
            settings.endGroup();
            if (!p.name.isEmpty()) m_profiles.append(p);
        }
        if (m_profiles.isEmpty()) {
            // Never silently overwrite a damaged existing profile file.
            setError(QStringLiteral("No usable VM profiles found in %1. Original file preserved.").arg(configFile()));
            VmProfile fallback;
            fallback.id = newId();
            fallback.name = QStringLiteral("Recovery VM");
            fallback.qemuPath = defaultQemuPath();
            m_profiles.append(fallback);
        }
        m_activeId = settings.value(QStringLiteral("meta/activeId")).toString();
        if (profileIndex(m_activeId) < 0) m_activeId = m_profiles.first().id;
        m_pid = settings.value(QStringLiteral("runtime/pid"), 0).toLongLong();
        m_expectedQemuName = settings.value(QStringLiteral("runtime/qemuName")).toString();
        m_dbusMode = settings.value(QStringLiteral("runtime/dbusMode"), false).toBool();
        m_runningProfileId = settings.value(QStringLiteral("runtime/profileId")).toString();
        if (m_pid > 0 && profileIndex(m_runningProfileId) >= 0) {
            // Always show the VM we must control when reopening the application.
            m_activeId = m_runningProfileId;
        }
        loadActiveFields();
        if (migratedLegacyFloppy)
            writeProfiles();
    }
    if (m_pid > 0 && processExists()) {
        m_qmpConnectionStartMs = QDateTime::currentMSecsSinceEpoch();
        m_running = true;
        m_status = QStringLiteral("Reconnecting to running VM...");
        if (m_dbusMode)
            m_dbusStatus = QStringLiteral("Waiting for QMP before discovering D-Bus consoles...");
    } else {
        m_pid = 0;
        m_expectedQemuName.clear();
        m_qmpNameConfirmed = false;
        m_runningProfileId.clear();
        m_dbusMode = false;
    }
}

bool VmController::writeProfiles()
{
    QSettings settings(configFile(), QSettings::IniFormat);
    // Remove only the owned namespace, and write the complete current list.
    // Qt uses an atomic sync where supported by the filesystem.
    settings.remove(QStringLiteral("profiles"));
    QStringList ids;
    for (const VmProfile &p : m_profiles) {
        ids.append(p.id);
        settings.beginGroup(QStringLiteral("profiles/") + p.id);
        settings.setValue(QStringLiteral("name"), p.name);
        settings.setValue(QStringLiteral("qemuPath"), p.qemuPath);
        settings.setValue(QStringLiteral("diskPath"), p.diskPath);
        settings.setValue(QStringLiteral("diskFormat"), p.diskFormat);
        settings.setValue(QStringLiteral("secondaryDisks"), p.secondaryDisks);
        settings.setValue(QStringLiteral("secondaryFormats"), p.secondaryFormats);
        settings.setValue(QStringLiteral("isoFormat"), p.isoFormat);
        settings.setValue(QStringLiteral("bootPrimary"), p.bootPrimary);
        settings.setValue(QStringLiteral("bootSecondary"), p.bootSecondary);
        settings.setValue(QStringLiteral("isoPath"), p.isoPath);
        settings.setValue(QStringLiteral("floppyPath"), p.floppyPath);
        settings.setValue(QStringLiteral("memoryMiB"), p.memoryMiB);
        settings.setValue(QStringLiteral("cpuModel"), p.cpuModel);
        settings.setValue(QStringLiteral("cpuCount"), p.cpuCount);
        settings.setValue(QStringLiteral("videoAdapter"), p.videoAdapter);
        settings.setValue(QStringLiteral("acpiEnabled"), p.acpiEnabled);
        settings.setValue(QStringLiteral("usbTabletEnabled"), p.usbTabletEnabled);
        settings.setValue(QStringLiteral("fullscreen"), p.fullscreen);
        settings.setValue(QStringLiteral("isoBootFirst"), p.isoBootFirst);
        settings.setValue(QStringLiteral("floppyBootFirst"), p.floppyBootFirst);
        settings.setValue(QStringLiteral("networkEnabled"), p.networkEnabled);
        settings.setValue(QStringLiteral("mouseOffsetX"), p.mouseOffsetX);
        settings.setValue(QStringLiteral("mouseOffsetY"), p.mouseOffsetY);
        settings.endGroup();
    }
    settings.setValue(QStringLiteral("meta/version"), 7);
    settings.setValue(QStringLiteral("meta/order"), ids);
    settings.setValue(QStringLiteral("meta/activeId"), m_activeId);
    settings.setValue(QStringLiteral("runtime/pid"), m_pid);
    settings.setValue(QStringLiteral("runtime/qemuName"), m_expectedQemuName);
    settings.setValue(QStringLiteral("runtime/dbusMode"), m_dbusMode);
    settings.setValue(QStringLiteral("runtime/profileId"), m_runningProfileId);
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        setError(QStringLiteral("Could not save VM profiles to %1").arg(configFile()));
        return false;
    }
    return true;
}

void VmController::save()
{
    copyActiveToProfile();
    if (writeProfiles()) {
        // Saving settings must not erase an unrelated startup/QMP failure.
        // Errors are cleared only after a successful new VM launch.
        emit stateChanged();
        emit profilesChanged();
    }
}

#define DEFINE_STRING_SETTER(method, field) \
void VmController::method(const QString &value) { \
    if (field == value || m_running) return; \
    field = value; \
    emit configChanged(); \
}
DEFINE_STRING_SETTER(setQemuPath, m_qemuPath)
DEFINE_STRING_SETTER(setDiskPath, m_diskPath)
DEFINE_STRING_SETTER(setDiskFormat, m_diskFormat)
DEFINE_STRING_SETTER(setIsoFormat, m_isoFormat)
DEFINE_STRING_SETTER(setIsoPath, m_isoPath)
DEFINE_STRING_SETTER(setFloppyPath, m_floppyPath)
#undef DEFINE_STRING_SETTER

// Preserve the original order for saved profiles without explicit boot choices.
// The fallback HDD entry is available even when no HDD is attached (BIOS boot).
QStringList VmController::legacyBootOrder() const
{
    QStringList order;
    if (!m_floppyPath.trimmed().isEmpty() && m_floppyBootFirst)
        order << QStringLiteral("floppy");
    if (!m_isoPath.trimmed().isEmpty() && m_isoBootFirst)
        order << QStringLiteral("cdrom");
    order << QStringLiteral("hdd");
    if (!m_isoPath.trimmed().isEmpty() && !m_isoBootFirst)
        order << QStringLiteral("cdrom");
    if (!m_floppyPath.trimmed().isEmpty() && !m_floppyBootFirst)
        order << QStringLiteral("floppy");
    return order;
}

QStringList VmController::effectiveBootOrder() const
{
    const QStringList fallback = legacyBootOrder();
    auto isAvailable = [this](const QString &value) -> bool {
        return value == QLatin1String("hdd")
            || (value == QLatin1String("cdrom") && !m_isoPath.trimmed().isEmpty())
            || (value == QLatin1String("floppy") && !m_floppyPath.trimmed().isEmpty());
    };
    // Empty keys identify unchanged legacy profiles; do not change their boot semantics.
    if (m_bootPrimary.isEmpty() && m_bootSecondary.isEmpty()) return fallback;
    QStringList result;
    QString primary = m_bootPrimary;
    if (!isAvailable(primary)) primary = fallback.first();
    result << primary;
    if (m_bootSecondary == QLatin1String("none")) return result;
    if (isAvailable(m_bootSecondary) && m_bootSecondary != primary) {
        result << m_bootSecondary;
    } else {
        for (const QString &candidate : fallback) {
            if (candidate != primary) { result << candidate; break; }
        }
    }
    return result;
}

QString VmController::bootPrimary() const
{
    return effectiveBootOrder().value(0, QStringLiteral("hdd"));
}
QString VmController::bootSecondary() const
{
    return effectiveBootOrder().value(1, QStringLiteral("none"));
}
void VmController::setBootPrimary(const QString &value)
{
    if (m_running || (value != QLatin1String("hdd") && value != QLatin1String("cdrom")
                      && value != QLatin1String("floppy"))) return;
    if (m_bootPrimary == value) return;
    m_bootPrimary = value;
    // Invalidate an explicit duplicate rather than sending it to -boot.
    if (m_bootSecondary == value) m_bootSecondary.clear();
    emit configChanged();
}
void VmController::setBootSecondary(const QString &value)
{
    if (m_running || (value != QLatin1String("none") && value != QLatin1String("hdd")
                      && value != QLatin1String("cdrom") && value != QLatin1String("floppy"))) return;
    if (value == bootPrimary()) return;
    if (m_bootSecondary == value) return;
    m_bootSecondary = value;
    emit configChanged();
}

// QCOW v1 and QCOW2 share the QFI\xfb magic, but their big-endian
// version field differs. A .img filename is not reliable evidence of raw.
static QString qcowHeaderFormat(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QString();
    const QByteArray header = file.read(8);
    if (header.size() != 8 || header.left(4) != QByteArray::fromHex("514649fb")
        || header.at(4) != 0 || header.at(5) != 0 || header.at(6) != 0)
        return QString();
    if (header.at(7) == 1) return QStringLiteral("qcow");
    if (header.at(7) == 2 || header.at(7) == 3) return QStringLiteral("qcow2");
    return QString();
}

static QString detectDiskFormat(const QString &path)
{
    const QString headerFormat = qcowHeaderFormat(path);
    if (!headerFormat.isEmpty()) return headerFormat;
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("qcow")) return QStringLiteral("qcow");
    if (suffix == QLatin1String("qcow2")) return QStringLiteral("qcow2");
    if (suffix == QLatin1String("vmdk")) return QStringLiteral("vmdk");
    if (suffix == QLatin1String("vdi")) return QStringLiteral("vdi");
    return QStringLiteral("raw");
}

QVariantList VmController::secondaryDisks() const
{
    QVariantList output;
    for (int i = 0; i < m_secondaryDisks.size(); ++i) {
        QVariantMap entry;
        entry.insert(QStringLiteral("index"), i);
        entry.insert(QStringLiteral("path"), m_secondaryDisks.at(i));
        entry.insert(QStringLiteral("format"), m_secondaryFormats.value(i, QStringLiteral("raw")));
        output << entry;
    }
    return output;
}

bool VmController::addSecondaryDisk()
{
    if (m_running || m_secondaryDisks.size() >= 2) return false;
    m_secondaryDisks << QString();
    m_secondaryFormats << QStringLiteral("raw");
    emit configChanged();
    return true;
}

bool VmController::removeSecondaryDisk(int index)
{
    if (m_running || index < 0 || index >= m_secondaryDisks.size()) return false;
    m_secondaryDisks.removeAt(index);
    m_secondaryFormats.removeAt(index);
    emit configChanged();
    return true;
}

bool VmController::chooseSecondaryDiskImage(int index, const QString &path)
{
    if (m_running || index < 0 || index >= m_secondaryDisks.size()) return false;
    const QString expanded = expandHome(path);
    if (!QFileInfo(expanded).isFile() || expanded.contains(QLatin1Char(','))) {
        setError(QStringLiteral("Invalid secondary disk path: %1").arg(expanded));
        return false;
    }
    m_secondaryDisks[index] = expanded;
    m_secondaryFormats[index] = detectDiskFormat(expanded);
    emit configChanged();
    return true;
}

bool VmController::setSecondaryDiskFormat(int index, const QString &format)
{
    if (m_running || index < 0 || index >= m_secondaryFormats.size()) return false;
    if (format != QLatin1String("raw") && format != QLatin1String("qcow")
        && format != QLatin1String("qcow2") && format != QLatin1String("vmdk")
        && format != QLatin1String("vdi")) return false;
    m_secondaryFormats[index] = format;
    emit configChanged();
    return true;
}

void VmController::setMemoryMiB(int value)
{
    if (m_memoryMiB == value || m_running) return;
    m_memoryMiB = value;
    emit configChanged();
}

void VmController::setCpuModel(const QString &value)
{
    if (m_running || m_cpuModel == value) return;
    if (!QStringList{QStringLiteral("default"), QStringLiteral("486"),
                     QStringLiteral("pentium"), QStringLiteral("pentium2"),
                     QStringLiteral("pentium3"), QStringLiteral("qemu32"),
                     QStringLiteral("qemu64")}.contains(value)) return;
    m_cpuModel = value;
    emit configChanged();
}

void VmController::setCpuCount(int value)
{
    if (m_running || m_cpuCount == value) return;
    if (value != 1 && value != 2 && value != 4) return;
    m_cpuCount = value;
    emit configChanged();
}

void VmController::setVideoAdapter(const QString &value)
{
    if (m_running || m_videoAdapter == value) return;
    if (value != QLatin1String("std") && value != QLatin1String("cirrus")
        && value != QLatin1String("vmware")) return;
    m_videoAdapter = value;
    emit configChanged();
}

// Optional M14 compatibility controls. Older profiles default to exactly the
// same QEMU arguments as M13 (ACPI enabled, USB tablet installed).
void VmController::setAcpiEnabled(bool value)
{
    if (m_running || m_acpiEnabled == value) return;
    m_acpiEnabled = value;
    emit configChanged();
}

void VmController::setUsbTabletEnabled(bool value)
{
    if (m_running || m_usbTabletEnabled == value) return;
    m_usbTabletEnabled = value;
    emit configChanged();
}

// One-shot hardware-only templates: never touch disk paths, boot preferences,
// networking or emulator binary. The user may fine-tune settings afterwards.
bool VmController::applyHardwarePreset(const QString &guest)
{
    if (m_running) {
        setError(QStringLiteral("Stop the VM before changing virtual hardware."));
        return false;
    }
    if (guest == QLatin1String("freedos")) {
        m_cpuModel = QStringLiteral("default");
        m_cpuCount = 1;
        m_memoryMiB = 64;
        m_videoAdapter = QStringLiteral("std");
        m_acpiEnabled = false;
        m_usbTabletEnabled = false;
    } else if (guest == QLatin1String("win95")) {
        m_cpuModel = QStringLiteral("pentium");
        m_cpuCount = 1;
        m_memoryMiB = 128;
        m_videoAdapter = QStringLiteral("cirrus");
        m_acpiEnabled = false;
        m_usbTabletEnabled = false;
    } else if (guest == QLatin1String("winxp")) {
        m_cpuModel = QStringLiteral("default");
        m_cpuCount = 1;
        m_memoryMiB = 512;
        m_videoAdapter = QStringLiteral("std");
        m_acpiEnabled = true;
        m_usbTabletEnabled = true;
    } else {
        setError(QStringLiteral("Unknown guest hardware preset: %1").arg(guest));
        return false;
    }
    emit configChanged();
    return true;
}

void VmController::setFullscreen(bool value)
{
    if (m_fullscreen == value || m_running) return;
    m_fullscreen = value;
    emit configChanged();
}

void VmController::setIsoBootFirst(bool value)
{
    if (m_isoBootFirst == value || m_running) return;
    m_isoBootFirst = value;
    emit configChanged();
}

void VmController::setFloppyBootFirst(bool value)
{
    if (m_floppyBootFirst == value || m_running) return;
    m_floppyBootFirst = value;
    emit configChanged();
}

void VmController::setNetworkEnabled(bool value)
{
    if (m_networkEnabled == value || m_running) return;
    m_networkEnabled = value;
    emit configChanged();
}

// M8: adjust touch alignment without restarting the VM. Store offsets in
// guest pixels, separately for each profile; never modify disk images.
void VmController::adjustMouseOffset(int dx, int dy)
{
    if (!m_mouseAbsolute || !m_mouseAvailable || !m_running || !m_dbusMode) return;
    const int x = qBound(-128, m_mouseOffsetX + dx, 128);
    const int y = qBound(-128, m_mouseOffsetY + dy, 128);
    if (x == m_mouseOffsetX && y == m_mouseOffsetY) return;
    m_mouseOffsetX = x;
    m_mouseOffsetY = y;
    copyActiveToProfile();
    writeProfiles();
    emit configChanged();
}

void VmController::resetMouseOffset()
{
    if (m_mouseOffsetX == 0 && m_mouseOffsetY == 0) return;
    m_mouseOffsetX = 0;
    m_mouseOffsetY = 0;
    copyActiveToProfile();
    writeProfiles();
    emit configChanged();
}

bool VmController::chooseDiskImage(const QString &path)
{
    if (m_running) {
        setError(QStringLiteral("Stop the VM before changing the disk image."));
        return false;
    }
    const QString expanded = expandHome(path);
    if (!QFileInfo(expanded).isFile() || expanded.contains(QLatin1Char(','))) {
        setError(QStringLiteral("Invalid disk-image path (or comma in filename): %1").arg(expanded));
        return false;
    }
    // Inspect QCOW version before falling back to filename extensions.
    // W95_c.img, for example, can contain a QCOW v1 disk rather than raw.
    setDiskPath(expanded);
    setDiskFormat(detectDiskFormat(expanded));
    return true;
}

bool VmController::createProfile(const QString &name)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before changing profiles.")); return false; }
    if (!nameAvailable(name)) { setError(QStringLiteral("Enter a unique profile name (1–80 characters).")); return false; }
    copyActiveToProfile();
    VmProfile p;
    p.id = newId();
    p.name = name.trimmed();
    p.qemuPath = m_qemuPath;  // Reuse the emulator, not a writable disk image.
    m_profiles.append(p);
    m_activeId = p.id;
    loadActiveFields();
    save();
    return true;
}

bool VmController::selectProfile(const QString &id)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before switching profiles.")); return false; }
    if (profileIndex(id) < 0) { setError(QStringLiteral("Unknown VM profile.")); return false; }
    if (m_activeId == id) return true;
    save();
    m_activeId = id;
    loadActiveFields();
    save();
    return true;
}

bool VmController::renameProfile(const QString &id, const QString &name)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before renaming profiles.")); return false; }
    const int index = profileIndex(id);
    if (index < 0 || !nameAvailable(name, id)) {
        setError(QStringLiteral("Profile name must be unique and between 1–80 characters."));
        return false;
    }
    m_profiles[index].name = name.trimmed();
    save();
    return true;
}

// M14r2: commit the name and the active editor fields together. Previously
// renameProfile() emitted profilesChanged in the middle of a QML TextField
// binding and the page then called save() a second time. A detached edit field
// plus one commit also protects against saving settings to the wrong clone.
bool VmController::saveProfileEdits(const QString &id, const QString &name)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before saving its configuration.")); return false; }
    if (id != m_activeId || profileIndex(id) < 0) {
        setError(QStringLiteral("The selected VM changed while editing. Reopen its settings."));
        return false;
    }
    if (!nameAvailable(name, id)) {
        setError(QStringLiteral("Profile name must be unique and between 1–80 characters."));
        return false;
    }
    copyActiveToProfile();
    const int index = profileIndex(id);
    const QString previousName = m_profiles[index].name;
    m_profiles[index].name = name.trimmed();
    if (!writeProfiles()) {
        m_profiles[index].name = previousName;
        return false;
    }
    m_lastError.clear();
    emit profilesChanged();
    emit stateChanged();
    return true;
}

// Read-only configuration inspection: this describes what WILL be passed to
// QEMU. The QMP query-block report confirms what QEMU ACTUALLY attached.
QString VmController::mediaConfigurationReport() const
{
    QStringList lines;
    lines << QStringLiteral("Profile: %1").arg(activeProfileName());
    const QStringList order = effectiveBootOrder();
    lines << QStringLiteral("BIOS boot order: %1").arg(order.join(QStringLiteral(" → ")));
    auto describe = [&lines](const QString &slot, const QString &path, const QString &format) {
        if (path.trimmed().isEmpty()) {
            lines << QStringLiteral("%1: empty").arg(slot);
            return;
        }
        const QString expanded = expandHome(path);
        const QFileInfo fi(expanded);
        QString state = !fi.isFile() ? QStringLiteral("MISSING")
                        : !fi.isReadable() ? QStringLiteral("NOT READABLE")
                        : QStringLiteral("file present");
        if (fi.isFile()) state += QStringLiteral(", %1 bytes").arg(fi.size());
        lines << QStringLiteral("%1: %2 [%3], %4").arg(slot, expanded, format, state);
        // Report QCOW v1/v2 mismatches without modifying the image.
        if (fi.isFile() && fi.isReadable()) {
            const QString actual = qcowHeaderFormat(expanded);
            if (!actual.isEmpty() && actual != format)
                lines << QStringLiteral("WARNING: actual format is %1, but VM is set to %2. "
                                        "Change the image-format dropdown before launching.")
                             .arg(actual, format);
        }
    };
    describe(QStringLiteral("IDE 0 primary HDD"), m_diskPath, m_diskFormat);
    for (int i = 0; i < m_secondaryDisks.size() && i < 2; ++i)
        describe(QStringLiteral("IDE %1 secondary HDD").arg(i == 0 ? 1 : 3),
                 m_secondaryDisks.at(i), m_secondaryFormats.value(i, QStringLiteral("raw")));
    describe(QStringLiteral("IDE 2 CD/DVD"), m_isoPath, m_isoFormat);
    describe(QStringLiteral("Floppy A:"), m_floppyPath, QStringLiteral("raw, read-only"));
    lines << QStringLiteral("A DOS boot floppy can show A: without recognising a DOS-compatible C: partition.");
    return lines.join(QLatin1Char('\n'));
}

// Inspect profile references without modifying a disk image. Sharing an image
// between VM profiles is intentional in some workflows, but writes to one
// profile's disk are visible to the other; a settings clone is not a disk copy.
QString VmController::storageReferences(const QString &path) const
{
    const QString input = expandHome(path);
    const QFileInfo inputInfo(input);
    const QString target = inputInfo.canonicalFilePath().isEmpty()
                           ? inputInfo.absoluteFilePath() : inputInfo.canonicalFilePath();
    if (path.trimmed().isEmpty()) return QStringLiteral("No image selected.");
    QStringList users;
    for (const VmProfile &p : m_profiles) {
        const bool active = p.id == m_activeId;
        const QString primary = active ? m_diskPath : p.diskPath;
        const QStringList secondary = active ? m_secondaryDisks : p.secondaryDisks;
        auto matches = [&target](const QString &candidate) {
            if (candidate.trimmed().isEmpty()) return false;
            const QFileInfo fi(expandHome(candidate));
            const QString resolved = fi.canonicalFilePath().isEmpty()
                                      ? fi.absoluteFilePath() : fi.canonicalFilePath();
            return target == resolved;
        };
        if (matches(primary)) users << p.name + QStringLiteral(" (primary HDD)");
        for (int i = 0; i < secondary.size(); ++i)
            if (matches(secondary.at(i)))
                users << p.name + QStringLiteral(" (secondary HDD %1)").arg(i + 1);
    }
    if (users.isEmpty()) return QStringLiteral("No VM profiles reference this HDD image.");
    return QStringLiteral("Referenced by: %1\n%2")
           .arg(users.join(QStringLiteral(", ")),
                users.size() > 1 ? QStringLiteral("WARNING: Shared writable disk. Changes in one VM affect the others.")
                                 : QStringLiteral("This disk belongs to the named VM profile."));
}

void VmController::inspectAttachedMedia()
{
    if (!m_running || !m_connected || m_qmp.state() != QLocalSocket::ConnectedState) {
        m_attachedMediaReport = QStringLiteral("Start a VM and wait for QMP before inspecting attached media.");
        emit attachedMediaChanged();
        return;
    }
    m_attachedMediaReport = QStringLiteral("Querying running QEMU block devices...");
    emit attachedMediaChanged();
    sendQmp(QStringLiteral("query-block"));
}

bool VmController::duplicateProfile(const QString &id)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before duplicating profiles.")); return false; }
    const int index = profileIndex(id);
    if (index < 0) { setError(QStringLiteral("Unknown VM profile.")); return false; }
    copyActiveToProfile();
    VmProfile copy = m_profiles.at(index);
    QString name = copy.name + QStringLiteral(" copy");
    int suffix = 2;
    while (!nameAvailable(name) && suffix < 1000)
        name = copy.name.left(65) + QStringLiteral(" copy %1").arg(suffix++);
    if (!nameAvailable(name)) { setError(QStringLiteral("Could not find a free profile name.")); return false; }
    copy.name = name;
    copy.id = newId();
    m_profiles.append(copy);
    m_activeId = copy.id;
    loadActiveFields();
    save();
    return true;
}

bool VmController::deleteProfile(const QString &id)
{
    if (m_running) { setError(QStringLiteral("Stop the VM before deleting profiles.")); return false; }
    const int index = profileIndex(id);
    if (index < 0) { setError(QStringLiteral("Unknown VM profile.")); return false; }
    if (m_profiles.size() == 1) {
        setError(QStringLiteral("At least one VM profile must remain."));
        return false;
    }
    copyActiveToProfile();
    const bool wasActive = m_activeId == id;
    m_profiles.removeAt(index);
    if (wasActive) {
        m_activeId = m_profiles.at(qMin(index, m_profiles.size() - 1)).id;
        loadActiveFields();
    }
    save();
    return true;
}

void VmController::setStatus(const QString &status)
{
    if (m_status == status) return;
    m_status = status;
    emit stateChanged();
}

void VmController::setError(const QString &error)
{
    m_lastError = error;
    qWarning().noquote() << "SailQEMU:" << error;
    emit stateChanged();
}

void VmController::setRunning(bool running, bool paused)
{
    m_running = running;
    m_paused = paused;
    emit stateChanged();
}

bool VmController::processExists() const
{
    if (m_pid <= 0) return false;
    // PID reuse protection: another process with the same PID must not be
    // treated as this VM. Require our QMP socket in the QEMU command line.
    QFile cmdline(QStringLiteral("/proc/%1/cmdline").arg(m_pid));
    if (!cmdline.open(QIODevice::ReadOnly)) return false;
    const QByteArray args = cmdline.readAll();
    if (!args.contains("qemu-system-x86_64") ||
        !args.contains("-qmp") ||
        !args.contains(m_socketPath.toUtf8())) return false;
    QFile status(QStringLiteral("/proc/%1/status").arg(m_pid));
    if (status.open(QIODevice::ReadOnly)) {
        const QByteArray s = status.readAll();
        if (s.contains("State:\tZ") || s.contains("State:\tX")) return false;
    }
    return ::kill(static_cast<pid_t>(m_pid), 0) == 0 || errno == EPERM;
}

void VmController::resetStopped(const QString &message)
{
    m_shutdownPending = false;
    m_shutdownStatus.clear();
    stopLiveDisplay();
    m_attachedMediaReport.clear();
    emit attachedMediaChanged();
    resetMouse();
    m_qmp.abort();
    m_qmpBuffer.clear();
    clearGuestInput();
    m_guestPreviewAvailable = false;
    emit guestPreviewChanged();
    m_pid = 0;
    m_expectedQemuName.clear();
    m_qmpNameConfirmed = false;
    m_qmpConnectionStartMs = 0;
    m_qmpConnectionWarned = false;
    m_hadQmpConnection = false;
    m_seenGuestShutdown = false;
    m_forceQuitRequested = false;
    m_runningProfileId.clear();
    m_connected = false;
    m_dbusMode = false;
    setDbusStatus(QStringLiteral("D-Bus display not started"));
    setRunning(false, false);
    setStatus(message);
    save();
}

bool VmController::startVm()
{
    return startVmInternal(false);
}

bool VmController::startVmWithConsole()
{
    // The Guest console remains available during boot, without pausing QEMU.
    return startVmInternal(false);
}

bool VmController::startVmWithDbusConsole()
{
    // A normal D-Bus launch boots immediately, without an extra Play tap.
    return startVmInternal(true);
}

bool VmController::startVmInternal(bool dbusDisplay)
{
    if (m_running || (m_pid > 0 && processExists())) {
        setError(QStringLiteral("A VM is already running. Use its Sailfish window."));
        return false;
    }
    const QString executable = expandHome(m_qemuPath);
    const QFileInfo binary(executable);
    if (!binary.isFile() || !binary.isExecutable()) {
        setError(QStringLiteral("QEMU executable is missing or not executable: %1").arg(executable));
        return false;
    }
    if (m_memoryMiB < 16 || m_memoryMiB > 32768) {
        setError(QStringLiteral("RAM must be between 16 and 32768 MiB."));
        return false;
    }
    if (m_cpuCount != 1 && m_cpuCount != 2 && m_cpuCount != 4) {
        setError(QStringLiteral("Virtual CPU count must be 1, 2 or 4."));
        return false;
    }
    if (!QStringList{QStringLiteral("default"), QStringLiteral("486"),
                     QStringLiteral("pentium"), QStringLiteral("pentium2"),
                     QStringLiteral("pentium3"), QStringLiteral("qemu32"),
                     QStringLiteral("qemu64")}.contains(m_cpuModel)) {
        setError(QStringLiteral("Unsupported x86 CPU model."));
        return false;
    }
    if (m_videoAdapter != QLatin1String("std") && m_videoAdapter != QLatin1String("cirrus")
        && m_videoAdapter != QLatin1String("vmware")) {
        setError(QStringLiteral("Unsupported VGA adapter."));
        return false;
    }
    const QString format = m_diskFormat.trimmed().toLower();
    const QString opticalFormat = m_isoFormat.trimmed().toLower();
    if (format != QLatin1String("raw") && format != QLatin1String("qcow")
        && format != QLatin1String("qcow2") && format != QLatin1String("vmdk")
        && format != QLatin1String("vdi")) {
        setError(QStringLiteral("Disk format must be raw, qcow, qcow2, vmdk or vdi."));
        return false;
    }
    if (opticalFormat != QLatin1String("raw") && opticalFormat != QLatin1String("qcow")
        && opticalFormat != QLatin1String("qcow2") && opticalFormat != QLatin1String("vmdk")
        && opticalFormat != QLatin1String("vdi")) {
        setError(QStringLiteral("CD/DVD image format must be raw, qcow, qcow2, vmdk or vdi."));
        return false;
    }
    const QString disk = expandHome(m_diskPath);
    const QString iso = expandHome(m_isoPath);
    const QString floppy = expandHome(m_floppyPath);
    if (!disk.isEmpty() && (!QFileInfo(disk).isFile() || disk.contains(QLatin1Char(',')))) {
        setError(QStringLiteral("Disk file is missing, or its filename contains a comma: %1").arg(disk));
        return false;
    }
    for (int i = 0; i < m_secondaryDisks.size(); ++i) {
        const QString extra = expandHome(m_secondaryDisks.at(i));
        const QString extraFormat = m_secondaryFormats.value(i, QStringLiteral("raw"));
        if (extra.isEmpty() || !QFileInfo(extra).isFile() || extra.contains(QLatin1Char(','))) {
            setError(QStringLiteral("Secondary HDD %1 is missing or invalid.").arg(i + 1));
            return false;
        }
        if (extraFormat != QLatin1String("raw") && extraFormat != QLatin1String("qcow")
            && extraFormat != QLatin1String("qcow2") && extraFormat != QLatin1String("vmdk")
            && extraFormat != QLatin1String("vdi")) {
            setError(QStringLiteral("Invalid secondary HDD format."));
            return false;
        }
        if (extra == disk || m_secondaryDisks.mid(0, i).contains(extra)) {
            setError(QStringLiteral("A disk image cannot be attached twice as writable storage."));
            return false;
        }
    }
    if (!iso.isEmpty() && (!QFileInfo(iso).isFile() || iso.contains(QLatin1Char(',')))) {
        setError(QStringLiteral("CD/DVD image missing, or its filename contains a comma: %1").arg(iso));
        return false;
    }
    if (iso.endsWith(QLatin1String(".fd"), Qt::CaseInsensitive)
        || iso.endsWith(QLatin1String(".ima"), Qt::CaseInsensitive)) {
        setError(QStringLiteral("This is a floppy image, not a CD-ROM. Use the Floppy image (A:) field."));
        return false;
    }
    if (!floppy.isEmpty() && (!QFileInfo(floppy).isFile() || floppy.contains(QLatin1Char(',')))) {
        setError(QStringLiteral("Floppy image missing, or its filename contains a comma: %1").arg(floppy));
        return false;
    }
    if (!floppy.isEmpty() && !QFileInfo(floppy).isReadable()) {
        setError(QStringLiteral("Floppy image is not readable: %1").arg(floppy));
        return false;
    }
    // Opening a QCOW header as raw exposes its physical file length as a
    // bogus disk and may overwrite QCOW metadata if the guest writes to it.
    auto checkQcowMismatch = [this](const QString &label, const QString &path,
                                   const QString &declaredFormat) -> bool {
        if (path.isEmpty()) return true;
        const QString actual = qcowHeaderFormat(path);
        if (actual.isEmpty() || actual == declaredFormat) return true;
        setError(QStringLiteral("%1 is %2, but its format is set to %3. "
                                "Select %2 in the VM image-format dropdown.")
                     .arg(label, actual, declaredFormat));
        return false;
    };
    if (!checkQcowMismatch(QStringLiteral("Primary HDD"), disk, format)) return false;
    for (int i = 0; i < m_secondaryDisks.size(); ++i)
        if (!checkQcowMismatch(QStringLiteral("Secondary HDD %1").arg(i + 1),
                               expandHome(m_secondaryDisks.at(i)),
                               m_secondaryFormats.value(i, QStringLiteral("raw")))) return false;
    if (!checkQcowMismatch(QStringLiteral("CD/DVD"), iso, opticalFormat)) return false;
    if (!QFileInfo(executable).fileName().contains(QLatin1String("qemu-system-x86_64"))) {
        setError(QStringLiteral("Only x86_64 guest profiles are currently supported."));
        return false;
    }
    if (m_pid > 0 && processExists()) {
        setError(QStringLiteral("Previous VM is still active; reconnect instead."));
        return false;
    }
    // The current packaged QEMU 10.2.4 was compiled without libslirp.
    // Check the selected executable rather than letting -net user kill QEMU.
    if (m_networkEnabled) {
        QProcess probe;
        probe.start(executable, QStringList() << QStringLiteral("-netdev") << QStringLiteral("help"));
        if (!probe.waitForStarted(2500)) {
            setError(QStringLiteral("Unable to inspect QEMU networking support: %1").arg(probe.errorString()));
            return false;
        }
        if (!probe.waitForFinished(4000)) {
            probe.kill(); probe.waitForFinished(1000);
            setError(QStringLiteral("Timed out checking QEMU user-mode networking support."));
            return false;
        }
        const QString netHelp = QString::fromLocal8Bit(probe.readAllStandardOutput()
                                                     + probe.readAllStandardError());
        // The -netdev help output lists available types, one per line.
        const QRegExp userBackend(QStringLiteral("(^|\\n)\\s*user(\\s|$)"), Qt::CaseInsensitive);
        if (userBackend.indexIn(netHelp) < 0) {
            setError(QStringLiteral("This QEMU executable has no SLIRP/user NAT backend. "
                                    "Disable 'Enable guest networking' in the VM profile. "
                                    "The installed QEMU 10.2.4 RPM was built without libslirp."));
            return false;
        }
    }
    if (dbusDisplay) {
        // The first integration step uses the normal session bus and the
        // well-known 'org.qemu' name, not QMP FD hand-off. Prevent confusing
        // another VM's display with ours when an owner already exists.
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected() || !bus.interface()) {
            setError(QStringLiteral("No D-Bus session bus available for the M5 display test."));
            return false;
        }
        QDBusReply<bool> occupied = bus.interface()->isServiceRegistered(QStringLiteral("org.qemu"));
        if (!occupied.isValid()) {
            setError(QStringLiteral("Cannot check session bus ownership: %1").arg(occupied.error().message()));
            return false;
        }
        if (occupied.value()) {
            setError(QStringLiteral("org.qemu is already in use by another VM. Stop it before starting M5 D-Bus mode."));
            return false;
        }
    }

    stopLiveDisplay();
    m_attachedMediaReport.clear();
    emit attachedMediaChanged();
    resetMouse();
    m_qmp.abort();
    clearGuestInput();
    m_guestPreviewAvailable = false;
    emit guestPreviewChanged();
    QFile::remove(socketFile());
    // x86 PC BIOS identifiers: a=floppy, c=hard disk, d=CD/DVD.
    // Legacy profiles retain their original order until edited via dropdowns.
    QString bootOrder;
    for (const QString &choice : effectiveBootOrder()) {
        if (choice == QLatin1String("floppy")) bootOrder += QLatin1Char('a');
        else if (choice == QLatin1String("cdrom")) bootOrder += QLatin1Char('d');
        else if (choice == QLatin1String("hdd")) bootOrder += QLatin1Char('c');
    }
    if (bootOrder.isEmpty()) bootOrder = QStringLiteral("c");

    // A distinct operator-side VM identity is exposed by BOTH QMP query-name
    // and org.qemu.Display1.VM.Name. Compare them instead of PIDs: Sailjail
    // runs QEMU in a PID namespace while the session bus reports host PIDs.
    // -name does not alter the guest's firmware/SMBIOS UUID.
    const QString launchName = QStringLiteral("sailqemu-") + newId();
    QStringList args;
    args << QStringLiteral("-name") << launchName;
    args << QStringLiteral("-display")
         << (dbusDisplay ? QStringLiteral("dbus,gl=off") : QStringLiteral("sdl,gl=off"))
         << QStringLiteral("-machine")
         << (m_acpiEnabled ? QStringLiteral("pc") : QStringLiteral("pc,acpi=off"))
         << QStringLiteral("-m") << QString::number(m_memoryMiB)
         << QStringLiteral("-vga") << m_videoAdapter
         << QStringLiteral("-net") << (m_networkEnabled ? QStringLiteral("nic,model=rtl8139") : QStringLiteral("none"))
         << QStringLiteral("-boot") << (QStringLiteral("order=") + bootOrder)
         << QStringLiteral("-qmp")
         << (QStringLiteral("unix:") + socketFile() + QStringLiteral(",server,nowait"));
    // The standard PC already includes a PS/2 mouse. Older guests can omit
    // the optional USB tablet while keeping relative mouse input available.
    if (m_usbTabletEnabled)
        args << QStringLiteral("-usb") << QStringLiteral("-device") << QStringLiteral("usb-tablet");
    // Preserve old QEMU defaults unless the user explicitly changes them.
    if (m_cpuModel != QLatin1String("default"))
        args << QStringLiteral("-cpu") << m_cpuModel;
    if (m_cpuCount > 1)
        args << QStringLiteral("-smp") << (QStringLiteral("cpus=") + QString::number(m_cpuCount));
    if (m_networkEnabled) args << QStringLiteral("-net") << QStringLiteral("user");
    if (!disk.isEmpty()) {
        args << QStringLiteral("-drive")
             << (QStringLiteral("file=") + disk + QStringLiteral(",format=") + format + QStringLiteral(",if=ide,index=0"));
    }
    // Explicit IDE units avoid collisions with the optical drive at index 2.
    // index 0 = primary HDD; index 1/3 = optional additional HDDs.
    const int secondaryUnits[] = {1, 3};
    for (int i = 0; i < m_secondaryDisks.size(); ++i) {
        args << QStringLiteral("-drive")
             << (QStringLiteral("file=") + expandHome(m_secondaryDisks.at(i))
                 + QStringLiteral(",format=") + m_secondaryFormats.at(i)
                 + QStringLiteral(",if=ide,index=") + QString::number(secondaryUnits[i]));
    }
    if (!iso.isEmpty()) {
        args << QStringLiteral("-drive")
             << (QStringLiteral("file=") + iso + QStringLiteral(",format=") + opticalFormat
                 + QStringLiteral(",if=ide,index=2,media=cdrom,readonly=on"));
    }
    if (!floppy.isEmpty()) {
        // Installation/boot floppy is protected from writes. The hard disk is
        // the persistent writable medium; a writeable A: can be added later.
        args << QStringLiteral("-drive")
             << (QStringLiteral("file=") + floppy
                 + QStringLiteral(",if=floppy,index=0,format=raw,readonly=on"));
    }
    if (m_fullscreen && !dbusDisplay) args << QStringLiteral("-full-screen");
    // No -S: normal VM starts must enter firmware/guest boot immediately.

    // Persist selected configuration before launching any guest.
    save();
    qint64 pid = 0;
    QString spawnError;
    if (!spawnQemu(executable, args, m_logPath, &pid, &spawnError)) {
        setError(spawnError);
        return false;
    }
    m_lastError.clear();
    m_pid = pid;
    m_expectedQemuName = launchName;
    m_qmpNameConfirmed = false;
    m_hadQmpConnection = false;
    m_seenGuestShutdown = false;
    m_forceQuitRequested = false;
    m_qmpConnectionWarned = false;
    m_qmpConnectionStartMs = QDateTime::currentMSecsSinceEpoch();
    m_dbusMode = dbusDisplay;
    setDbusStatus(dbusDisplay
                  ? QStringLiteral("Waiting for QEMU D-Bus service...")
                  : QStringLiteral("D-Bus display not started"));
    m_runningProfileId = m_activeId;
    m_tick = 0;
    setRunning(true, false);
    setStatus(dbusDisplay ? QStringLiteral("Starting VM with integrated D-Bus display...")
                          : QStringLiteral("Starting VM with SDL display..."));
    save();
    return true;
}

void VmController::readQmp()
{
    m_qmpBuffer.append(m_qmp.readAll());
    if (m_qmpBuffer.size() > 1024 * 1024) {
        setError(QStringLiteral("QMP response exceeded 1 MiB."));
        m_qmp.abort();
        m_qmpBuffer.clear();
        return;
    }
    int newline = -1;
    while ((newline = m_qmpBuffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_qmpBuffer.left(newline).trimmed();
        m_qmpBuffer.remove(0, newline + 1);
        if (!line.isEmpty()) handleQmpLine(line);
    }
}

void VmController::handleQmpLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) return;
    const QJsonObject obj = document.object();
    if (obj.contains(QStringLiteral("QMP"))) {
        m_qmp.write("{\"execute\":\"qmp_capabilities\",\"id\":\"caps\"}\n");
        return;
    }
    if (obj.contains(QStringLiteral("error"))) {
        if (obj.value(QStringLiteral("id")).toString() == QLatin1String("guest-preview"))
            m_guestPreviewPending = false;
        if (obj.value(QStringLiteral("id")).toString() == QLatin1String("system_powerdown")) {
            m_shutdownPending = false;
            m_shutdownStatus.clear();
        }
        const QString description = obj.value(QStringLiteral("error")).toObject()
            .value(QStringLiteral("desc")).toString();
        if (obj.value(QStringLiteral("id")).toString() == QLatin1String("query-name")) {
            m_qmpNameConfirmed = false;
            setDbusStatus(QStringLiteral("QMP cannot verify VM identity: %1").arg(description));
        }
        if (obj.value(QStringLiteral("id")).toString() == QLatin1String("query-block")) {
            m_attachedMediaReport = QStringLiteral("QMP query-block failed: %1").arg(description);
            emit attachedMediaChanged();
        }
        setError(QStringLiteral("QMP: %1").arg(description));
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("guest-preview")) {
        m_guestPreviewPending = false;
        completeGuestPreview();
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("caps")) {
        m_qmpNameConfirmed = false;
        m_connected = true;
        m_hadQmpConnection = true;
        m_qmpConnectionWarned = false;
        setStatus(QStringLiteral("QEMU running (QMP connected)"));
        emit stateChanged();
        sendQmp(QStringLiteral("query-status"));
        if (m_dbusMode) sendQmp(QStringLiteral("query-name"));
        if (m_dbusMode)
            QTimer::singleShot(250, this, &VmController::refreshDbusDisplay);
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("system_powerdown")) {
        // A successful QMP acknowledgement means only that the power-button
        // request was issued; it does NOT mean the guest has shut down.
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("query-name")) {
        const QString qmpName = obj.value(QStringLiteral("return")).toObject()
                                .value(QStringLiteral("name")).toString();
        m_qmpNameConfirmed = !m_expectedQemuName.isEmpty()
                             && qmpName == m_expectedQemuName;
        if (!m_qmpNameConfirmed) {
            setDbusStatus(QStringLiteral("QMP identity mismatch. Refusing D-Bus input; "
                                         "shut down and restart the VM with this build."));
            return;
        }
        if (m_dbusMode)
            QTimer::singleShot(0, this, &VmController::refreshDbusDisplay);
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("query-block")) {
        const QJsonArray blocks = obj.value(QStringLiteral("return")).toArray();
        QStringList lines;
        lines << QStringLiteral("Attached devices reported by QEMU (%1):").arg(blocks.size());
        for (const QJsonValue &blockValue : blocks) {
            const QJsonObject block = blockValue.toObject();
            const QString device = block.value(QStringLiteral("device")).toString();
            const QJsonObject inserted = block.value(QStringLiteral("inserted")).toObject();
            if (inserted.isEmpty()) {
                lines << QStringLiteral("%1: no medium").arg(device);
                continue;
            }
            lines << QStringLiteral("%1: %2 (%3, %4)")
                         .arg(device, inserted.value(QStringLiteral("file")).toString(),
                              inserted.value(QStringLiteral("drv")).toString(),
                              inserted.value(QStringLiteral("ro")).toBool()
                                  ? QStringLiteral("read-only") : QStringLiteral("writable"));
        }
        m_attachedMediaReport = lines.join(QLatin1Char('\n'));
        emit attachedMediaChanged();
        return;
    }
    if (obj.value(QStringLiteral("id")).toString() == QLatin1String("query-status")) {
        const QString state = obj.value(QStringLiteral("return")).toObject()
            .value(QStringLiteral("status")).toString();
        if (state == QLatin1String("paused") || state == QLatin1String("prelaunch")) {
            setRunning(true, true);
            setStatus(QStringLiteral("VM paused"));
        } else if (!state.isEmpty()) {
            setRunning(true, false);
            setStatus(QStringLiteral("VM: %1").arg(state));
        }
        return;
    }
    const QString event = obj.value(QStringLiteral("event")).toString();
    if (event == QLatin1String("STOP")) {
        setRunning(true, true);
        setStatus(QStringLiteral("VM paused"));
    } else if (event == QLatin1String("RESUME")) {
        setRunning(true, false);
        setStatus(QStringLiteral("VM running"));
    } else if (event == QLatin1String("SHUTDOWN")) {
        m_seenGuestShutdown = true;
        m_shutdownPending = false;
        m_shutdownStatus.clear();
        setStatus(QStringLiteral("VM shutting down..."));
    }
}

void VmController::sendQmp(const QString &command)
{
    if (!m_connected || m_qmp.state() != QLocalSocket::ConnectedState) {
        setError(QStringLiteral("QMP not connected yet."));
        return;
    }
    QJsonObject payload;
    payload.insert(QStringLiteral("execute"), command);
    payload.insert(QStringLiteral("id"), command);
    m_qmp.write(QJsonDocument(payload).toJson(QJsonDocument::Compact) + '\n');
    m_qmp.flush();
}

// QEMU 4.1 QMP send-key uses physical PC keyboard qcodes, not Unicode.
// Only the well-defined US/ASCII subset is accepted here. Never silently
// replace unsupported characters: the guest could receive a wrong command.
static bool guestCharacterKeyCodes(QChar character, QStringList &keys)
{
    const ushort code = character.unicode();
    if (code >= 'a' && code <= 'z') {
        keys << QString(QChar(code));
        return true;
    }
    if (code >= 'A' && code <= 'Z') {
        keys << QStringLiteral("shift") << QString(QChar(code + ('a' - 'A')));
        return true;
    }
    if (code >= '0' && code <= '9') {
        keys << QString(QChar(code));
        return true;
    }
    const char *name = nullptr;
    bool shift = false;
    switch (code) {
    case ' ': name = "spc"; break;
    case '\n': case '\r': name = "ret"; break;
    case '\t': name = "tab"; break;
    case '-': name = "minus"; break;
    case '=': name = "equal"; break;
    case '[': name = "bracket_left"; break;
    case ']': name = "bracket_right"; break;
    case ';': name = "semicolon"; break;
    case '\'': name = "apostrophe"; break;
    case '`': name = "grave_accent"; break;
    case '\\': name = "backslash"; break;
    case ',': name = "comma"; break;
    case '.': name = "dot"; break;
    case '/': name = "slash"; break;
    case '!': name = "1"; shift = true; break;
    case '@': name = "2"; shift = true; break;
    case '#': name = "3"; shift = true; break;
    case '$': name = "4"; shift = true; break;
    case '%': name = "5"; shift = true; break;
    case '^': name = "6"; shift = true; break;
    case '&': name = "7"; shift = true; break;
    case '*': name = "8"; shift = true; break;
    case '(': name = "9"; shift = true; break;
    case ')': name = "0"; shift = true; break;
    case '_': name = "minus"; shift = true; break;
    case '+': name = "equal"; shift = true; break;
    case '{': name = "bracket_left"; shift = true; break;
    case '}': name = "bracket_right"; shift = true; break;
    case ':': name = "semicolon"; shift = true; break;
    case '"': name = "apostrophe"; shift = true; break;
    case '~': name = "grave_accent"; shift = true; break;
    case '|': name = "backslash"; shift = true; break;
    case '<': name = "comma"; shift = true; break;
    case '>': name = "dot"; shift = true; break;
    case '?': name = "slash"; shift = true; break;
    default: return false;
    }
    if (shift) keys << QStringLiteral("shift");
    keys << QLatin1String(name);
    return true;
}

bool VmController::guestInputReady()
{
    if (!m_running || m_paused || !m_connected || m_qmp.state() != QLocalSocket::ConnectedState) {
        setError(QStringLiteral("Start/resume the VM and wait for QMP to connect before sending keys."));
        return false;
    }
    return true;
}

bool VmController::queueKeyStroke(const QStringList &keyCodes)
{
    if (m_pendingKeys.size() >= 256) {
        setError(QStringLiteral("Guest keyboard queue is full. Retry after the previous input finishes."));
        return false;
    }
    m_pendingKeys.append(keyCodes);
    if (!m_keyTimer.isActive()) {
        dispatchNextKey();
        m_keyTimer.start();
    }
    return true;
}

void VmController::dispatchNextKey()
{
    if (!m_running || m_paused || !m_connected || m_qmp.state() != QLocalSocket::ConnectedState) {
        clearGuestInput();
        return;
    }
    if (m_pendingKeys.isEmpty()) {
        m_keyTimer.stop();
        return;
    }
    const QStringList codes = m_pendingKeys.takeFirst();
    QJsonArray keys;
    for (const QString &code : codes) {
        QJsonObject key;
        key.insert(QStringLiteral("type"), QStringLiteral("qcode"));
        key.insert(QStringLiteral("data"), code);
        keys.append(key);
    }
    QJsonObject arguments;
    arguments.insert(QStringLiteral("keys"), keys);
    arguments.insert(QStringLiteral("hold-time"), 65);
    QJsonObject command;
    command.insert(QStringLiteral("execute"), QStringLiteral("send-key"));
    command.insert(QStringLiteral("arguments"), arguments);
    command.insert(QStringLiteral("id"), QStringLiteral("guest-key-%1").arg(++m_guestKeySequence));
    m_qmp.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
    m_qmp.flush();
}

void VmController::clearGuestInput()
{
    m_keyTimer.stop();
    m_pendingKeys.clear();
    m_guestPreviewPending = false;
}

bool VmController::sendGuestKey(const QString &key)
{
    if (!guestInputReady()) return false;
    // Map a small, explicit allow-list to QEMU's QKeyCode enum. The QML UI
    // never exposes arbitrary QMP or HMP commands.
    QStringList keys;
    const QString code = key.toLower().trimmed();
    const QStringList singleKeys = QStringList()
        << QStringLiteral("ret") << QStringLiteral("esc")
        << QStringLiteral("backspace") << QStringLiteral("tab")
        << QStringLiteral("spc") << QStringLiteral("up")
        << QStringLiteral("down") << QStringLiteral("left") << QStringLiteral("right")
        << QStringLiteral("home") << QStringLiteral("end")
        << QStringLiteral("pgup") << QStringLiteral("pgdn")
        << QStringLiteral("insert") << QStringLiteral("delete")
        << QStringLiteral("f1") << QStringLiteral("f2")
        << QStringLiteral("f3") << QStringLiteral("f4")
        << QStringLiteral("f5") << QStringLiteral("f6")
        << QStringLiteral("f7") << QStringLiteral("f8")
        << QStringLiteral("f9") << QStringLiteral("f10")
        << QStringLiteral("f11") << QStringLiteral("f12")
        << QStringLiteral("meta_l");
    if (singleKeys.contains(code)) keys << code;
    else if (code == QLatin1String("ctrl-c"))
        keys << QStringLiteral("ctrl") << QStringLiteral("c");
    else if (code == QLatin1String("ctrl-alt-delete"))
        keys << QStringLiteral("ctrl") << QStringLiteral("alt") << QStringLiteral("delete");
    else if (code == QLatin1String("alt-tab"))
        keys << QStringLiteral("alt") << QStringLiteral("tab");
    else if (code == QLatin1String("alt-f4"))
        keys << QStringLiteral("alt") << QStringLiteral("f4");
    else if (code == QLatin1String("ctrl-esc"))
        keys << QStringLiteral("ctrl") << QStringLiteral("esc");
    else if (code == QLatin1String("ctrl-v"))
        keys << QStringLiteral("ctrl") << QStringLiteral("v");
    else {
        setError(QStringLiteral("Unsupported guest key: %1").arg(key));
        return false;
    }
    return queueKeyStroke(keys);
}

bool VmController::sendGuestText(const QString &text, bool pressEnter)
{
    if (!guestInputReady()) return false;
    if (text.isEmpty() && !pressEnter) return true;
    if (text.size() > 128) {
        setError(QStringLiteral("Send a maximum of 128 ASCII characters at once."));
        return false;
    }
    // Validate the entire command before queuing anything, so a Unicode
    // character never results in sending only half of a command to FreeDOS.
    QList<QStringList> strokes;
    for (QChar character : text) {
        QStringList keys;
        if (!guestCharacterKeyCodes(character, keys)) {
            setError(QStringLiteral("Only ASCII US keyboard characters are supported for now (unsupported U+%1).")
                .arg(character.unicode(), 4, 16, QLatin1Char('0')));
            return false;
        }
        strokes.append(keys);
    }
    if (pressEnter) strokes.append(QStringList() << QStringLiteral("ret"));
    if (m_pendingKeys.size() + strokes.size() > 256) {
        setError(QStringLiteral("Guest input queue is full."));
        return false;
    }
    for (const QStringList &stroke : strokes)
        queueKeyStroke(stroke);
    return true;
}

QString VmController::guestPreviewUrl() const
{
    if (m_liveDisplayAvailable)
        return QUrl::fromLocalFile(m_livePngPath).toString()
            + QStringLiteral("?r=%1").arg(m_liveRevision);
    if (!m_guestPreviewAvailable) return QString();
    return QUrl::fromLocalFile(m_guestPreviewPngPath).toString()
        + QStringLiteral("?r=%1").arg(m_guestPreviewRevision);
}

void VmController::refreshGuestPreview()
{
    if (!m_running || !m_connected || m_qmp.state() != QLocalSocket::ConnectedState)
        return;
    // Once D-Bus frames arrive we no longer poll QMP screendump. The older
    // path remains available if the listener stops or yields no frames.
    if (m_dbusMode && m_liveDisplayAvailable) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    // Recover from a lost QMP response rather than permanently suppressing
    // previews. The guest's QMP commands are never allowed to queue forever.
    if (m_guestPreviewPending && now - m_guestPreviewRequestMs < 7000) return;
    if (m_guestPreviewPending) m_guestPreviewPending = false;
    m_guestPreviewPending = true;
    m_guestPreviewRequestMs = now;
    QJsonObject arguments;
    arguments.insert(QStringLiteral("filename"), m_guestPreviewPpmPath);
    QJsonObject command;
    command.insert(QStringLiteral("execute"), QStringLiteral("screendump"));
    command.insert(QStringLiteral("arguments"), arguments);
    command.insert(QStringLiteral("id"), QStringLiteral("guest-preview"));
    m_qmp.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
    m_qmp.flush();
}

void VmController::completeGuestPreview()
{
    QFileInfo info(m_guestPreviewPpmPath);
    if (!info.isFile() || info.size() < 12 || info.size() > 32 * 1024 * 1024) return;
    QImageReader reader(m_guestPreviewPpmPath, "ppm");
    const QImage screenshot = reader.read();
    if (screenshot.isNull()) {
        setError(QStringLiteral("Could not decode the QEMU PPM preview: %1").arg(reader.errorString()));
        return;
    }
    // One short-lived copy in the app cache, not a continuously running
    // remote-desktop protocol. M5 D-Bus mode has no separate SDL window.
    if (!screenshot.save(m_guestPreviewPngPath, "PNG")) {
        setError(QStringLiteral("Could not save the guest preview PNG."));
        return;
    }
    QFile::setPermissions(m_guestPreviewPngPath, QFile::ReadOwner | QFile::WriteOwner);
    QFile::remove(m_guestPreviewPpmPath);
    m_guestPreviewAvailable = true;
    ++m_guestPreviewRevision;
    emit guestPreviewChanged();
}

// Verify one uniquely named QEMU instance over both QMP and D-Bus.
// Unlike PIDs, this identity is the same across Sailjail PID namespaces.
// M6 then registers a separate GDBus framebuffer listener through an FD.
// QEMU sends frame updates to that listener, never through QMP screendump.
void VmController::setDbusStatus(const QString &message, const QString &consoles, bool ready)
{
    if (m_dbusStatus == message && m_dbusConsoles == consoles && m_dbusReady == ready)
        return;
    m_dbusStatus = message;
    m_dbusConsoles = consoles;
    m_dbusReady = ready;
    emit dbusChanged();
}

static QVariant dbusProperty(const QString &objectPath, const QString &interfaceName,
                             const QString &propertyName, QString *error)
{
    QDBusInterface properties(QStringLiteral("org.qemu"), objectPath,
                              QStringLiteral("org.freedesktop.DBus.Properties"),
                              QDBusConnection::sessionBus());
    QDBusReply<QDBusVariant> reply = properties.call(QStringLiteral("Get"), interfaceName, propertyName);
    if (!reply.isValid()) {
        if (error) *error = reply.error().message();
        return QVariant();
    }
    return reply.value().variant();
}

void VmController::refreshDbusDisplay()
{
    if (!m_dbusMode || !m_running || m_pid <= 0 || !m_connected) {
        setDbusStatus(QStringLiteral("No running M5 D-Bus guest connected through QMP."));
        return;
    }
    const QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || !bus.interface()) {
        setDbusStatus(QStringLiteral("D-Bus session bus is unavailable."));
        return;
    }
    if (!m_qmpNameConfirmed || m_expectedQemuName.isEmpty()) {
        setDbusStatus(QStringLiteral("Waiting for verified QMP VM identity. "
                                     "If this VM predates 0.5.0-4, shut it down and restart."));
        return;
    }
    const QString vmPath = QStringLiteral("/org/qemu/Display1/VM");
    const QString vmIface = QStringLiteral("org.qemu.Display1.VM");
    QString error;
    const QVariant name = dbusProperty(vmPath, vmIface, QStringLiteral("Name"), &error);
    if (!name.isValid()) {
        setDbusStatus(QStringLiteral("Cannot query D-Bus VM: %1").arg(error));
        return;
    }
    if (name.toString() != m_expectedQemuName) {
        setDbusStatus(QStringLiteral("D-Bus VM name does not match our verified QMP VM. "
                                     "Refusing guest input (other QEMU instance or sandbox issue)."));
        return;
    }
    const QVariant consoleIds = dbusProperty(vmPath, vmIface, QStringLiteral("ConsoleIDs"), &error);
    if (!consoleIds.isValid()) {
        setDbusStatus(QStringLiteral("Cannot read D-Bus consoles: %1").arg(error));
        return;
    }
    QList<uint> ids;
    if (consoleIds.userType() == qMetaTypeId<QDBusArgument>()) {
        ids = qdbus_cast<QList<uint> >(qvariant_cast<QDBusArgument>(consoleIds));
    } else if (consoleIds.canConvert<QVariantList>()) {
        for (const QVariant &id : consoleIds.toList()) ids.append(id.toUInt());
    } else {
        setDbusStatus(QStringLiteral("D-Bus ConsoleIDs returned an unknown data format."));
        return;
    }
    QStringList info;
    uint primaryId = ids.isEmpty() ? 0 : ids.first();
    bool foundGraphic = false;
    for (uint id : ids) {
        const QString path = QStringLiteral("/org/qemu/Display1/Console_%1").arg(id);
        const QString iface = QStringLiteral("org.qemu.Display1.Console");
        const QVariant label = dbusProperty(path, iface, QStringLiteral("Label"), &error);
        const QVariant type = dbusProperty(path, iface, QStringLiteral("Type"), &error);
        const QVariant width = dbusProperty(path, iface, QStringLiteral("Width"), &error);
        const QVariant height = dbusProperty(path, iface, QStringLiteral("Height"), &error);
        if (!type.isValid() || !width.isValid() || !height.isValid()) {
            setDbusStatus(QStringLiteral("Cannot query console %1: %2").arg(id).arg(error));
            return;
        }
        if (type.toString() == QLatin1String("Graphic") && !foundGraphic) {
            primaryId = id;
            foundGraphic = true;
        }
        info << QStringLiteral("Console %1: %2 (%3), %4 × %5")
                    .arg(id)
                    .arg(label.isValid() ? label.toString() : QStringLiteral("Unnamed"))
                    .arg(type.toString())
                    .arg(width.toUInt()).arg(height.toUInt());
    }
    const QString vmName = name.toString().isEmpty() ? QStringLiteral("Unnamed VM") : name.toString();
    setDbusStatus(QStringLiteral("Verified QMP/D-Bus identity: %1; %2 console(s).")
                  .arg(vmName).arg(ids.size()), info.join(QStringLiteral("\n")), true);
    if (!ids.isEmpty()) {
        refreshDbusMouse(primaryId);
        launchLiveDisplay(primaryId);
    }
}

void VmController::setLiveDisplayStatus(const QString &message)
{
    if (m_liveDisplayStatus == message) return;
    m_liveDisplayStatus = message;
    emit liveDisplayChanged();
}

void VmController::stopLiveDisplay()
{
    if (m_liveHelper.state() != QProcess::NotRunning) {
        m_liveHelper.terminate();
        if (!m_liveHelper.waitForFinished(350)) {
            m_liveHelper.kill();
            m_liveHelper.waitForFinished(350);
        }
    }
    if (m_liveHelper.isOpen()) m_liveHelper.readAllStandardOutput();
    QFile::remove(m_livePngPath);
    QFile::remove(m_guestCursorPngPath);
    QFile::remove(m_guestCursorStatePath);
    m_liveRecentLog.clear();
    m_liveLastModified = 0;
    m_liveLastFrameMs = 0;
    m_liveLastAttempt = 0;
    m_liveSampleStart = 0;
    m_liveSampleFrames = 0;
    m_liveObservedHz = 0;
    if (m_liveDisplayAvailable) {
        m_liveDisplayAvailable = false;
        emit guestPreviewChanged();
    }
    setLiveDisplayStatus(QStringLiteral("Live D-Bus listener not started"));
}

void VmController::launchLiveDisplay(uint consoleId)
{
    if (!m_dbusMode || !m_running || !m_dbusReady || m_pid <= 0) return;
    if (m_liveHelper.state() != QProcess::NotRunning) return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_liveLastAttempt < 6000) return;
    m_liveLastAttempt = now;
    const QString helper = QStringLiteral("/usr/bin/harbour-sailqemu-display-listener");
    if (!QFileInfo(helper).isExecutable()) {
        setLiveDisplayStatus(QStringLiteral("Missing M6 display listener executable"));
        return;
    }
    QFile::remove(m_livePngPath);
    m_liveDisplayAvailable = false;
    m_liveLastModified = 0;
    m_liveLastFrameMs = 0;
    m_liveSampleStart = 0;
    m_liveSampleFrames = 0;
    m_liveObservedHz = 0;
    m_liveRecentLog.clear();
    m_liveHelper.start(helper, QStringList()
                       << m_expectedQemuName
                       << QString::number(consoleId)
                       << m_livePngPath);
    setLiveDisplayStatus(QStringLiteral("Connecting D-Bus framebuffer listener (console %1)...").arg(consoleId));
}

void VmController::updateLiveDisplay()
{
    if (!m_running || !m_dbusMode) return;
    updateGuestCursor();
    if (m_liveHelper.state() == QProcess::NotRunning) {
        if (m_liveDisplayAvailable) {
            m_liveDisplayAvailable = false;
            emit guestPreviewChanged();
        }
        if (m_liveLastAttempt) {
            const QString reason = m_liveRecentLog.trimmed().isEmpty()
                ? m_liveHelper.errorString() : m_liveRecentLog.trimmed();
            setLiveDisplayStatus(QStringLiteral("D-Bus frame listener stopped: %1 (QMP fallback)")
                                 .arg(reason.left(240)));
        }
        return;
    }
    const QFileInfo frame(m_livePngPath);
    if (!frame.isFile() || frame.size() < 60) {
        if (!m_liveDisplayAvailable && m_liveLastAttempt &&
            QDateTime::currentMSecsSinceEpoch() - m_liveLastAttempt > 4500) {
            const QString hint = m_liveRecentLog.trimmed().isEmpty()
                ? QStringLiteral("waiting for a supported 32-bit scanout")
                : m_liveRecentLog.trimmed().right(160);
            setLiveDisplayStatus(QStringLiteral("No D-Bus frame yet (%1); QMP preview fallback")
                                 .arg(hint));
        }
        if (m_liveDisplayAvailable) {
            m_liveDisplayAvailable = false;
            emit guestPreviewChanged();
        }
        return;
    }
    const qint64 modified = frame.lastModified().toMSecsSinceEpoch();
    if (modified == m_liveLastModified) return;
    m_liveLastModified = modified;
    m_liveDisplayAvailable = true;
    ++m_liveRevision;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    m_liveLastFrameMs = now;
    if (!m_liveSampleStart) m_liveSampleStart = now;
    ++m_liveSampleFrames;
    m_liveLastFrameKiB = frame.size() / 1024;
    // 'updates/s' is measured from observed atomic file replacements,
    // not asserted to equal actual Qt Quick GPU presentation FPS.
    if (now - m_liveSampleStart >= 2500) {
        m_liveObservedHz = int((1000LL * m_liveSampleFrames +
                                  (now - m_liveSampleStart) / 2) /
                                 (now - m_liveSampleStart));
        m_liveSampleFrames = 0;
        m_liveSampleStart = now;
        setLiveDisplayStatus(QStringLiteral("Live D-Bus: %1 updates/s; last frame %2 KiB (QMP fallback idle)")
                             .arg(m_liveObservedHz).arg(m_liveLastFrameKiB));
    } else if (m_liveRevision == 1) {
        setLiveDisplayStatus(QStringLiteral("Live D-Bus: receiving frames (QMP fallback idle)"));
    }
    emit guestPreviewChanged();
}


// QEMU exposes Mouse on the same verified graphical Console object as
// RegisterListener. A USB tablet provides absolute pointer positioning;
// if only a relative mouse exists, we translate consecutive image deltas.
void VmController::resetMouse()
{
    const bool changed = m_mouseAvailable || m_mouseAbsolute ||
                         m_mouseStatus != QLatin1String("Mouse not connected");
    m_mouseAvailable = false;
    m_mouseAbsolute = false;
    m_mouseStatus = QStringLiteral("Mouse not connected");
    m_mouseConsoleId = 0;
    m_mouseConsoleWidth = 0;
    m_mouseConsoleHeight = 0;
    m_mouseInputDiagnostic = QStringLiteral("No guest mouse input sent");
    m_mouseHasLast = false;
    if (changed) emit mouseChanged();
    m_guestCursorVisible = false;
    m_guestCursorImageAvailable = false;
    m_guestCursorStateModified = 0;
    m_guestCursorImageModified = 0;
    emit guestCursorChanged();
}

void VmController::refreshDbusMouse(uint consoleId)
{
    if (!m_running || !m_dbusMode || !m_dbusReady || !m_connected) return;
    const QString path = QStringLiteral("/org/qemu/Display1/Console_%1").arg(consoleId);
    QString error;
    const QVariant absolute = dbusProperty(path, QStringLiteral("org.qemu.Display1.Mouse"),
                                          QStringLiteral("IsAbsolute"), &error);
    m_mouseConsoleId = consoleId;
    m_mouseHasLast = false;
    m_mouseAvailable = absolute.isValid();
    m_mouseAbsolute = m_mouseAvailable && absolute.toBool();
    refreshMouseGeometry();
    m_mouseStatus = !m_mouseAvailable
        ? QStringLiteral("QEMU mouse interface unavailable: %1").arg(error)
        : m_mouseAbsolute ? QStringLiteral("Mouse ready: absolute (USB tablet)")
                          : QStringLiteral("Mouse ready: relative movement");
    emit mouseChanged();
}

// The display framebuffer and the input console normally share a size, but
// graphics mode changes can temporarily desynchronise them. SetAbsPosition
// specifically uses the *current Console.Width / Console.Height* in QEMU.
void VmController::refreshMouseGeometry()
{
    if (!m_running || !m_dbusMode || !m_dbusReady || !m_mouseAvailable) return;
    const QString path = QStringLiteral("/org/qemu/Display1/Console_%1").arg(m_mouseConsoleId);
    const QString iface = QStringLiteral("org.qemu.Display1.Console");
    const QVariant w = dbusProperty(path, iface, QStringLiteral("Width"), nullptr);
    const QVariant h = dbusProperty(path, iface, QStringLiteral("Height"), nullptr);
    // Input mode may change when Windows loads a different mouse driver.
    const QVariant absolute = dbusProperty(path, QStringLiteral("org.qemu.Display1.Mouse"),
                                           QStringLiteral("IsAbsolute"), nullptr);
    if (absolute.isValid() && m_mouseAbsolute != absolute.toBool()) {
        m_mouseAbsolute = absolute.toBool();
        m_mouseHasLast = false;
        m_mouseStatus = m_mouseAbsolute ? QStringLiteral("Mouse ready: absolute (USB tablet)")
                                        : QStringLiteral("Mouse ready: relative movement");
        emit mouseChanged();
    }
    if (!w.isValid() || !h.isValid()) return;
    const int width = w.toInt(), height = h.toInt();
    if (width < 1 || width > 8192 || height < 1 || height > 8192) return;
    if (width != m_mouseConsoleWidth || height != m_mouseConsoleHeight) {
        m_mouseConsoleWidth = width;
        m_mouseConsoleHeight = height;
        emit mouseChanged();
    }
}

bool VmController::mouseCall(const QString &method, const QVariantList &args)
{
    if (!m_running || !m_connected || m_paused || !m_dbusMode || !m_dbusReady || !m_mouseAvailable)
        return false;
    QDBusInterface pointer(QStringLiteral("org.qemu"),
        QStringLiteral("/org/qemu/Display1/Console_%1").arg(m_mouseConsoleId),
        QStringLiteral("org.qemu.Display1.Mouse"), QDBusConnection::sessionBus());
    if (!pointer.isValid()) {
        setError(QStringLiteral("Guest mouse disconnected: %1").arg(pointer.lastError().message()));
        return false;
    }
    // Avoid blocking the QML rendering thread on rapid motion events.
    QDBusPendingCall pending = pointer.asyncCallWithArgumentList(method, args);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this, method](QDBusPendingCallWatcher *finished) {
        if (finished->isError()) {
            const QString message = QStringLiteral("QEMU mouse %1 rejected: %2")
                .arg(method, finished->error().message());
            m_mouseInputDiagnostic = message;
            emit mouseChanged();
            setError(message);
        }
        finished->deleteLater();
    });
    return true;
}

bool VmController::guestMouseMove(int x, int y)
{
    if (!m_mouseAvailable || !m_dbusReady || !m_running || m_paused || !m_connected)
        return false;
    x = qBound(0, x, 8191);
    y = qBound(0, y, 8191);
    if (m_mouseAbsolute) {
        // Keep the actual sent coordinates visible for on-device comparison
        // against the cursor event forwarded by QEMU's display listener.
        const QString diagnostic = QStringLiteral("Sent mouse: %1, %2; console %3 × %4")
            .arg(x).arg(y).arg(m_mouseConsoleWidth).arg(m_mouseConsoleHeight);
        if (m_mouseInputDiagnostic != diagnostic) {
            m_mouseInputDiagnostic = diagnostic;
            emit mouseChanged();
        }
        return mouseCall(QStringLiteral("SetAbsPosition"), QVariantList() << QVariant(uint(x)) << QVariant(uint(y)));
    }
    // Avoid accidental relative moves from callers that supply absolute screen
    // coordinates. Use guestMouseRelative(dx,dy) for finger-swipe deltas.
    return false;
}

// Direct relative PS/2 motion: do not treat the next tap location as an
// absolute guest position. The guest mouse driver handles motion/acceleration.
bool VmController::guestMouseRelative(int dx, int dy)
{
    if (!m_mouseAvailable || m_mouseAbsolute || !m_dbusReady ||
        !m_running || m_paused || !m_connected)
        return false;
    dx = qBound(-512, dx, 512);
    dy = qBound(-512, dy, 512);
    if (dx == 0 && dy == 0) return true;
    m_mouseInputDiagnostic = QStringLiteral("PS/2 relative movement: dx=%1, dy=%2")
        .arg(dx).arg(dy);
    emit mouseChanged();
    return mouseCall(QStringLiteral("RelMotion"), QVariantList() << dx << dy);
}

bool VmController::guestMouseButton(const QString &button, bool pressed)
{
    uint id = 0;
    if (button == QLatin1String("left")) id = 0;
    else if (button == QLatin1String("middle")) id = 1;
    else if (button == QLatin1String("right")) id = 2;
    else if (button == QLatin1String("wheel-up")) id = 3;
    else if (button == QLatin1String("wheel-down")) id = 4;
    else return false;
    return mouseCall(pressed ? QStringLiteral("Press") : QStringLiteral("Release"),
                     QVariantList() << QVariant(id));
}

bool VmController::guestMouseClick(int x, int y, const QString &button)
{
    // For PS/2/relative guests, a click is at the CURRENT guest cursor.
    // Sending a mouse move to (x,y) here wrongly uses touch coordinates as
    // consecutive deltas, so the guest clicks elsewhere (notably Win95).
    if (m_mouseAbsolute && !guestMouseMove(x, y)) return false;
    if (!guestMouseButton(button, true)) return false;
    return guestMouseButton(button, false);
}

bool VmController::guestMouseWheel(bool up)
{
    const QString wheel = up ? QStringLiteral("wheel-up") : QStringLiteral("wheel-down");
    if (!guestMouseButton(wheel, true)) return false;
    return guestMouseButton(wheel, false);
}

QString VmController::guestCursorUrl() const
{
    if (!m_guestCursorImageAvailable) return QString();
    return QUrl::fromLocalFile(m_guestCursorPngPath).toString()
        + QStringLiteral("?rev=%1").arg(m_guestCursorRevision);
}

void VmController::updateGuestCursor()
{
    const QFileInfo stateFile(m_guestCursorStatePath);
    const QFileInfo imageFile(m_guestCursorPngPath);
    bool changed = false;
    if (imageFile.exists() && imageFile.size() > 50) {
        const qint64 modified = imageFile.lastModified().toMSecsSinceEpoch();
        if (!m_guestCursorImageAvailable || modified != m_guestCursorImageModified) {
            m_guestCursorImageModified = modified;
            m_guestCursorImageAvailable = true;
            ++m_guestCursorRevision;
            changed = true;
        }
    } else if (m_guestCursorImageAvailable) {
        m_guestCursorImageAvailable = false;
        changed = true;
    }
    if (stateFile.exists()) {
        const qint64 modified = stateFile.lastModified().toMSecsSinceEpoch();
        if (modified != m_guestCursorStateModified) {
            m_guestCursorStateModified = modified;
            QFile f(m_guestCursorStatePath);
            if (f.open(QIODevice::ReadOnly)) {
                const QList<QByteArray> parts = f.readAll().trimmed().split(' ');
                if (parts.size() == 5) {
                    bool ok[5];
                    int n[5];
                    for (int i = 0; i < 5; ++i) n[i] = parts[i].toInt(&ok[i]);
                    if (ok[0] && ok[1] && ok[2] && ok[3] && ok[4]) {
                        m_guestCursorX = n[0]; m_guestCursorY = n[1];
                        m_guestCursorVisible = n[2] != 0;
                        m_guestCursorHotX = n[3]; m_guestCursorHotY = n[4];
                        changed = true;
                    }
                }
            }
        }
    }
    if (changed) emit guestCursorChanged();
}

void VmController::pauseVm() { sendQmp(QStringLiteral("stop")); }
void VmController::resumeVm() { sendQmp(QStringLiteral("cont")); }
void VmController::requestShutdown()
{
    if (!m_running || !m_connected) {
        setError(QStringLiteral("Cannot request shutdown: QMP is disconnected."));
        return;
    }
    // system_powerdown simulates an ACPI power button; QEMU explicitly does
    // not guarantee that the guest implements or responds to it (notably DOS).
    // Do not silently issue QMP quit: hard poweroff can corrupt guest disks.
    sendQmp(QStringLiteral("system_powerdown"));
    m_shutdownPending = true;
    m_shutdownStatus = QStringLiteral("ACPI shutdown requested. DOS/older guests may ignore it.");
    setStatus(m_shutdownStatus);
    const qint64 requestedPid = m_pid;
    QTimer::singleShot(12000, this, [this, requestedPid]() {
        if (m_running && m_connected && m_pid == requestedPid && m_shutdownPending) {
            m_shutdownStatus = QStringLiteral("Guest did not shut down after 12 s. "
                                             "Exit the guest OS if possible, or use Force off (unsafe). ");
            setStatus(m_shutdownStatus);
        }
    });
}

void VmController::resyncGuestDisplay()
{
    if (!m_running || !m_connected) return;
    if (!m_dbusMode) {
        refreshGuestPreview();
        return;
    }
    // Re-register a listener to obtain a fresh complete scanout (rather than
    // retaining an earlier placeholder or a partial update). Do not touch
    // the emulator process, disks, input devices, or guest OS state.
    stopLiveDisplay();
    refreshDbusDisplay();
    refreshGuestPreview();
    setStatus(QStringLiteral("Display listener resynchronised. If QEMU's startup "
                             "message remains, the guest may not have initialised "
                             "its video adapter."));
}
void VmController::forceQuit()
{
    // Only label a subsequent exit as user-requested when QMP can receive quit.
    if (m_connected && m_qmp.state() == QLocalSocket::ConnectedState) {
        m_forceQuitRequested = true;
        sendQmp(QStringLiteral("quit"));
    } else if (m_pid > 0 && processExists()) {
        // The user explicitly confirmed Force off in the guest-console
        // remorse popup. If QMP is unavailable (e.g. sandboxed launch), a
        // verified QEMU process can still be terminated without the shell.
        // This is an unsafe power cut; never use it for ACPI shutdown.
        if (::kill(static_cast<pid_t>(m_pid), SIGTERM) == 0) {
            m_forceQuitRequested = true;
            setStatus(QStringLiteral("Force off sent to verified QEMU PID (SIGTERM; unsafe)."));
        } else {
            const int code = errno;
            setError(QStringLiteral("Cannot force off QEMU PID %1: %2 (errno %3).")
                     .arg(m_pid).arg(QString::fromLocal8Bit(std::strerror(code))).arg(code));
        }
    } else {
        setError(QStringLiteral("Cannot force off: QMP disconnected and no verified QEMU process exists."));
    }
}
void VmController::refresh() { poll(); }

void VmController::poll()
{
    ++m_tick;
    if (m_pid <= 0) return;
    if (!processExists()) {
        QString outcome;
        if (m_forceQuitRequested)
            outcome = QStringLiteral("QEMU stopped after Force off.");
        else if (m_seenGuestShutdown)
            outcome = QStringLiteral("Guest shut down; QEMU exited.");
        else if (!m_hadQmpConnection)
            outcome = QStringLiteral("QEMU exited before QMP connected. Check Diagnostics / QEMU log.");
        else
            outcome = QStringLiteral("QEMU exited without a shutdown event. Check Diagnostics / QEMU log.");
        resetStopped(outcome);
        return;
    }
    // A running QEMU should expose QMP promptly. Keep retrying without
    // force-killing the guest or repeatedly overwriting the status message.
    if (!m_connected && !m_qmpConnectionWarned && m_qmpConnectionStartMs > 0 &&
        QDateTime::currentMSecsSinceEpoch() - m_qmpConnectionStartMs >= 15000) {
        m_qmpConnectionWarned = true;
        setStatus(QStringLiteral("QEMU process is alive, but QMP is not connected after 15 seconds. "
                                 "Controls may be unavailable; inspect Diagnostics and QEMU log."));
    }
    if (m_qmp.state() == QLocalSocket::UnconnectedState) {
        m_qmp.connectToServer(socketFile());
        return;
    }
    if (m_connected && m_tick % 3 == 0)
        sendQmp(QStringLiteral("query-status"));
    if (m_connected && m_dbusMode && !m_dbusReady && m_tick % 4 == 0)
        refreshDbusDisplay();
    // Windows mode switches can change the console geometry after discovery.
    if (m_connected && m_dbusReady && m_mouseAvailable && m_tick % 2 == 0)
        refreshMouseGeometry();
}
