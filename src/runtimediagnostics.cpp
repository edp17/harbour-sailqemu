#include "runtimediagnostics.h"
#include "vmcontroller.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStringList>
#include <unistd.h>

RuntimeDiagnostics::RuntimeDiagnostics(VmController *vm, QObject *parent)
    : QObject(parent), m_vm(vm)
{
    m_timer.setInterval(2500);
    connect(&m_timer, &QTimer::timeout, this, &RuntimeDiagnostics::refresh);
    m_timer.start();
    refresh();
}

bool RuntimeDiagnostics::readProcCpu(qint64 pid, quint64 *ticks)
{
    if (pid <= 0) return false;
    QFile file(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!file.open(QIODevice::ReadOnly)) return false;
    const QByteArray row = file.readLine();
    // comm is surrounded by parentheses and may itself contain spaces or ')'.
    // Fields after the LAST ')' start at Linux /proc/stat field 3 (state).
    const int close = row.lastIndexOf(')');
    if (close < 0) return false;
    const QList<QByteArray> fields = row.mid(close + 1).simplified().split(' ');
    if (fields.size() <= 12) return false;
    bool userOk = false;
    bool systemOk = false;
    const quint64 user = fields.at(11).toULongLong(&userOk);   // field 14: utime
    const quint64 system = fields.at(12).toULongLong(&systemOk); // field 15: stime
    if (!userOk || !systemOk) return false;
    *ticks = user + system;
    return true;
}

qint64 RuntimeDiagnostics::readProcRssKiB(qint64 pid)
{
    if (pid <= 0) return -1;
    QFile file(QStringLiteral("/proc/%1/statm").arg(pid));
    if (!file.open(QIODevice::ReadOnly)) return -1;
    const QList<QByteArray> fields = file.readLine().simplified().split(' ');
    if (fields.size() < 2) return -1;
    bool ok = false;
    const qint64 residentPages = fields.at(1).toLongLong(&ok);
    const long pageSize = ::sysconf(_SC_PAGESIZE);
    if (!ok || residentPages < 0 || pageSize <= 0) return -1;
    return (residentPages * pageSize) / 1024;
}

double RuntimeDiagnostics::sampleCpu(qint64 pid, CpuSample *previous, qint64 nowMs)
{
    quint64 ticks = 0;
    if (!readProcCpu(pid, &ticks)) {
        *previous = CpuSample();
        return -1.0;
    }
    double measured = -1.0;
    const long frequency = ::sysconf(_SC_CLK_TCK);
    if (previous->pid == pid && previous->timeMs > 0 &&
        nowMs > previous->timeMs && ticks >= previous->ticks && frequency > 0) {
        const qint64 elapsed = nowMs - previous->timeMs;
        // 100% is one fully busy CPU core; multiple QEMU threads can exceed it.
        measured = double(ticks - previous->ticks) * 100000.0 /
                   (double(frequency) * double(elapsed));
    }
    previous->pid = pid;
    previous->ticks = ticks;
    previous->timeMs = nowMs;
    previous->lastPercent = measured;
    return measured;
}

QString RuntimeDiagnostics::formattedCpu(double percent)
{
    return percent < 0 ? QStringLiteral("sampling...")
                       : QString::number(percent, 'f', 1) + QLatin1String("% of one core");
}

QString RuntimeDiagnostics::formattedMemory(qint64 rssKiB)
{
    return rssKiB < 0 ? QStringLiteral("unavailable")
                      : QString::number(double(rssKiB) / 1024.0, 'f', 1) + QLatin1String(" MiB RSS");
}

void RuntimeDiagnostics::copyReport()
{
    QClipboard *clipboard = QGuiApplication::clipboard();
    if (clipboard) clipboard->setText(m_report);
}

// Read only a bounded tail; QEMU logs can be very large if debugging is enabled.
static QString recentQemuLog(const QString &logPath)
{
    QFile file(logPath);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("No QEMU log available yet.");
    const qint64 length = file.size();
    if (length == 0) return QStringLiteral("QEMU log is empty (this can be normal).");
    if (length > 4096 && !file.seek(length - 4096))
        return QStringLiteral("Could not seek to the end of the QEMU log.");
    QString result = QString::fromLocal8Bit(file.read(4096));
    // Avoid showing an incomplete first line when reading from the file's tail.
    if (length > 4096 && result.contains(QLatin1Char('\n')))
        result = result.mid(result.indexOf(QLatin1Char('\n')) + 1);
    result.remove(QLatin1Char('\0'));
    result.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    if (result.size() > 2500) result = result.right(2500);
    return result.trimmed().isEmpty() ? QStringLiteral("QEMU log contains no readable text.")
                                       : result.trimmed();
}

