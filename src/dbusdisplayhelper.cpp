// SailQEMU M6: standalone, unprivileged QEMU D-Bus framebuffer listener.
// The frontend launches this helper only after checking the unique QEMU
// name through QMP and D-Bus. Recheck that name here, since Sailjail PID
// namespaces make host-bus PIDs different from sandbox process PIDs.
// The helper holds no guest disk or keyboard permissions of its own.
#include <QCoreApplication>
#include <QImage>
#include <QSaveFile>
#include <QImageWriter>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QByteArray>
#include <QBuffer>
#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <glib-unix.h>
#include <pixman.h>
#include <sys/socket.h>
#include <unistd.h>
#include <csignal>
#include <limits>
#include <cstring>

namespace {
constexpr int kMaxSide = 4096;
// Roll back aggressive 90 ms frame cadence following the on-device report
// of increased Windows XP startup time. Retain incremental dirty tracking.
constexpr guint kFrameIntervalMs = 120;
constexpr qint64 kMaxBytes = 64 * 1024 * 1024;
const char kListenerPath[] = "/org/qemu/Display1/Listener";
const char kListenerXml[] = R"XML(<node>
 <interface name="org.qemu.Display1.Listener">
  <method name="Scanout"><arg type="u" direction="in"/><arg type="u" direction="in"/><arg type="u" direction="in"/><arg type="u" direction="in"/><arg type="ay" direction="in"/></method>
  <method name="Update"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="u" direction="in"/><arg type="u" direction="in"/><arg type="ay" direction="in"/></method>
  <method name="Disable"/>
  <method name="MouseSet"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="i" direction="in"/></method>
  <method name="CursorDefine"><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="i" direction="in"/><arg type="ay" direction="in"/></method>
  <property name="Interfaces" type="as" access="read"/>
 </interface>
</node>)XML";

struct State {
    QImage frame;
    QString output;
    QString cursorPng;
    QString cursorState;
    int cursorX = 0, cursorY = 0;
    int cursorHotX = 0, cursorHotY = 0;
    bool cursorVisible = false;
    GMainLoop *loop = nullptr;
    guint flushSource = 0;
    unsigned frames = 0;
    unsigned unsupported = 0;
};

// A QEMU pixman image uses native-endian 32-bit words. Convert a D-Bus
// rectangle DIRECTLY into the retained framebuffer, without constructing a
// temporary QImage for each Update. Guard every byte and rectangle boundary.
// RGB32 is always opaque, matching the older M6 decoder (including x8 data).
static bool writePixels(QImage &target, int x, int y, int w, int h,
                        int stride, guint32 fmt, const guint8 *bytes,
                        gsize size, bool *changed = nullptr)
{
    if (changed) *changed = false;
    if (target.isNull() || w < 1 || h < 1 || x < 0 || y < 0 ||
        qint64(x) + w > target.width() || qint64(y) + h > target.height() ||
        w > kMaxSide || h > kMaxSide || stride < qint64(w) * 4 ||
        stride > kMaxSide * 16 || qint64(stride) * h > kMaxBytes ||
        gsize(stride) * gsize(h) > size || !bytes)
        return false;
    if (fmt != PIXMAN_x8r8g8b8 && fmt != PIXMAN_a8r8g8b8 &&
        fmt != PIXMAN_x8b8g8r8 && fmt != PIXMAN_a8b8g8r8)
        return false;
    const bool reversed = fmt == PIXMAN_x8b8g8r8 || fmt == PIXMAN_a8b8g8r8;
    bool anyChanged = false;
    for (int row = 0; row < h; ++row) {
        const guint8 *src = bytes + size_t(row) * stride;
        QRgb *dst = reinterpret_cast<QRgb *>(target.scanLine(y + row)) + x;
        for (int col = 0; col < w; ++col) {
            guint32 word = 0;
            std::memcpy(&word, src + size_t(col) * 4, 4);
            const QRgb pixel = reversed
                ? qRgb(word & 0xff, (word >> 8) & 0xff, (word >> 16) & 0xff)
                : (word | 0xff000000u);
            if (dst[col] != pixel) {
                dst[col] = pixel;
                anyChanged = true;
            }
        }
    }
    if (changed) *changed = anyChanged;
    return true;
}

