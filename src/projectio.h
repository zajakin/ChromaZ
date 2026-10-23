#ifndef PROJECTIO_H
#define PROJECTIO_H

#include <QByteArray>
#include <QtGlobal>

// Формат файла проекта .ChromaZ:
//   * новый — JSON, сжатый qCompress (4 байта длины + поток zlib);
//   * старый — обычный JSON (UTF-8), поддерживается при открытии для обратной совместимости.
namespace ProjectIO {

inline QByteArray pack(const QByteArray &json, int compressionLevel = 9) {
  return qCompress(json, compressionLevel);
}

// Возвращает JSON из содержимого файла проекта; пустой массив — если формат не распознан или данные повреждены.
inline QByteArray unpack(QByteArray data) {
  if (data.startsWith("\xEF\xBB\xBF")) data.remove(0, 3);          // BOM UTF-8
  if (data.isEmpty()) return QByteArray();

  // Обычный JSON: первый непробельный символ — '{'.
  for (char c : data) {
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') continue;
    if (c == '{') return data;
    break;
  }

  // Сжатый формат qCompress. Перед распаковкой проверяем заголовок, чтобы произвольный файл
  // (например, не проект) не заставил qUncompress выделять гигабайты памяти.
  if (data.size() < 6) return QByteArray();
  const quint32 expected = (quint32(quint8(data[0])) << 24) | (quint32(quint8(data[1])) << 16) |
                           (quint32(quint8(data[2])) << 8)  |  quint32(quint8(data[3]));
  if (expected == 0 || expected > (1u << 30)) return QByteArray();   // > 1 ГиБ — не проект
  const quint8 cmf = quint8(data[4]), flg = quint8(data[5]);
  if ((cmf & 0x0F) != 8 || ((quint32(cmf) << 8) | flg) % 31 != 0) return QByteArray();   // не zlib
  return qUncompress(data);
}

} // namespace ProjectIO

#endif // PROJECTIO_H
