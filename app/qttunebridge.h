// File/layer: app/qttunebridge.h | C++17, Qt 6.12+, MSVC/Clang/GCC
#pragma once

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVariantMap>

#include <atomic>
#include <vector>

#include "qttune/core.h"   // qttune_frame_t, qttune_session_t — typedef'd
                           // anonymous structs; cannot be forward-declared.

/**
 * Canonical frame storage. Rows NEVER reorder: arrival order, monotonic.
 * Newest appended at back; back-eviction at capacity. Ordering/filtering
 * is a view concern handled by FrameSortProxy.
 */
class FrameListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit FrameListModel(QObject* parent = nullptr);

    static constexpr int MaxFrames = 10000;   // backpressure limit (public: bridge reads)

    enum FrameRoles {
        TimestampRole = Qt::UserRole + 1,
        DlcRole,
        ExtendedRole,
        PayloadRole
    };

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendFrame(const QVariantMap& frame);   // O(1) amortized, back-eviction
    void clear();
    int count() const { return static_cast<int>(m_frames.size()); }

signals:
    void countChanged(int newCount);

private:
    std::vector<QVariantMap> m_frames;
};

/**
 * View-facing ordering. Wraps FrameListModel; never mutates storage.
 * newestFirst=true (default): live log, newest at top, view stays at
 * origin — zero scroll management. newestFirst=false: chronological
 * reading; auto-follow/jump-to-latest is a future LogPage feature.
 */
class FrameSortProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool newestFirst READ newestFirst WRITE setNewestFirst
               NOTIFY newestFirstChanged)

public:
    explicit FrameSortProxy(QObject* parent = nullptr);

    bool newestFirst() const;
    void setNewestFirst(bool newestFirst);

protected:
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

signals:
    void newestFirstChanged();

private:
    bool m_newestFirst = true;
};

class QtTuneBridge : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString coreVersion READ coreVersion CONSTANT)
    Q_PROPERTY(bool coreInitialized READ coreInitialized NOTIFY coreInitializedChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(int frameCount READ frameCount NOTIFY frameCountChanged)
    Q_PROPERTY(int droppedFrames READ droppedFrames NOTIFY droppedFramesChanged)
    Q_PROPERTY(FrameSortProxy* model READ model CONSTANT)
    Q_PROPERTY(int guiNotifyCount READ guiNotifyCount NOTIFY notifyCountChanged)

public:
    explicit QtTuneBridge(QObject* parent = nullptr);
    ~QtTuneBridge() override;

    QString coreVersion() const;
    bool coreInitialized() const;
    bool connected() const { return m_connected.load(std::memory_order_acquire); }
    int frameCount() const { return m_currentFrameCount; }
    int droppedFrames() const { return m_droppedFrames.load(std::memory_order_relaxed); }
    FrameSortProxy* model() const { return m_sortProxy; }   // QML binds the PROXY
    int guiNotifyCount() const { return m_guiNotifyCount; }

    Q_INVOKABLE QString statusString(int statusCode) const;
    Q_INVOKABLE int maxFrameCount() const { return FrameListModel::MaxFrames; }

public slots:
    Q_INVOKABLE QString connectSession(const QString& uri);
    Q_INVOKABLE void disconnectSession();
    Q_INVOKABLE void clearFrames();

signals:
    void coreInitializedChanged();
    void connectedChanged(bool);
    void frameCountChanged();
    void framesReset();
    void droppedFramesChanged(int);
    void notifyCountChanged(int newCount);

private:
    static void onFrame(const qttune_frame_t* frame, void* user_data);   // worker thread
    void handleFrameInternal(const qttune_frame_t* frame);               // GUI thread

    bool m_initialized = false;
    qttune_session_t* m_session = nullptr;   // closed in dtor BEFORE model teardown
    QString m_currentUri;

    std::atomic<bool> m_connected{false};
    std::atomic<int> m_droppedFrames{0};
    int m_currentFrameCount = 0;
    int m_lastReportedDropped = 0;           // GUI-thread-only mirror

    FrameListModel* m_sourceModel = nullptr;  // canonical storage
    FrameSortProxy* m_sortProxy = nullptr;    // view-facing

    int m_guiNotifyCount = 0;           // incremented per frame notification
    // QAtomicInteger<int> m_notifyRateCounter{0};  // for rate calc (optional, per-second)
};