static bool decodePixels(QImage &target, int w, int h, int stride,
                         guint32 fmt, const guint8 *bytes, gsize size)
{
    if (w < 1 || h < 1 || w > kMaxSide || h > kMaxSide ||
        qint64(w) * h * 4 > kMaxBytes)
        return false;
    QImage next(w, h, QImage::Format_RGB32);
    if (next.isNull() || !writePixels(next, 0, 0, w, h, stride, fmt,
                                     bytes, size)) return false;
    target = next;
    return true;
}

static gboolean flushFrame(gpointer user)
{
    State *s = static_cast<State *>(user);
    s->flushSource = 0;
    if (s->frame.isNull()) return G_SOURCE_REMOVE;
    QSaveFile file(s->output);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "M6: cannot open frame output" << s->output << file.errorString();
        return G_SOURCE_REMOVE;
    }
    // Moderate PNG compression reduces CPU time on the AArch64 host. The
    // image remains LOSSLESS, and the committed rename is atomic for Qt Quick.
    QImageWriter writer(&file, "png");
    writer.setCompression(35);
    if (!writer.write(s->frame) || !file.commit()) {
        qWarning() << "M6: cannot write frame" << s->output << file.errorString();
        file.cancelWriting();
        return G_SOURCE_REMOVE;
    }
    return G_SOURCE_REMOVE;
}

static void scheduleFlush(State *s)
{
    // Coalesce bursts of D-Bus updates: never encode unchanged rectangles,
    // and at most one lossless frame per interval. Native guest size is kept.
    if (!s->flushSource) s->flushSource = g_timeout_add(kFrameIntervalMs, flushFrame, s);
}

static void writeCursorState(State *s)
{
    QSaveFile file(s->cursorState);
    if (file.open(QIODevice::WriteOnly)) {
        const QByteArray status = QByteArray::number(s->cursorX) + ' '
            + QByteArray::number(s->cursorY) + ' '
            + QByteArray::number(s->cursorVisible ? 1 : 0) + ' '
            + QByteArray::number(s->cursorHotX) + ' '
            + QByteArray::number(s->cursorHotY) + '\n';
        if (file.write(status) != status.size() || !file.commit())
            qWarning() << "M7: cannot write cursor state" << s->cursorState;
    }
}

static const guint8 *variantBytes(GVariant *data, gsize *length)
{
    return static_cast<const guint8 *>(g_variant_get_fixed_array(data, length, sizeof(guint8)));
}

