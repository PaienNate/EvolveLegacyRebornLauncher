
#include <QString>
#include <QFile>
#include "SaveFileHelper.h"

QString SaveFileHelper::readOriginal2Hex(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return nullptr;
    }

    QByteArray data = file.readAll();
    QString hexString = data.toHex();

    return hexString;
}

bool SaveFileHelper::writeNew2Binary(const QString& replaced, const QString& path) {
    QFile saveFile(path);
    if (!saveFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QByteArray binaryData;
    for (int i = 0; i < replaced.length(); i += 2) {
        bool ok;
        char byte = static_cast<char>(replaced.mid(i, 2).toInt(&ok, 16));

        if (ok) {
            binaryData.append(byte);
        } else {
            return false;
        }
    }

    saveFile.write(binaryData);
    saveFile.close();
    return true;
}