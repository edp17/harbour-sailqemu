#include "diskmanager.h"
#include "vmcontroller.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTimer>

DiskManager::DiskManager(VmController *vm, QObject *parent) : QObject(parent), m_vm(vm)
{
    m_directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                  + QStringLiteral("/SailQEMU/disks");
    QDir().mkpath(m_directory);
    m_process.setProcessChannelMode(QProcess::MergedChannels);
    connect(&m_process, &QProcess::readyRead, this, [this]() {
        m_output += QString::fromLocal8Bit(m_process.readAll());
        if (m_output.size() > 16000) m_output = m_output.right(16000);
        emit changed();
    });
    connect(&m_process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(
                &QProcess::finished), this, &DiskManager::finish);
}

QString DiskManager::expanded(const QString &path) const
{
    const QString trimmed = path.trimmed();
    if (trimmed == QLatin1String("~")) return QDir::homePath();
    if (trimmed.startsWith(QLatin1String("~/"))) return QDir::homePath() + trimmed.mid(1);
    return trimmed;
}

void DiskManager::setSourcePath(const QString &value)
{
    if (m_sourcePath == value) return;
    m_sourcePath = value;
    emit changed();
}

QString DiskManager::imageTool(const QString &qemuExecutable) const
{
    const QFileInfo emulator(expanded(qemuExecutable));
    const QString img = emulator.absoluteDir().filePath(QStringLiteral("qemu-img"));
    return QFileInfo(img).isExecutable() ? img : QString();
}

void DiskManager::fail(const QString &why)
{
    m_resultPath.clear();
    m_resultFormat.clear();
    m_status = why;
    m_report = why;
    emit changed();
    emit operationCompleted();
}

bool DiskManager::begin(Job job, const QString &binary, const QStringList &arguments)
{
    if (busy()) { fail(QStringLiteral("An image operation is already running.")); return false; }
    if (m_vm->running()) { fail(QStringLiteral("Stop the VM before working with disk images.")); return false; }
    if (binary.isEmpty()) { fail(QStringLiteral("qemu-img not found beside the selected QEMU executable.")); return false; }
    m_cancelRequested = false;
    m_output.clear();
    m_report.clear();
    m_resultPath.clear();
    m_resultFormat.clear();
    m_job = job;
    m_status = job == Inspect ? QStringLiteral("Inspecting image...")
               : job == Create ? QStringLiteral("Creating image...")
               : QStringLiteral("Copying/converting image...");
    m_process.start(binary, arguments, QIODevice::ReadOnly);
    if (!m_process.waitForStarted(1500)) {
        m_job = None;
        m_stage.reset();
        fail(QStringLiteral("Cannot start qemu-img: %1").arg(m_process.errorString()));
        return false;
    }
    emit changed();
    return true;
}

bool DiskManager::prepareOutput(const QString &destination, const QString &format)
{
    m_destination.clear();
    m_stagedPath.clear();
    m_stage.reset();
    if (format != QLatin1String("raw") && format != QLatin1String("qcow2")) {
        fail(QStringLiteral("New images must use raw or qcow2 format."));
        return false;
    }
    const QString output = expanded(destination);
    const QFileInfo finalInfo(output);
    if (!finalInfo.isAbsolute() || finalInfo.fileName().isEmpty() ||
        finalInfo.fileName() == QLatin1String(".") ||
        finalInfo.fileName() == QLatin1String("..")) {
        fail(QStringLiteral("Enter an absolute destination filename."));
        return false;
    }
    if (finalInfo.exists() || finalInfo.isSymLink()) {
        fail(QStringLiteral("Destination already exists; no file was changed: %1").arg(output));
        return false;
    }
    const QDir parent = finalInfo.absoluteDir();
    if (!parent.exists()) {
        fail(QStringLiteral("Destination folder does not exist: %1").arg(parent.absolutePath()));
        return false;
    }
    if (!QFileInfo(parent.absolutePath()).isWritable()) {
        fail(QStringLiteral("Destination folder is not writable."));
        return false;
    }
    // Place the staging directory on the destination filesystem so QFile::rename
    // can publish atomically. QTemporaryDir always cleans an interrupted job.
    m_stage.reset(new QTemporaryDir(parent.filePath(QStringLiteral(".sailqemu-image-XXXXXX"))));
    if (!m_stage->isValid()) {
        m_stage.reset();
        fail(QStringLiteral("Cannot reserve staging directory in destination folder."));
        return false;
    }
    // QTemporaryDir has path(), but not filePath() in Sailfish Qt 5.6.
    // QDir::filePath() joins the directory with the staging filename.
    m_stagedPath = QDir(m_stage->path()).filePath(QStringLiteral("new-image"));
    m_destination = output;
    m_pendingFormat = format;
    return true;
}

bool DiskManager::inspectImage(const QString &qemuExecutable, const QString &path)
{
    if (busy()) return false;
    const QString file = expanded(path);
    if (!QFileInfo(file).isFile() || !QFileInfo(file).isReadable()) {
        fail(QStringLiteral("Image is missing or unreadable: %1").arg(file)); return false;
    }
    m_sourcePath = file;
    return begin(Inspect, imageTool(qemuExecutable),
                 QStringList() << QStringLiteral("info") << QStringLiteral("--output=json") << file);
}