static void methodCall(GDBusConnection *, const gchar *, const gchar *, const gchar *,
                       const gchar *method, GVariant *params, GDBusMethodInvocation *inv,
                       gpointer user)
{
    State *s = static_cast<State *>(user);
    if (g_strcmp0(method, "Scanout") == 0) {
        guint32 w, h, stride, fmt;
        GVariant *data = nullptr;
        g_variant_get(params, "(uuuu@ay)", &w, &h, &stride, &fmt, &data);
        gsize len = 0;
        const guint8 *bytes = variantBytes(data, &len);
        QImage img;
        if (decodePixels(img, int(w), int(h), int(stride), fmt, bytes, len)) {
            // A full Scanout may be repeated even if every pixel is identical.
            // Do not re-encode it unless the contents or resolution changed.
            bool differs = img.size() != s->frame.size() || s->frame.isNull();
            if (!differs) {
                for (int row = 0; row < img.height() && !differs; ++row)
                    differs = std::memcmp(img.constScanLine(row),
                                          s->frame.constScanLine(row),
                                          size_t(img.width()) * 4) != 0;
            }
            if (differs) {
                s->frame = img;
                ++s->frames;
                scheduleFlush(s);
            }
        } else if (++s->unsupported <= 4) {
            qWarning() << "M6: unsupported Scanout" << w << h << stride
                       << "pixman" << (QStringLiteral("0x") + QString::number(fmt, 16))
                       << "bytes" << len;
        }
        g_variant_unref(data);
    } else if (g_strcmp0(method, "Update") == 0) {
        gint32 x, y, w, h;
        guint32 stride, fmt;
        GVariant *data = nullptr;
        g_variant_get(params, "(iiiiuu@ay)", &x, &y, &w, &h, &stride, &fmt, &data);
        gsize len = 0;
        const guint8 *bytes = variantBytes(data, &len);
        bool changed = false;
        if (writePixels(s->frame, x, y, w, h, int(stride), fmt,
                        bytes, len, &changed)) {
            if (changed) {
                ++s->frames;
                scheduleFlush(s);
            }
        } else if (++s->unsupported <= 4) {
            qWarning() << "M12: unsupported Update" << x << y << w << h
                       << "stride" << stride << "pixman"
                       << (QStringLiteral("0x") + QString::number(fmt, 16));
        }
        g_variant_unref(data);
    } else if (g_strcmp0(method, "Disable") == 0) {
        s->frame = QImage();
        QFile::remove(s->output);
    } else if (g_strcmp0(method, "MouseSet") == 0) {
        gint32 x = 0, y = 0, on = 0;
        g_variant_get(params, "(iii)", &x, &y, &on);
        s->cursorX = x; s->cursorY = y;
        s->cursorVisible = on != 0;
        writeCursorState(s);
    } else if (g_strcmp0(method, "CursorDefine") == 0) {
        gint32 w = 0, h = 0, hotX = 0, hotY = 0;
        GVariant *data = nullptr;
        g_variant_get(params, "(iiii@ay)", &w, &h, &hotX, &hotY, &data);
        gsize len = 0;
        const guint8 *bytes = variantBytes(data, &len);
        s->cursorHotX = hotX; s->cursorHotY = hotY;
        if (w > 0 && h > 0 && w <= 256 && h <= 256 &&
            len >= gsize(w) * gsize(h) * 4 && bytes) {
            // D-Bus CursorDefine uses native-endian ARGB32, which corresponds
            // to QImage::Format_ARGB32 on little-endian Sailfish AArch64.
            QImage cursor(reinterpret_cast<const uchar *>(bytes), w, h,
                          w * 4, QImage::Format_ARGB32);
            const QImage copy = cursor.copy();
            QSaveFile file(s->cursorPng);
            if (file.open(QIODevice::WriteOnly)) {
                if (!copy.save(&file, "PNG") || !file.commit())
                    qWarning() << "M7: cursor PNG save failed" << s->cursorPng;
            }
        } else {
            QFile::remove(s->cursorPng);
        }
        writeCursorState(s);
        g_variant_unref(data);
    }
    g_dbus_method_invocation_return_value(inv, g_variant_new("()"));
}

static GVariant *getProperty(GDBusConnection *, const gchar *, const gchar *,
                              const gchar *, const gchar *name, GError **, gpointer)
{
    if (g_strcmp0(name, "Interfaces") == 0)
        return g_variant_new_strv(nullptr, 0); // No shared-map/DMABUF support.
    return nullptr;
}

static void peerClosed(GDBusConnection *, gboolean, GError *, gpointer p)
{
    g_main_loop_quit(static_cast<State *>(p)->loop);
}

static gboolean stopLoop(gpointer p)
{
    g_main_loop_quit(static_cast<State *>(p)->loop);
    return G_SOURCE_CONTINUE;
}
}

