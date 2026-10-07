#include "qttunebridge.h"
#include "qttune/version.h"
#include "qttune/signals.h"

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

    while (static_cast<int>(m_frames.size()) > MaxFrames) {
        beginRemoveRows(QModelIndex(), 0, 0);
        m_frames.erase(m_frames.begin());
        endRemoveRows();
    }
    emit countChanged(static_cast<int>(m_frames.size()));
}

void FrameListModel::clear()
{
    if (m_frames.empty()) return;
    beginResetModel();
    m_frames.clear();
    endResetModel();
    emit countChanged(0);
}

// ---------------- FrameSortProxy ----------------
bool FrameSortProxy::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const quint64 lhs = sourceModel()->data(left, FrameListModel::TimestampRole).toULongLong();
    const quint64 rhs = sourceModel()->data(right, FrameListModel::TimestampRole).toULongLong();
    return lhs < rhs;
}

FrameSortProxy::FrameSortProxy(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(FrameListModel::TimestampRole);
    setDynamicSortFilter(true);
    sort(0, Qt::DescendingOrder);
}

bool FrameSortProxy::newestFirst() const
{
    return m_newestFirst;
}

void FrameSortProxy::setNewestFirst(bool newestFirst)
{
    if (m_newestFirst == newestFirst) return;
    m_newestFirst = newestFirst;
    sort(0, newestFirst ? Qt::DescendingOrder : Qt::AscendingOrder);
    emit newestFirstChanged();
}

// ---------------- SignalListModel ----------------
SignalListModel::SignalListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int SignalListModel::rowCount(const QModelIndex&) const
{
    return static_cast<int>(m_entries.size());
}

QVariant SignalListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(m_entries.size())) {
        return QVariant();
    }
    const Entry& e = m_entries[static_cast<size_t>(index.row())];
    switch (role) {
        case IdRole: return e.id;
        case NameRole: return QString::fromUtf8(e.name);
        case UnitRole: return QString::fromUtf8(e.unit);
        case ValueRole: return e.value;
        case SubscribedRole: return e.subscribed;
        default: return QVariant();
    }
}

QHash<int, QByteArray> SignalListModel::roleNames() const
{
    return {
        {IdRole, "id"},
        {NameRole, "name"},
        {UnitRole, "unit"},
        {ValueRole, "value"},
        {SubscribedRole, "subscribed"}
    };
}

void SignalListModel::setSignalSet(const void* set)
{
    const QttuneSignalSet* sigSet = static_cast<const QttuneSignalSet*>(set);
    if (!sigSet || !sigSet->defs) return;

    beginResetModel();
    m_entries.clear();
    m_subscribedCount = 0;

    for (uint32_t i = 0; i < sigSet->count; ++i) {
        Entry e = {};
        e.id = sigSet->defs[i].id;
        strncpy(e.name, sigSet->defs[i].name, sizeof(e.name) - 1);
        strncpy(e.unit, sigSet->defs[i].unit, sizeof(e.unit) - 1);
        e.subscribed = true;
        ++m_subscribedCount;
        m_entries.push_back(e);
    }
    endResetModel();
}

void SignalListModel::updateValues(const std::vector<std::pair<uint16_t, float>>& values)
{
    if (m_entries.empty() || values.empty()) return;

    int firstRow = -1;
    int lastRow = -1;
    for (const auto& pair : values) {
        for (size_t i = 0; i < m_entries.size(); ++i) {
            Entry& e = m_entries[i];
            if (e.id == pair.first && e.subscribed) {
                e.value = pair.second;
                if (firstRow < 0 || static_cast<int>(i) < firstRow) firstRow = static_cast<int>(i);
                if (static_cast<int>(i) > lastRow) lastRow = static_cast<int>(i);
            }
        }
    }
    if (firstRow >= 0) {
        // one contiguous range instead of per-row notifications — that's
        // the coalescing promise at the model layer too
        emit dataChanged(index(firstRow), index(lastRow), {ValueRole});
    }
}

void SignalListModel::toggleSubscribed(int row)
{
    if (row < 0 || row >= static_cast<int>(m_entries.size())) return;
    Entry& e = m_entries[static_cast<size_t>(row)];
    e.subscribed = !e.subscribed;
    if (e.subscribed) ++m_subscribedCount; else --m_subscribedCount;
    emit dataChanged(index(row), index(row), {SubscribedRole});
    emit subscribedCountChanged();
}