bool DiskManager::createImage(const QString &qemuExecutable, const QString &destination,
                              const QString &format, int sizeMiB)
{
    if (busy()) return false;
    if (m_vm->running()) { fail(QStringLiteral("Stop the VM before creating disks.")); return false; }
    if (sizeMiB < 16 || sizeMiB > 131072) {
        fail(QStringLiteral("Disk size must be 16–131072 MiB.")); return false;
    }
    if (!prepareOutput(destination, format)) return false;
    return begin(Create, imageTool(qemuExecutable), QStringList()
                 << QStringLiteral("create") << QStringLiteral("-f") << format
                 << m_stagedPath << QString::number(sizeMiB) + QLatin1String("M"));
}

bool DiskManager::copyImage(const QString &qemuExecutable, const QString &source,
                            const QString &destination, const QString &format)
{
    if (busy()) return false;
    if (m_vm->running()) { fail(QStringLiteral("Stop the VM before copying disks.")); return false; }
    const QString src = expanded(source);
    if (!QFileInfo(src).isFile() || !QFileInfo(src).isReadable()) {
        fail(QStringLiteral("Source is missing or unreadable: %1").arg(src)); return false;
    }
    if (!prepareOutput(destination, format)) return false;
    if (QFileInfo(src).canonicalFilePath() == QFileInfo(m_destination).absoluteFilePath()) {
        m_stage.reset(); fail(QStringLiteral("Source and destination must differ.")); return false;
    }
    m_sourcePath = src;
    // Source is opened read-only; qemu-img auto-detects QCOW1, QCOW2, RAW,
    // VDI and VMDK. The conversion writes only to a NEW staged destination.
    return begin(Convert, imageTool(qemuExecutable), QStringList()
                 << QStringLiteral("convert") << QStringLiteral("-p")
                 << QStringLiteral("-O") << format << src << m_stagedPath);
}

void DiskManager::cancel()
{
    if (!busy()) return;
    m_cancelRequested = true;
    m_status = QStringLiteral("Cancelling image operation...");
    emit changed();
    if (m_process.state() != QProcess::NotRunning) m_process.kill();
    // QProcess::finished handles cleanup asynchronously; never block Qt Quick.
}

void DiskManager::finish(int code, QProcess::ExitStatus result)
{
    const Job completed = m_job;
    if (completed == None) return;
    m_output += QString::fromLocal8Bit(m_process.readAll());
    m_job = None;
    const bool ok = !m_cancelRequested && result == QProcess::NormalExit && code == 0;
    if (ok && completed == Inspect) {
        const QJsonDocument doc = QJsonDocument::fromJson(m_output.toUtf8());
        if (!doc.isObject()) {
            m_report = QStringLiteral("qemu-img returned an invalid image report.\n") + m_output.right(1000);
            m_status = QStringLiteral("Inspect failed");
        } else {
            const QJsonObject v = doc.object();
            m_report = QStringLiteral("Path: %1\nFormat: %2\nVirtual size: %3 MiB\n"
                                      "Allocated size: %4 MiB")
                    .arg(m_sourcePath, v.value(QStringLiteral("format")).toString())
                    .arg(v.value(QStringLiteral("virtual-size")).toDouble() / 1048576.0, 0, 'f', 1)
                    .arg(v.value(QStringLiteral("actual-size")).toDouble() / 1048576.0, 0, 'f', 1);
            const QString backing = v.value(QStringLiteral("backing-filename")).toString();
            if (!backing.isEmpty())
                m_report += QStringLiteral("\nBacking image: %1\nKeep the backing image; this image depends on it.").arg(backing);
            m_report += QStringLiteral("\n") + m_vm->storageReferences(m_sourcePath);
            m_status = QStringLiteral("Inspection completed");
        }
    } else if (ok && (completed == Create || completed == Convert)) {
        // QFile::rename refuses to replace an existing file. Never overwrite
        // existing VM disks, including if a file appeared while converting.
        if (QFile::exists(m_destination) || QFileInfo(m_destination).isSymLink() ||
            !QFile::rename(m_stagedPath, m_destination)) {
            m_status = QStringLiteral("Destination appeared or could not be written. Existing files untouched.");
            m_report = m_status;
        } else {
            m_resultPath = m_destination;
            m_resultFormat = m_pendingFormat;
            m_status = completed == Create ? QStringLiteral("Image created") : QStringLiteral("Image copy ready");
            m_report = QStringLiteral("Saved: %1\nFormat: %2\n\nOriginal source was not changed.")
                           .arg(m_resultPath, m_resultFormat);
        }
    } else {
        m_status = m_cancelRequested
                   ? QStringLiteral("Operation cancelled; incomplete image discarded.")
                   : QStringLiteral("qemu-img failed; no output image published.");
        m_report = m_status + QLatin1String("\n") + m_output.right(2000);
    }
    m_cancelRequested = false;
    m_stage.reset();
    m_destination.clear();
    m_stagedPath.clear();
    emit changed();
    emit operationCompleted();
}
