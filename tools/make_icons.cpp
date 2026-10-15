// Writes resources/favdir.ico (Windows) and resources/favdir.icns (macOS) from the icon drawn
// in src/ui/Icons.cpp. Run it after changing the drawing and commit the output:
//
//   cmake --build --preset windows --target make_icons
//   build/windows/bin/Release/make_icons resources
//
// Both formats are written by hand with PNG entries, which Windows Vista and macOS 10.7 and
// later read; Qt has no writer for .icns.

#include "ui/Icons.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QtEndian>

#include <cstdio>

namespace {

QByteArray png(int size)
{
    QByteArray data;
    QBuffer buffer(&data);
    buffer.open(QIODevice::WriteOnly);
    Icons::render(size).save(&buffer, "PNG");
    return data;
}

template <typename T>
void appendLE(QByteArray &out, T value)
{
    char bytes[sizeof(T)];
    qToLittleEndian(value, bytes);
    out.append(bytes, sizeof(T));
}

template <typename T>
void appendBE(QByteArray &out, T value)
{
    char bytes[sizeof(T)];
    qToBigEndian(value, bytes);
    out.append(bytes, sizeof(T));
}

QByteArray ico()
{
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    const quint16 count = std::size(sizes);

    QByteArray header;
    appendLE<quint16>(header, 0);  // reserved
    appendLE<quint16>(header, 1);  // type: icon
    appendLE<quint16>(header, count);

    QByteArray images;
    quint32 offset = 6 + 16 * count;
    for (int size : sizes) {
        const QByteArray data = png(size);
        header.append(char(size >= 256 ? 0 : size));  // 0 means 256
        header.append(char(size >= 256 ? 0 : size));
        header.append(char(0));  // palette size
        header.append(char(0));  // reserved
        appendLE<quint16>(header, 1);   // colour planes
        appendLE<quint16>(header, 32);  // bits per pixel
        appendLE<quint32>(header, quint32(data.size()));
        appendLE<quint32>(header, offset);
        offset += quint32(data.size());
        images.append(data);
    }
    return header + images;
}

QByteArray icns()
{
    struct Entry
    {
        const char *type;
        int size;
    };
    const Entry entries[] = {
        {"icp4", 16},  {"ic11", 32},  {"icp5", 32},  {"ic12", 64},  {"ic07", 128},
        {"ic13", 256}, {"ic08", 256}, {"ic14", 512}, {"ic09", 512}, {"ic10", 1024},
    };

    QByteArray body;
    for (const Entry &e : entries) {
        const QByteArray data = png(e.size);
        body.append(e.type, 4);
        appendBE<quint32>(body, quint32(8 + data.size()));
        body.append(data);
    }
    QByteArray out("icns", 4);
    appendBE<quint32>(out, quint32(8 + body.size()));
    return out + body;
}

bool write(const QString &path, const QByteArray &data)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) {
        std::fprintf(stderr, "cannot write %s\n", qPrintable(path));
        return false;
    }
    std::printf("wrote %s (%lld bytes)\n", qPrintable(QDir::toNativeSeparators(path)),
                static_cast<long long>(data.size()));
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    const QDir out(argc > 1 ? QString::fromLocal8Bit(argv[1]) : QStringLiteral("resources"));
    if (!out.exists()) {
        std::fprintf(stderr, "output folder %s does not exist\n", qPrintable(out.path()));
        return 1;
    }
    const bool ok = write(out.filePath(QStringLiteral("favdir.ico")), ico())
                    && write(out.filePath(QStringLiteral("favdir.icns")), icns());
    return ok ? 0 : 1;
}
