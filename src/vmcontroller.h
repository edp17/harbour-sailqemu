#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>
#include <QLocalSocket>
#include <QTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QList>
#include <QStringList>

struct VmProfile
{
    QString id;
    QString name;
    QString qemuPath;
    QString diskPath;
    QString diskFormat = QStringLiteral("raw");
    QStringList secondaryDisks;
    QStringList secondaryFormats;
    QString isoFormat = QStringLiteral("raw");
    QString bootPrimary;     // empty: preserve legacy boot preferences
    QString bootSecondary;   // empty: preserve legacy boot preferences; "none" disables it
    QString isoPath;
    QString floppyPath;
    int memoryMiB = 128;
    // Empty/default means the same virtual hardware as in M12r2.
    QString cpuModel = QStringLiteral("default");
    int cpuCount = 1;
    QString videoAdapter = QStringLiteral("std");
    // Missing in older profiles: keep the M13 default PC/USB behaviour.
    bool acpiEnabled = true;
    bool usbTabletEnabled = true;
    bool fullscreen = false;
    bool isoBootFirst = true;
    bool floppyBootFirst = true;
    bool networkEnabled = false;
    // Guest-pixel correction for absolute mouse events; kept per VM profile.
    int mouseOffsetX = 0;
    int mouseOffsetY = 0;
};

