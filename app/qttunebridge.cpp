#include "qttunebridge.h"

#include "qttune/version.h"

#include <QMetaObject>
#include <QByteArray>

// ---------------- FrameListModel ----------------

FrameListModel::FrameListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    m_frames.reserve(MaxFrames);
}

int FrameListModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_frames.size());
}

QVariant FrameListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_frames.size())) {
        return QVariant();
    }

    const QVariantMap& frame = m_frames[static_cast<size_t>(index.row())];
    switch (role) {
        case TimestampRole: return frame.value("timestampUs");
        case DlcRole:       return frame.value("dlc");
        case ExtendedRole:  return frame.value("extended");
        case PayloadRole:   return frame.value("payloadHex");
        default:            return QVariant();
    }
}

QHash<int, QByteArray> FrameListModel::roleNames() const
{
    return {
        {TimestampRole, "timestampUs"},
        {DlcRole,       "dlc"},
        {ExtendedRole,  "extended"},
        {PayloadRole,   "payloadHex"}
    };
}

void FrameListModel::appendFrame(const QVariantMap& frame)
{
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_frames.push_back(frame);
    endInsertRows();

    // Capacity: evict OLDEST, which in arrival order is at the FRONT
    while (static_cast<int>(m_frames.size()) > MaxFrames) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_frames.erase(m_frames.begin());
        endRemoveRows();
    }

    emit countChanged(static_cast<int>(m_frames.size()));
}

void FrameListModel::clear()
{
    if (m_frames.empty()) {
        return;
    }
    beginResetModel();
    m_frames.clear();
    endResetModel();
    emit countChanged(0);
}

// ---------------- QtTuneBridge ----------------

QtTuneBridge::QtTuneBridge(QObject* parent)
    : QObject(parent)
    , m_sourceModel(new FrameListModel(this))
    , m_sortProxy(new FrameSortProxy(this))
{
    m_initialized = (qttune_init() == QT_OK);
    m_sortProxy->setSourceModel(m_sourceModel);
}

QtTuneBridge::~QtTuneBridge()
{
    // Ordering constraint: session close FIRST. The worker join happens
    // inside close, so no queued lambda can arrive after model teardown.
    if (m_session != nullptr) {
        qttune_session_close(m_session);
        m_session = nullptr;
    }

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

QString QtTuneBridge::connectSession(const QString& uri)
{
    if (m_connected.load(std::memory_order_acquire)) {
        return {};  // idempotent success
    }

    // Named local: uri.toUtf8() returns a temporary; binding it keeps the
    // buffer alive for the whole function scope.
    const QByteArray uri8 = uri.toUtf8();

    qttune_session_t* session = nullptr;
    qttune_status_t rc = qttune_session_create(uri8.constData(), &session);
    if (rc != QT_OK || session == nullptr) {
        return QString::fromUtf8(qttune_status_string(rc));
    }
    m_session = session;
    m_currentUri = uri;

    rc = qttune_register_frame_callback(session, &QtTuneBridge::onFrame, this);
    if (rc != QT_OK) {
        qttune_session_close(session);
        m_session = nullptr;
        return QString::fromUtf8(qttune_status_string(rc));
    }

    rc = qttune_session_start(session);
    if (rc != QT_OK) {
        qttune_unregister_frame_callback(session, &QtTuneBridge::onFrame, this);
        qttune_session_close(session);
        m_session = nullptr;
        return QString::fromUtf8(qttune_status_string(rc));
    }

    m_connected.store(true, std::memory_order_release);
    m_currentFrameCount = 0;
    m_droppedFrames.store(0, std::memory_order_relaxed);
    m_lastReportedDropped = 0;
    emit connectedChanged(true);
    emit framesReset();

    return {};
}

void QtTuneBridge::disconnectSession()
{
    if (!m_connected.load(std::memory_order_acquire)) {
        return;
    }

    m_connected.store(false, std::memory_order_release);   // drop stragglers

    if (m_session != nullptr) {
        qttune_session_close(m_session);   // joins mock worker
        m_session = nullptr;
    }

    emit connectedChanged(false);
}

void QtTuneBridge::clearFrames()
{
    if (m_sourceModel) {
        m_sourceModel->clear();
    }
    m_currentFrameCount = 0;
    emit framesReset();
}

void QtTuneBridge::onFrame(const qttune_frame_t* frame, void* user_data)
{
    QtTuneBridge* self = static_cast<QtTuneBridge*>(user_data);
    if (frame == nullptr || self == nullptr) {
        return;
    }
    if (!self->m_connected.load(std::memory_order_acquire)) {
        return;
    }
    if (self->m_currentFrameCount >= FrameListModel::MaxFrames) {
        self->m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const qttune_frame_t copy = *frame;   // frame is stack-owned by mock_worker

    QMetaObject::invokeMethod(self, [self, copy]() {
        self->handleFrameInternal(&copy);
    }, Qt::QueuedConnection);
}

void QtTuneBridge::handleFrameInternal(const qttune_frame_t* frame)
{
    if (!m_connected.load(std::memory_order_acquire)) {
        return;
    }

    QVariantMap vm;
    vm["timestampUs"] = QVariant::fromValue<quint64>(frame->timestamp_us);
    vm["dlc"]         = static_cast<uint32_t>(frame->data_length);
    vm["extended"]    = frame->is_extended != 0;
    vm["payloadHex"]  = QByteArray(
        reinterpret_cast<const char*>(frame->data),
        static_cast<qsizetype>(frame->data_length)).toHex();

    m_sourceModel->appendFrame(vm);
    ++m_currentFrameCount;
    emit frameCountChanged();

    // and sync the drop counter to the GUI thread while we're here:
    const int dropped = m_droppedFrames.load(std::memory_order_relaxed);
    if (dropped != m_lastReportedDropped) {
        m_lastReportedDropped = dropped;
        emit droppedFramesChanged(dropped);     // TODO change to coalesce for v0.3 and real data
    }
}

// ---------------- FrameSortProxy ----------------

// QSortFilterProxyModel's default lessThan on a quint64 stored in a QVariant,
// comparison goes through QVariant::compare — for numeric types that's fine in 
// Qt 6, but the safest defense is an explicit lessThan override
bool FrameSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    // Explicit numeric compare on the sort role; never string-compare quint64
    const quint64 lhs = sourceModel()->data(left, FrameListModel::TimestampRole).toULongLong();
    const quint64 rhs = sourceModel()->data(right, FrameListModel::TimestampRole).toULongLong();
    return lhs < rhs;
}

FrameSortProxy::FrameSortProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    // Sort key: timestamp. Comparator must be numeric — QVariant's default
    // comparison would string-compare quint64s and 100 < 99. Use a lambda
    // comparator so timestamps sort numerically.
    setSortRole(FrameListModel::TimestampRole);
    setDynamicSortFilter(true);
    sort(0, Qt::DescendingOrder);   // newestFirst default = true
}

bool FrameSortProxy::newestFirst() const
{
    return m_newestFirst;
}

void FrameSortProxy::setNewestFirst(bool newestFirst)
{
    if (m_newestFirst == newestFirst) {
        return;
    }
    m_newestFirst = newestFirst;
    sort(0, newestFirst ? Qt::DescendingOrder : Qt::AscendingOrder);
    emit newestFirstChanged();
}
