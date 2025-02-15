
#ifndef EVOLVELEGACYREBORNLAUNCHER_SAVEFILEHELPER_H
#define EVOLVELEGACYREBORNLAUNCHER_SAVEFILEHELPER_H


class SaveFileHelper {
public:
    static QString readOriginal2Hex(const QString& filename);
    static bool writeNew2Binary(const QString& replaced, const QString& path);
};


#endif //EVOLVELEGACYREBORNLAUNCHER_SAVEFILEHELPER_H