void RuntimeDiagnostics::refresh()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 selfPid = ::getpid();
    const qint64 qemuPid = m_vm->qemuPid();
    const double frontendCpu = sampleCpu(selfPid, &m_frontend, now);
    const double emulatorCpu = m_vm->running()
                               ? sampleCpu(qemuPid, &m_emulator, now) : -1.0;
    if (!m_vm->running()) m_emulator = CpuSample();

    QString executable = m_vm->qemuPath().trimmed();
    if (executable == QLatin1String("~")) executable = QDir::homePath();
    else if (executable.startsWith(QLatin1String("~/")))
        executable = QDir::homePath() + executable.mid(1);
    executable = QDir::cleanPath(executable);
    const QFileInfo qemu(executable);
    const QFileInfo img(qemu.absoluteDir().filePath(QStringLiteral("qemu-img")));
    const QFileInfo listener(QStringLiteral("/usr/bin/harbour-sailqemu-display-listener"));
    const QFileInfo runner(QStringLiteral("/usr/share/harbour-sailqemu/bin/sailqemu-runner.sh"));
    QStringList lines;
    lines << QStringLiteral("Read-only runtime diagnostics (updates every 2.5 s)")
          << QStringLiteral("CPU: 100% means one fully occupied CPU core; this is NOT total device utilisation.")
          << QString()
          << QStringLiteral("Frontend: %1; %2")
                 .arg(formattedCpu(frontendCpu), formattedMemory(readProcRssKiB(selfPid)));
    if (m_vm->running()) {
        lines << QStringLiteral("QEMU (PID %1): %2; %3")
                     .arg(qemuPid)
                     .arg(formattedCpu(emulatorCpu), formattedMemory(readProcRssKiB(qemuPid)))
              << QStringLiteral("Guest: %1 (%2)")
                     .arg(m_vm->activeProfileName(), m_vm->paused() ? QStringLiteral("paused") : QStringLiteral("running"))
              << QStringLiteral("QMP: %1")
                     .arg(m_vm->connected() ? QStringLiteral("connected") : QStringLiteral("not connected"))
              << QStringLiteral("Display: %1")
                     .arg(m_vm->dbusMode() ? QStringLiteral("D-Bus") : QStringLiteral("SDL"));
        if (m_vm->mouseConsoleWidth() > 0 && m_vm->mouseConsoleHeight() > 0)
            lines << QStringLiteral("Guest console: %1 × %2 pixels")
                         .arg(m_vm->mouseConsoleWidth()).arg(m_vm->mouseConsoleHeight());
        else
            lines << QStringLiteral("Guest console geometry: not reported yet");
        if (m_vm->liveDisplay()) {
            const qint64 age = now - m_vm->lastLiveFrameMs();
            if (age > 4000 || age < 0)
                lines << QStringLiteral("Frame updates: no new image for %1 s")
                             .arg(qMax(qint64(0), age) / 1000);
            else
                lines << QStringLiteral("Observed framebuffer file changes: ~%1/s (NOT presentation FPS)")
                             .arg(m_vm->observedFrameUpdates());
        } else {
            lines << QStringLiteral("Live framebuffer unavailable; QMP screenshot fallback may be active");
        }
        lines << QStringLiteral("Display detail: %1").arg(m_vm->liveDisplayStatus());
    } else {
        lines << QStringLiteral("QEMU: stopped")
              << QStringLiteral("Selected profile: %1").arg(m_vm->activeProfileName());
    }
    lines << QStringLiteral("VM status: %1").arg(m_vm->status())
          << QString()
          << QStringLiteral("Runtime checks")
          << QStringLiteral("QEMU executable: %1 — %2")
                 .arg(qemu.isFile() && qemu.isExecutable() ? QStringLiteral("OK") : QStringLiteral("MISSING / NOT EXECUTABLE"), executable)
          << QStringLiteral("qemu-img beside QEMU: %1")
                 .arg(img.isFile() && img.isExecutable() ? QStringLiteral("OK") : QStringLiteral("MISSING / NOT EXECUTABLE"))
          << QStringLiteral("D-Bus display listener: %1")
                 .arg(listener.isFile() && listener.isExecutable() ? QStringLiteral("OK") : QStringLiteral("MISSING / NOT EXECUTABLE"))
          << QStringLiteral("VM launch script: %1")
                 .arg(runner.isFile() ? QStringLiteral("OK") : QStringLiteral("not found in installed path"))
          << QString()
          << QStringLiteral("QEMU log: %1").arg(m_vm->logPath())
          << QString()
          << QStringLiteral("Recent QEMU log (last ~2.5 KiB; read-only):")
          << recentQemuLog(m_vm->logPath());
    if (!m_vm->lastError().isEmpty())
        lines << QStringLiteral("Last error: %1").arg(m_vm->lastError());
    lines << QStringLiteral("Measured frontend CPU and RAM include SailQEMU UI; QEMU CPU includes its emulator threads.");
    const QString next = lines.join(QLatin1Char('\n'));
    if (next != m_report) {
        m_report = next;
        emit changed();
    }
}
