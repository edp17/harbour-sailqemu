#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QtGlobal>

class VmController;

// Lightweight read-only samples from /proc. A CPU percentage is measured
// against ONE CPU core (100%=one fully occupied core), not total device load.
class RuntimeDiagnostics : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString report READ report NOTIFY changed)
public:
    explicit RuntimeDiagnostics(VmController *vm, QObject *parent = nullptr);
    QString report() const { return m_report; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void copyReport();
signals:
    void changed();
private:
    struct CpuSample {
        qint64 pid = 0;
        quint64 ticks = 0;
        qint64 timeMs = 0;
        double lastPercent = -1.0;
    };
    static bool readProcCpu(qint64 pid, quint64 *ticks);
    static qint64 readProcRssKiB(qint64 pid);
    double sampleCpu(qint64 pid, CpuSample *previous, qint64 nowMs);
    static QString formattedCpu(double percent);
    static QString formattedMemory(qint64 rssKiB);
    VmController *m_vm;
    QTimer m_timer;
    CpuSample m_frontend;
    CpuSample m_emulator;
    QString m_report;
};