class VmController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString qemuPath READ qemuPath WRITE setQemuPath NOTIFY configChanged)
    Q_PROPERTY(QString diskPath READ diskPath WRITE setDiskPath NOTIFY configChanged)
    Q_PROPERTY(QString diskFormat READ diskFormat WRITE setDiskFormat NOTIFY configChanged)
    Q_PROPERTY(QVariantList secondaryDisks READ secondaryDisks NOTIFY configChanged)
    Q_PROPERTY(QString isoFormat READ isoFormat WRITE setIsoFormat NOTIFY configChanged)
    Q_PROPERTY(QString bootPrimary READ bootPrimary WRITE setBootPrimary NOTIFY configChanged)
    Q_PROPERTY(QString bootSecondary READ bootSecondary WRITE setBootSecondary NOTIFY configChanged)
    Q_PROPERTY(QString isoPath READ isoPath WRITE setIsoPath NOTIFY configChanged)
    Q_PROPERTY(QString floppyPath READ floppyPath WRITE setFloppyPath NOTIFY configChanged)
    Q_PROPERTY(int memoryMiB READ memoryMiB WRITE setMemoryMiB NOTIFY configChanged)
    Q_PROPERTY(QString cpuModel READ cpuModel WRITE setCpuModel NOTIFY configChanged)
    Q_PROPERTY(int cpuCount READ cpuCount WRITE setCpuCount NOTIFY configChanged)
    Q_PROPERTY(QString videoAdapter READ videoAdapter WRITE setVideoAdapter NOTIFY configChanged)
    Q_PROPERTY(bool acpiEnabled READ acpiEnabled WRITE setAcpiEnabled NOTIFY configChanged)
    Q_PROPERTY(bool usbTabletEnabled READ usbTabletEnabled WRITE setUsbTabletEnabled NOTIFY configChanged)
    Q_PROPERTY(bool fullscreen READ fullscreen WRITE setFullscreen NOTIFY configChanged)
    Q_PROPERTY(bool isoBootFirst READ isoBootFirst WRITE setIsoBootFirst NOTIFY configChanged)
    Q_PROPERTY(bool floppyBootFirst READ floppyBootFirst WRITE setFloppyBootFirst NOTIFY configChanged)
    Q_PROPERTY(bool networkEnabled READ networkEnabled WRITE setNetworkEnabled NOTIFY configChanged)
    Q_PROPERTY(QVariantList profiles READ profiles NOTIFY profilesChanged)
    Q_PROPERTY(int profileCount READ profileCount NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfileId READ activeProfileId NOTIFY profilesChanged)
    Q_PROPERTY(QString activeProfileName READ activeProfileName NOTIFY profilesChanged)
    Q_PROPERTY(QString attachedMediaReport READ attachedMediaReport NOTIFY attachedMediaChanged)
    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(bool paused READ paused NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(QString status READ status NOTIFY stateChanged)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(QString lastError READ lastError NOTIFY stateChanged)
    Q_PROPERTY(bool shutdownPending READ shutdownPending NOTIFY stateChanged)
    Q_PROPERTY(QString shutdownStatus READ shutdownStatus NOTIFY stateChanged)
    Q_PROPERTY(QString guestPreviewUrl READ guestPreviewUrl NOTIFY guestPreviewChanged)
    Q_PROPERTY(bool guestPreviewAvailable READ guestPreviewAvailable NOTIFY guestPreviewChanged)
    Q_PROPERTY(bool dbusMode READ dbusMode NOTIFY dbusChanged)
    Q_PROPERTY(bool dbusReady READ dbusReady NOTIFY dbusChanged)
    Q_PROPERTY(QString dbusStatus READ dbusStatus NOTIFY dbusChanged)
    Q_PROPERTY(QString dbusConsoles READ dbusConsoles NOTIFY dbusChanged)
    Q_PROPERTY(bool liveDisplay READ liveDisplay NOTIFY guestPreviewChanged)
    Q_PROPERTY(QString liveDisplayStatus READ liveDisplayStatus NOTIFY liveDisplayChanged)
    Q_PROPERTY(bool mouseAvailable READ mouseAvailable NOTIFY mouseChanged)
    Q_PROPERTY(bool mouseAbsolute READ mouseAbsolute NOTIFY mouseChanged)
    Q_PROPERTY(QString mouseStatus READ mouseStatus NOTIFY mouseChanged)
    Q_PROPERTY(int mouseConsoleWidth READ mouseConsoleWidth NOTIFY mouseChanged)
    Q_PROPERTY(int mouseConsoleHeight READ mouseConsoleHeight NOTIFY mouseChanged)
    Q_PROPERTY(QString mouseInputDiagnostic READ mouseInputDiagnostic NOTIFY mouseChanged)
    Q_PROPERTY(int mouseOffsetX READ mouseOffsetX NOTIFY configChanged)
    Q_PROPERTY(int mouseOffsetY READ mouseOffsetY NOTIFY configChanged)
    Q_PROPERTY(bool guestCursorVisible READ guestCursorVisible NOTIFY guestCursorChanged)
    Q_PROPERTY(QString guestCursorUrl READ guestCursorUrl NOTIFY guestCursorChanged)
    Q_PROPERTY(int guestCursorX READ guestCursorX NOTIFY guestCursorChanged)
    Q_PROPERTY(int guestCursorY READ guestCursorY NOTIFY guestCursorChanged)
    Q_PROPERTY(int guestCursorHotX READ guestCursorHotX NOTIFY guestCursorChanged)
    Q_PROPERTY(int guestCursorHotY READ guestCursorHotY NOTIFY guestCursorChanged)

public:
    explicit VmController(QObject *parent = nullptr);

    QString qemuPath() const { return m_qemuPath; }
    QString diskPath() const { return m_diskPath; }
    QString diskFormat() const { return m_diskFormat; }
    QVariantList secondaryDisks() const;
    QString isoFormat() const { return m_isoFormat; }
    QString bootPrimary() const;
    QString bootSecondary() const;
    QString isoPath() const { return m_isoPath; }
    QString floppyPath() const { return m_floppyPath; }
    int memoryMiB() const { return m_memoryMiB; }
    QString cpuModel() const { return m_cpuModel; }
    int cpuCount() const { return m_cpuCount; }
    QString videoAdapter() const { return m_videoAdapter; }
    bool acpiEnabled() const { return m_acpiEnabled; }
    bool usbTabletEnabled() const { return m_usbTabletEnabled; }
    bool fullscreen() const { return m_fullscreen; }
    bool isoBootFirst() const { return m_isoBootFirst; }
    bool floppyBootFirst() const { return m_floppyBootFirst; }
    bool networkEnabled() const { return m_networkEnabled; }
    QVariantList profiles() const;
    int profileCount() const { return m_profiles.size(); }
    QString activeProfileId() const { return m_activeId; }
    QString activeProfileName() const;
    QString attachedMediaReport() const { return m_attachedMediaReport; }
    bool running() const { return m_running; }
    qint64 qemuPid() const { return m_pid; }
    int observedFrameUpdates() const { return m_liveObservedHz; }
    qint64 lastLiveFrameMs() const { return m_liveLastFrameMs; }
    bool paused() const { return m_paused; }
    bool connected() const { return m_connected; }
    QString status() const { return m_status; }
    QString lastError() const { return m_lastError; }
    bool shutdownPending() const { return m_shutdownPending; }
    QString shutdownStatus() const { return m_shutdownStatus; }
    QString logPath() const { return m_logPath; }
    QString guestPreviewUrl() const;
    bool guestPreviewAvailable() const { return m_guestPreviewAvailable || m_liveDisplayAvailable; }
    bool dbusMode() const { return m_dbusMode; }
    bool dbusReady() const { return m_dbusReady; }
    QString dbusStatus() const { return m_dbusStatus; }
    QString dbusConsoles() const { return m_dbusConsoles; }
    bool liveDisplay() const { return m_liveDisplayAvailable; }
    QString liveDisplayStatus() const { return m_liveDisplayStatus; }
    bool mouseAvailable() const { return m_mouseAvailable; }
    bool mouseAbsolute() const { return m_mouseAbsolute; }
    QString mouseStatus() const { return m_mouseStatus; }
    int mouseOffsetX() const { return m_mouseOffsetX; }
    int mouseOffsetY() const { return m_mouseOffsetY; }
    bool guestCursorVisible() const { return m_guestCursorVisible; }
    QString guestCursorUrl() const;
    int guestCursorX() const { return m_guestCursorX; }
    int guestCursorY() const { return m_guestCursorY; }
    int guestCursorHotX() const { return m_guestCursorHotX; }
    int guestCursorHotY() const { return m_guestCursorHotY; }

    int mouseConsoleWidth() const { return m_mouseConsoleWidth; }
    int mouseConsoleHeight() const { return m_mouseConsoleHeight; }
    QString mouseInputDiagnostic() const { return m_mouseInputDiagnostic; }

    void setQemuPath(const QString &value);
    void setDiskPath(const QString &value);
    void setDiskFormat(const QString &value);
    void setIsoFormat(const QString &value);
    void setBootPrimary(const QString &value);
    void setBootSecondary(const QString &value);
    Q_INVOKABLE bool addSecondaryDisk();
    Q_INVOKABLE bool removeSecondaryDisk(int index);
    Q_INVOKABLE bool chooseSecondaryDiskImage(int index, const QString &path);
    Q_INVOKABLE bool setSecondaryDiskFormat(int index, const QString &format);
    void setIsoPath(const QString &value);
    void setFloppyPath(const QString &value);
    void setMemoryMiB(int value);
    void setCpuModel(const QString &value);
    void setCpuCount(int value);
    void setVideoAdapter(const QString &value);
    void setAcpiEnabled(bool value);
    void setUsbTabletEnabled(bool value);
    Q_INVOKABLE bool applyHardwarePreset(const QString &guest);
    void setFullscreen(bool value);
    void setIsoBootFirst(bool value);
    void setFloppyBootFirst(bool value);
    void setNetworkEnabled(bool value);
    Q_INVOKABLE bool chooseDiskImage(const QString &path);

    Q_INVOKABLE void save();
    Q_INVOKABLE bool createProfile(const QString &name);
    Q_INVOKABLE bool selectProfile(const QString &id);
    Q_INVOKABLE bool renameProfile(const QString &id, const QString &name);
    Q_INVOKABLE bool saveProfileEdits(const QString &id, const QString &name);
    Q_INVOKABLE QString mediaConfigurationReport() const;
    Q_INVOKABLE QString storageReferences(const QString &path) const;
    Q_INVOKABLE void inspectAttachedMedia();
    Q_INVOKABLE bool duplicateProfile(const QString &id);
    Q_INVOKABLE bool deleteProfile(const QString &id);
    Q_INVOKABLE bool startVm();
    Q_INVOKABLE bool startVmWithConsole();
    Q_INVOKABLE bool startVmWithDbusConsole();
    Q_INVOKABLE void refreshDbusDisplay();
    Q_INVOKABLE void resyncGuestDisplay();
    Q_INVOKABLE void pauseVm();
    Q_INVOKABLE void resumeVm();
    Q_INVOKABLE void requestShutdown();
    Q_INVOKABLE void forceQuit();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE bool sendGuestKey(const QString &key);
    Q_INVOKABLE bool sendGuestText(const QString &text, bool pressEnter);
    Q_INVOKABLE void refreshGuestPreview();
    Q_INVOKABLE bool guestMouseMove(int x, int y);
    // PS/2-style input is relative. Movement is a finger-swipe delta, NOT
    // another point in the guest framebuffer (which only works with a tablet).
    Q_INVOKABLE bool guestMouseRelative(int dx, int dy);
    Q_INVOKABLE bool guestMouseButton(const QString &button, bool pressed);
    Q_INVOKABLE bool guestMouseClick(int x, int y, const QString &button);
    Q_INVOKABLE bool guestMouseWheel(bool up);
    Q_INVOKABLE void adjustMouseOffset(int dx, int dy);
    Q_INVOKABLE void resetMouseOffset();

signals:
    void configChanged();
    void profilesChanged();
    void stateChanged();
    void guestPreviewChanged();
    void dbusChanged();
    void liveDisplayChanged();
    void mouseChanged();
    void guestCursorChanged();
    void attachedMediaChanged();

private:
    bool startVmInternal(bool dbusDisplay);
    void setDbusStatus(const QString &message, const QString &consoles = QString(), bool ready = false);
    void setLiveDisplayStatus(const QString &message);
    void launchLiveDisplay(uint consoleId);
    void stopLiveDisplay();
    void updateLiveDisplay();
    void refreshDbusMouse(uint consoleId);
    void refreshMouseGeometry();
    bool mouseCall(const QString &method, const QVariantList &args);
    void resetMouse();
    void updateGuestCursor();
    void load();
    void migrateLegacy();
    bool writeProfiles();
    QStringList legacyBootOrder() const;
    QStringList effectiveBootOrder() const;
    void copyActiveToProfile();
    void loadActiveFields();
    int profileIndex(const QString &id) const;
    bool nameAvailable(const QString &name, const QString &excludingId = QString()) const;
    QString newId() const;
    QString defaultQemuPath() const;
    void setStatus(const QString &status);
    void setError(const QString &error);
    void setRunning(bool running, bool paused = false);
    void sendQmp(const QString &command);
    bool guestInputReady();
    bool queueKeyStroke(const QStringList &keyCodes);
    void dispatchNextKey();
    void clearGuestInput();
    void completeGuestPreview();
    void readQmp();
    void handleQmpLine(const QByteArray &line);
    void poll();
    bool processExists() const;
    void resetStopped(const QString &message);
    QString configFile() const;
    QString legacyFile() const;
    QString socketFile() const;

    QString m_attachedMediaReport;
    QVector<VmProfile> m_profiles;
    QString m_activeId;
    QString m_runningProfileId;

    QString m_qemuPath;
    QString m_diskPath;
    QString m_diskFormat = QStringLiteral("raw");
    QStringList m_secondaryDisks;
    QStringList m_secondaryFormats;
    QString m_isoFormat = QStringLiteral("raw");
    QString m_bootPrimary;
    QString m_bootSecondary;
    QString m_isoPath;
    QString m_floppyPath;
    int m_memoryMiB = 128;
    QString m_cpuModel = QStringLiteral("default");
    int m_cpuCount = 1;
    QString m_videoAdapter = QStringLiteral("std");
    bool m_acpiEnabled = true;
    bool m_usbTabletEnabled = true;
    bool m_fullscreen = false;
    bool m_isoBootFirst = true;
    bool m_floppyBootFirst = true;
    bool m_networkEnabled = false;
    int m_mouseOffsetX = 0;
    int m_mouseOffsetY = 0;

    // Lifecycle diagnostics: these flags describe QMP events, never guest disks.
    bool m_hadQmpConnection = false;
    bool m_seenGuestShutdown = false;
    bool m_forceQuitRequested = false;
    bool m_qmpConnectionWarned = false;
    qint64 m_qmpConnectionStartMs = 0;
    bool m_shutdownPending = false;
    QString m_shutdownStatus;
    bool m_running = false;
    bool m_paused = false;
    bool m_connected = false;
    QString m_status = QStringLiteral("Not running");
    QString m_lastError;
    QString m_logPath;
    QString m_socketPath;
    qint64 m_pid = 0;
    QString m_expectedQemuName; // per-launch -name token, persisted for reconnection
    bool m_qmpNameConfirmed = false;
    QLocalSocket m_qmp;
    QByteArray m_qmpBuffer;
    QTimer m_pollTimer;
    QTimer m_keyTimer;
    QList<QStringList> m_pendingKeys;
    quint64 m_guestKeySequence = 0;
    QString m_guestPreviewPpmPath;
    QString m_guestPreviewPngPath;
    bool m_guestPreviewPending = false;
    bool m_guestPreviewAvailable = false;
    // Ephemeral framebuffer/cursor files in a private /tmp directory, not
    // hundreds of writes per minute to the persistent app cache.
    QTemporaryDir m_liveFrameDir;
    QProcess m_liveHelper;
    QTimer m_liveTimer;
    QString m_livePngPath;
    QString m_liveDisplayStatus = QStringLiteral("Live D-Bus listener not started");
    QString m_liveRecentLog;
    bool m_liveDisplayAvailable = false;
    qint64 m_liveLastModified = 0;
    qint64 m_liveLastFrameMs = 0;
    qint64 m_liveLastAttempt = 0;
    unsigned m_liveRevision = 0;
    qint64 m_liveSampleStart = 0;
    int m_liveSampleFrames = 0;
    int m_liveObservedHz = 0;
    qint64 m_liveLastFrameKiB = 0;
    bool m_mouseAvailable = false;
    bool m_mouseAbsolute = false;
    QString m_mouseStatus = QStringLiteral("Mouse not connected");
    uint m_mouseConsoleId = 0;
    int m_mouseConsoleWidth = 0;
    int m_mouseConsoleHeight = 0;
    QString m_mouseInputDiagnostic = QStringLiteral("No guest mouse input sent");
    int m_mouseLastX = 0;
    int m_mouseLastY = 0;
    bool m_mouseHasLast = false;
    QString m_guestCursorPngPath;
    QString m_guestCursorStatePath;
    bool m_guestCursorVisible = false;
    bool m_guestCursorImageAvailable = false;
    int m_guestCursorX = 0;
    int m_guestCursorY = 0;
    int m_guestCursorHotX = 0;
    int m_guestCursorHotY = 0;
    qint64 m_guestCursorStateModified = 0;
    qint64 m_guestCursorImageModified = 0;
    unsigned m_guestCursorRevision = 0;
    bool m_dbusMode = false;
    bool m_dbusReady = false;
    QString m_dbusStatus = QStringLiteral("D-Bus display not started");
    QString m_dbusConsoles;
    qint64 m_guestPreviewRequestMs = 0;
    unsigned m_guestPreviewRevision = 0;
    int m_tick = 0;
};
