// File/layer: app/qttunebridge.h | C++17, Qt 6.12+, MSVC/Clang/GCC
#pragma once

#include <QObject>
#include <QString>
#include <QAbstractListModel>
#include <QSortFilterProxyModel>
#include <QVariantMap>
#include <QTimer>

#include <atomic>
#include <vector>

#include "qttune/core.h"

class FrameListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit FrameListModel(QObject* parent = nullptr);
    static constexpr int MaxFrames = 10000;

    enum FrameRoles {
        TimestampRole = Qt::UserRole + 1,
        DlcRole,
        ExtendedRole,
        PayloadRole
    };

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void appendFrame(const QVariantMap& frame);
    void clear();
    int count() const { return static_cast<int>(m_frames.size()); }

signals:
    void countChanged(int newCount);

private:
    std::vector<QVariantMap> m_frames;
};

class FrameSortProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    Q_PROPERTY(bool newestFirst READ newestFirst WRITE setNewestFirst NOTIFY newestFirstChanged)

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

class SignalListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int subscribedCount READ subscribedCount NOTIFY subscribedCountChanged)

public:
    explicit SignalListModel(QObject* parent = nullptr);

    enum SignalRoles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        UnitRole,
        ValueRole,
        SubscribedRole
    };

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setSignalSet(const void* set);  // opaque pointer to QttuneSignalSet
    void updateValues(const std::vector<std::pair<uint16_t, float>>& values);
    int subscribedCount() const { return static_cast<int>(m_subscribedCount); }

    Q_INVOKABLE void toggleSubscribed(int row);

signals:
    void valuesUpdated();
    void subscribedCountChanged();

private:
    struct Entry {
        uint16_t id;
        char name[32];
        char unit[16];
        float value = 0.0f;
        bool subscribed = true;
    };

    std::vector<Entry> m_entries;
    int m_subscribedCount = 0;
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
    Q_PROPERTY(int signalBatchCount READ signalBatchCount NOTIFY signalBatchChanged)
    Q_PROPERTY(SignalListModel* signalModel READ signalModel CONSTANT)

public:
    explicit QtTuneBridge(QObject* parent = nullptr);
    ~QtTuneBridge() override;

    QString coreVersion() const;
    bool coreInitialized() const;
    bool connected() const { return m_connected.load(std::memory_order_acquire); }
    int frameCount() const { return m_currentFrameCount; }
    int droppedFrames() const { return m_droppedFrames.load(std::memory_order_relaxed); }
    FrameSortProxy* model() const { return m_sortProxy; }
    int guiNotifyCount() const { return m_guiNotifyCount; }
    int signalBatchCount() const { return m_signalBatchCount; }
    SignalListModel* signalModel() const { return m_signalModel; }

    Q_INVOKABLE QString statusString(int statusCode) const;
    Q_INVOKABLE int maxFrameCount() const { return FrameListModel::MaxFrames; }
    Q_INVOKABLE void forceFrameCountForTesting(int count);

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
    void signalBatchChanged();

private:
    static void onFrame(const qttune_frame_t* frame, void* user_data);
    void handleFrameInternal(const qttune_frame_t* frame);

    bool m_initialized = false;
    qttune_session_t* m_session = nullptr;
    QString m_currentUri;

    std::atomic<bool> m_connected{false};
    std::atomic<int> m_droppedFrames{0};
    int m_currentFrameCount = 0;
    int m_lastReportedDropped = 0;

    FrameListModel* m_sourceModel = nullptr;
    FrameSortProxy* m_sortProxy = nullptr;

    QTimer* m_coalesceTimer = nullptr;
    std::vector<std::pair<uint16_t, float>> m_stagingBuffer;
    int m_signalBatchCount = 0;

    SignalListModel* m_signalModel = nullptr;
    int m_guiNotifyCount = 0;

    std::atomic<int> m_inFlightFrames{0};   // posted-but-not-yet-processed count
    static constexpr int MaxInFlightFrames = 1000;   // queue backlog bound
};
