#include "qttunebridge.h"

#include "qttune/core.h"
#include "qttune/version.h"

QtTuneBridge::QtTuneBridge(QObject* parent)
    : QObject(parent)
{
    m_initialized = (qttune_init() == QT_OK);
}

QtTuneBridge::~QtTuneBridge()
{
    if (m_initialized) {
        qttune_shutdown();
    }
}

QString QtTuneBridge::coreVersion() const
{
    return QString::fromUtf8(qttune_version_string());
}

bool QtTuneBridge::coreInitialized() const
{
    return m_initialized;
}

QString QtTuneBridge::statusString(int statusCode) const
{
    return QString::fromUtf8(qttune_status_string(static_cast<qttune_status_t>(statusCode)));
}
