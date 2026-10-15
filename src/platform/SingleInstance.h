#pragma once

#include <QObject>
#include <QString>

class QLocalServer;

// Keeps favdir to one running copy per user. The first instance listens on a local socket;
// later ones tell it to show its window and exit.
class SingleInstance : public QObject
{
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);

    // Returns true when this is the first instance. Otherwise asks the running instance to
    // show itself and returns false.
    bool start();

signals:
    void showRequested();

private:
    QString m_name;
    QLocalServer *m_server = nullptr;
};
