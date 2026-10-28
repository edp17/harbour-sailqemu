#pragma once
#include <QObject>
#include <QProcess>
#include <QScopedPointer>
#include <QTemporaryDir>

class VmController;

// Non-destructive qemu-img operations. QProcess runs asynchronously so a
// multi-gigabyte conversion cannot freeze the Sailfish Qt Quick interface.
class DiskManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString report READ report NOTIFY changed)
    Q_PROPERTY(QString resultPath READ resultPath NOTIFY changed)
    Q_PROPERTY(QString resultFormat READ resultFormat NOTIFY changed)
    Q_PROPERTY(QString sourcePath READ sourcePath WRITE setSourcePath NOTIFY changed)
    Q_PROPERTY(QString suggestedDirectory READ suggestedDirectory CONSTANT)
public:
    explicit DiskManager(VmController *vm, QObject *parent = nullptr);
    bool busy() const { return m_process.state() != QProcess::NotRunning || m_job != None; }
    QString status() const { return m_status; }
    QString report() const { return m_report; }
    QString resultPath() const { return m_resultPath; }
    QString resultFormat() const { return m_resultFormat; }
    QString sourcePath() const { return m_sourcePath; }
    QString suggestedDirectory() const { return m_directory; }
    void setSourcePath(const QString &value);

    Q_INVOKABLE bool inspectImage(const QString &qemuExecutable, const QString &path);
    Q_INVOKABLE bool createImage(const QString &qemuExecutable, const QString &destination,
                                 const QString &format, int sizeMiB);
    Q_INVOKABLE bool copyImage(const QString &qemuExecutable, const QString &source,
                               const QString &destination, const QString &format);
    Q_INVOKABLE void cancel();
signals:
    void changed();
    void operationCompleted();
private:
    enum Job { None, Inspect, Create, Convert };
    bool begin(Job job, const QString &binary, const QStringList &arguments);
    bool prepareOutput(const QString &destination, const QString &format);
    QString imageTool(const QString &qemuExecutable) const;
    QString expanded(const QString &path) const;
    void fail(const QString &why);
    void finish(int code, QProcess::ExitStatus result);
    VmController *m_vm;
    QProcess m_process;
    QScopedPointer<QTemporaryDir> m_stage;
    Job m_job = None;
    QString m_directory;
    QString m_sourcePath;
    QString m_status = QStringLiteral("Ready");
    QString m_report;
    QString m_resultPath;
    QString m_resultFormat;
    QString m_destination;
    QString m_stagedPath;
    QString m_pendingFormat;
    QString m_output;
    bool m_cancelRequested = false;
};
