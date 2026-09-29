#pragma once

#include <QObject>
#include <QString>

class QtTuneBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString coreVersion READ coreVersion CONSTANT)
    Q_PROPERTY(bool coreInitialized READ coreInitialized NOTIFY coreInitializedChanged)

public:
    explicit QtTuneBridge(QObject* parent = nullptr);
    ~QtTuneBridge() override;

    QString coreVersion() const;
    bool coreInitialized() const;

    // Phase 2: Q_INVOKABLE connectSession(uri), disconnectSession(), ...
    Q_INVOKABLE QString statusString(int statusCode) const;

signals:
    void coreInitializedChanged();

private:
    bool m_initialized = false;
};