// ---------------- QtTuneBridge ----------------
QtTuneBridge::QtTuneBridge(QObject* parent)
    : QObject(parent)
    , m_sourceModel(new FrameListModel(this))
    , m_sortProxy(new FrameSortProxy(this))
    , m_coalesceTimer(new QTimer(this))
    , m_signalModel(new SignalListModel(this))
{
    m_initialized = (qttune_init() == QT_OK);
    m_sortProxy->setSourceModel(m_sourceModel);

    // Coalescing timer: 100ms window for batched signal updates
    connect(m_coalesceTimer, &QTimer::timeout, this, [this]() {
        if (!m_stagingBuffer.empty()) {
            m_signalModel->updateValues(m_stagingBuffer);
            m_stagingBuffer.clear();
            ++m_signalBatchCount;
            emit signalBatchChanged();
        }
    });
}

QtTuneBridge::~QtTuneBridge()
{
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
    if (m_connected.load(std::memory_order_acquire)) return {};

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
    m_signalModel->setSignalSet(qttune_mock_signal_set());
    m_coalesceTimer->start(100);  // <-- start batching timer
    emit connectedChanged(true);
    emit framesReset();
    return {};
}

void QtTuneBridge::disconnectSession()
{
    if (!m_connected.load(std::memory_order_acquire)) return;

    m_connected.store(false, std::memory_order_release);
    if (m_session != nullptr) {
        qttune_session_close(m_session);
        m_session = nullptr;
    }
    m_coalesceTimer->stop();  // <-- stop batching timer
    emit connectedChanged(false);
}

void QtTuneBridge::clearFrames()
{
    if (m_sourceModel) m_sourceModel->clear();
    m_currentFrameCount = 0;
    emit framesReset();
}

// qttunebridge.cpp
void QtTuneBridge::onFrame(const qttune_frame_t* frame, void* user_data)
{
    QtTuneBridge* self = static_cast<QtTuneBridge*>(user_data);
    if (!frame || !self) return;
    if (!self->m_connected.load(std::memory_order_acquire)) return;

    // Backpressure: bound the QUEUED backlog, not the lifetime count.
    // The model handles total memory via its own ring eviction.
    if (self->m_inFlightFrames.load(std::memory_order_relaxed) >= MaxInFlightFrames) {
        self->m_droppedFrames.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    const qttune_frame_t copy = *frame;
    self->m_inFlightFrames.fetch_add(1, std::memory_order_relaxed);

    QMetaObject::invokeMethod(self, [self, copy]() {
        self->m_inFlightFrames.fetch_sub(1, std::memory_order_relaxed);
        self->handleFrameInternal(&copy);
    }, Qt::QueuedConnection);
}

void QtTuneBridge::handleFrameInternal(const qttune_frame_t* frame)
{
    if (!m_connected.load(std::memory_order_acquire)) return;

    QVariantMap vm;
    vm["timestampUs"] = QVariant::fromValue<quint64>(frame->timestamp_us);
    vm["dlc"] = static_cast<uint32_t>(frame->data_length);
    vm["extended"] = frame->is_extended != 0;
    vm["payloadHex"] = QByteArray(
        reinterpret_cast<const char*>(frame->data),
        static_cast<qsizetype>(frame->data_length)).toHex();

    m_sourceModel->appendFrame(vm);
    ++m_currentFrameCount;

    // Decode signals into staging buffer
    const QttuneSignalSet* set = qttune_mock_signal_set();
    m_stagingBuffer.clear();
    for (uint32_t i = 0; i < set->count; ++i) {
        const QttuneSignalDef& def = set->defs[i];
        float val = 0.0f;
        if (qttune_decode_signal(&def, frame->data, 8, &val) == 0) {
            m_stagingBuffer.emplace_back(def.id, val);
        }
    }

    ++m_guiNotifyCount;
    emit frameCountChanged();
    emit notifyCountChanged(m_guiNotifyCount);

    const int dropped = m_droppedFrames.load(std::memory_order_relaxed);
    if (dropped != m_lastReportedDropped) {
        m_lastReportedDropped = dropped;
        emit droppedFramesChanged(dropped);
    }
}

void QtTuneBridge::forceFrameCountForTesting(int count)
{
    if (count < 0) {
        return;
    }
    m_currentFrameCount = count;
}