int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    if (argc == 2 && QByteArray(argv[1]) == "--self-test") {
        // Validates pixel order and availability of Qt's PNG writer without
        // starting QEMU or registering a D-Bus service.
        const guint8 rgb32[] = { 0x12, 0x34, 0x56, 0xff,
                                 0xab, 0xcd, 0xef, 0xff };
        QImage frame;
        if (!decodePixels(frame, 2, 1, 8, PIXMAN_x8r8g8b8,
                          rgb32, sizeof(rgb32)) ||
            frame.pixel(0, 0) != qRgb(0x56, 0x34, 0x12) ||
            frame.pixel(1, 0) != qRgb(0xef, 0xcd, 0xab)) {
            qCritical() << "M6 SELF-TEST FAIL: pixman RGB32 conversion";
            return 10;
        }
        QByteArray encoded;
        QBuffer buffer(&encoded);
        buffer.open(QIODevice::WriteOnly);
        if (!frame.save(&buffer, "PNG") || encoded.size() < 60) {
            qCritical() << "M6 SELF-TEST FAIL: PNG encoder unavailable";
            return 11;
        }
        qInfo() << "M6 SELF-TEST PASS: pixman RGB32 to Qt PNG";
        const quint32 argb = qRgba(0x12, 0x34, 0x56, 0x7f);
        QImage cursor(reinterpret_cast<const uchar *>(&argb), 1, 1, 4, QImage::Format_ARGB32);
        if (cursor.pixel(0, 0) != argb || cursor.copy().isNull()) {
            qCritical() << "M7 SELF-TEST FAIL: ARGB cursor conversion";
            return 12;
        }
        qInfo() << "M7 SELF-TEST PASS: ARGB cursor conversion";
        // Incremental blits must preserve surrounding pixels and suppress
        // redundant updates (critical for a quiet DOS command prompt).
        const guint8 patch[] = { 0x00, 0x10, 0x20, 0x00 };
        bool changed = false;
        if (!writePixels(frame, 1, 0, 1, 1, 4, PIXMAN_x8r8g8b8,
                         patch, sizeof(patch), &changed) || !changed ||
            frame.pixel(0, 0) != qRgb(0x56, 0x34, 0x12) ||
            frame.pixel(1, 0) != qRgb(0x20, 0x10, 0x00)) {
            qCritical() << "M12 SELF-TEST FAIL: region conversion";
            return 13;
        }
        if (!writePixels(frame, 1, 0, 1, 1, 4, PIXMAN_x8r8g8b8,
                         patch, sizeof(patch), &changed) || changed ||
            writePixels(frame, 2, 0, 1, 1, 4, PIXMAN_x8r8g8b8,
                        patch, sizeof(patch), &changed)) {
            qCritical() << "M12 SELF-TEST FAIL: unchanged or out-of-range blit";
            return 14;
        }
        QByteArray fastPng;
        QBuffer fastBuffer(&fastPng);
        fastBuffer.open(QIODevice::WriteOnly);
        QImageWriter fastWriter(&fastBuffer, "png");
        fastWriter.setCompression(35);
        if (!fastWriter.write(frame) || fastPng.size() < 60 ||
            QImage::fromData(fastPng, "PNG").pixel(1, 0) != qRgb(0x20, 0x10, 0x00)) {
            qCritical() << "M12 SELF-TEST FAIL: lossless fast encoder";
            return 15;
        }
        qInfo() << "M12 SELF-TEST PASS: incremental update, unchanged-frame skip, fast PNG";
        return 0;
    }
    if (argc != 4) {
        qCritical() << "Usage: harbour-sailqemu-display-listener <verified-qemu-name> <console-id> <absolute-png-path>";
        return 2;
    }
    bool idOk = false;
    const QByteArray expectedName(argv[1]);
    const uint console = QString::fromLocal8Bit(argv[2]).toUInt(&idOk);
    State s;
    s.output = QString::fromLocal8Bit(argv[3]);
    s.cursorPng = s.output + QStringLiteral(".cursor.png");
    s.cursorState = s.output + QStringLiteral(".cursor-state");
    if (!expectedName.startsWith("sailqemu-") || expectedName.size() != 45 ||
        !idOk || !QFileInfo(s.output).isAbsolute()) return 2;
    if (!QDir().mkpath(QFileInfo(s.output).absolutePath())) return 2;
    QFile::remove(s.output);
    QFile::remove(s.cursorPng);
    QFile::remove(s.cursorState);

    GError *error = nullptr;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus) {
        qCritical() << "M6: D-Bus session bus:" << (error ? error->message : "unknown error");
        g_clear_error(&error);
        return 3;
    }
    // Check org.qemu.Display1.VM.Name instead of the bus process ID.
    // The frontend already confirmed the same name through QMP query-name.
    // PIDs disagree across Sailjail's PID namespace, even for the SAME VM.
    GVariant *reply = g_dbus_connection_call_sync(bus, "org.qemu",
        "/org/qemu/Display1/VM", "org.freedesktop.DBus.Properties", "Get",
        g_variant_new("(ss)", "org.qemu.Display1.VM", "Name"),
        G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, 3000, nullptr, &error);
    GVariant *actualNameValue = nullptr;
    if (reply) {
        g_variant_get(reply, "(v)", &actualNameValue);
        g_variant_unref(reply);
    }
    const bool nameMatches = !error && actualNameValue &&
        g_variant_is_of_type(actualNameValue, G_VARIANT_TYPE_STRING) &&
        g_strcmp0(g_variant_get_string(actualNameValue, nullptr),
                  expectedName.constData()) == 0;
    if (!nameMatches) {
        qCritical() << "M6: QEMU D-Bus VM identity does not match verified frontend name:"
                    << (error ? error->message : "different VM or invalid Name property");
        if (actualNameValue) g_variant_unref(actualNameValue);
        g_clear_error(&error);
        g_object_unref(bus);
        return 4;
    }
    g_variant_unref(actualNameValue);

    int fds[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0) {
        qCritical() << "M6: socketpair failed";
        g_object_unref(bus);
        return 5;
    }
    GUnixFDList *fdlist = g_unix_fd_list_new();
    int index = g_unix_fd_list_append(fdlist, fds[1], &error);
    if (index < 0) {
        qCritical() << "M6: Unix FD append failed:" << (error ? error->message : "");
        g_clear_error(&error);
        close(fds[0]); close(fds[1]);
        g_object_unref(fdlist); g_object_unref(bus);
        return 5;
    }
    const QByteArray path = QStringLiteral("/org/qemu/Display1/Console_%1").arg(console).toUtf8();
    GVariant *result = g_dbus_connection_call_with_unix_fd_list_sync(bus,
        "org.qemu", path.constData(), "org.qemu.Display1.Console", "RegisterListener",
        g_variant_new("(h)", index), nullptr, G_DBUS_CALL_FLAGS_NONE, 5000,
        fdlist, nullptr, nullptr, &error);
    close(fds[1]);
    g_object_unref(fdlist);
    if (!result) {
        qCritical() << "M6: RegisterListener failed:" << (error ? error->message : "");
        g_clear_error(&error); close(fds[0]); g_object_unref(bus);
        return 6;
    }
    g_variant_unref(result);
    GSocket *socket = g_socket_new_from_fd(fds[0], &error);
    if (!socket) {
        qCritical() << "M6: GSocket from FD failed:" << (error ? error->message : "");
        g_clear_error(&error); close(fds[0]); g_object_unref(bus);
        return 7;
    }
    GSocketConnection *stream = g_socket_connection_factory_create_connection(socket);
    g_object_unref(socket);
    // QEMU is the authenticated D-Bus SERVER on the passed socket. The helper
    // is the CLIENT and exports the /org/qemu/Display1/Listener object.
    GDBusConnection *peer = g_dbus_connection_new_sync(G_IO_STREAM(stream), nullptr,
        GDBusConnectionFlags(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
                             G_DBUS_CONNECTION_FLAGS_DELAY_MESSAGE_PROCESSING),
        nullptr, nullptr, &error);
    g_object_unref(stream);
    if (!peer) {
        qCritical() << "M6: peer authentication failed:" << (error ? error->message : "");
        g_clear_error(&error); g_object_unref(bus);
        return 7;
    }
    GDBusNodeInfo *introspection = g_dbus_node_info_new_for_xml(kListenerXml, &error);
    if (!introspection) {
        qCritical() << "M6: invalid listener XML" << (error ? error->message : "");
        g_clear_error(&error); g_object_unref(peer); g_object_unref(bus);
        return 8;
    }
    static const GDBusInterfaceVTable vtable = { methodCall, getProperty, nullptr };
    guint registration = g_dbus_connection_register_object(peer, kListenerPath,
        introspection->interfaces[0], &vtable, &s, nullptr, &error);
    if (!registration) {
        qCritical() << "M6: cannot register listener:" << (error ? error->message : "");
        g_clear_error(&error); g_dbus_node_info_unref(introspection);
        g_object_unref(peer); g_object_unref(bus);
        return 8;
    }
    s.loop = g_main_loop_new(nullptr, FALSE);
    g_signal_connect(peer, "closed", G_CALLBACK(peerClosed), &s);
    g_dbus_connection_start_message_processing(peer);
    g_unix_signal_add(SIGTERM, stopLoop, &s);
    g_unix_signal_add(SIGINT, stopLoop, &s);
    g_main_loop_run(s.loop);
    if (s.flushSource) g_source_remove(s.flushSource);
    g_main_loop_unref(s.loop);
    g_dbus_connection_unregister_object(peer, registration);
    g_dbus_node_info_unref(introspection);
    g_dbus_connection_close_sync(peer, nullptr, nullptr);
    g_object_unref(peer);
    g_object_unref(bus);
    QFile::remove(s.output);
    QFile::remove(s.cursorPng);
    QFile::remove(s.cursorState);
    return 0;
}
