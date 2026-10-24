#ifndef PROJECTIO_H
#define PROJECTIO_H

#include <QByteArray>
#include <QtGlobal>

// Resource limits for .ChromaZ projects. Increase only if legitimately large
// Sanger projects need more space.
namespace ProjectIO {

inline constexpr qint64 MaxProjectFileBytes = 256LL * 1024LL * 1024LL;
inline constexpr quint32 MaxUncompressedProjectBytes = 128U * 1024U * 1024U;

inline QByteArray pack(const QByteArray &json, int compressionLevel = 9) {
  if (static_cast<quint64>(json.size()) > MaxUncompressedProjectBytes) {
    return {};
  }
  return qCompress(json, compressionLevel);
}

// Returns an empty array if the input is not a supported project or exceeds limits.
inline QByteArray unpack(const QByteArray &input) {
  if (input.isEmpty() || input.size() > MaxProjectFileBytes) return {};
  
  QByteArray data = input;
  if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3); // UTF-8 BOM
  if (data.isEmpty()) return {};
  
  // Legacy, uncompressed JSON. Bound it too, not just compressed projects.
  for (char c : data) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    if (c == '{') {
      if (static_cast<quint64>(data.size()) > MaxUncompressedProjectBytes)
        return {};
      return data;
    }
    break;
  }
  
  // qCompress format: 4-byte uncompressed length followed by zlib data.
  if (data.size() < 6) return {};
  
  const quint32 expected =
    (quint32(quint8(data[0])) << 24) |
    (quint32(quint8(data[1])) << 16) |
    (quint32(quint8(data[2])) << 8)  |
    quint32(quint8(data[3]));
  
  if (expected == 0 || expected > MaxUncompressedProjectBytes)
    return {};
  
  const quint8 cmf = quint8(data[4]);
  const quint8 flg = quint8(data[5]);
  if ((cmf & 0x0F) != 8 ||
      ((quint32(cmf) << 8) | flg) % 31 != 0) {
    return {};
  }
  
  const QByteArray json = qUncompress(data);
  // Also detect a truncated/broken stream or a misleading length prefix.
  if (json.isEmpty() || static_cast<quint64>(json.size()) != expected)
    return {};
  return json;
}

} // namespace ProjectIO

#endif // PROJECTIO_H