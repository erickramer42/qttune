#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QThread>
#include <QVariant>

#include <atomic>

#include "qttunebridge.h"   // QtTuneBridge + FrameListModel

namespace {

/* Spin the GUI event loop for `ms`, letting queued invocations drain.
 * Without this, worker-posted lambdas never execute. */
void spinLoop(int ms)
{
    QEventLoop loop;
    QTimer::singleShot(ms, &loop, &QEventLoop::quit);
    loop.exec();
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);   // ONE instance, lives for the process
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(BridgeTest, LifecycleIsIdempotent)
{
    QtTuneBridge bridge;

    EXPECT_FALSE(bridge.connected());

    EXPECT_TRUE(bridge.connectSession("mock://test").isEmpty());
    EXPECT_TRUE(bridge.connected());

    // Double connect = idempotent success, no new session
    EXPECT_TRUE(bridge.connectSession("mock://test").isEmpty());
    EXPECT_TRUE(bridge.connected());

    bridge.disconnectSession();
    EXPECT_FALSE(bridge.connected());

    bridge.disconnectSession();   // double + triple disconnect: no-op
    bridge.disconnectSession();
    EXPECT_FALSE(bridge.connected());

    // Reconnect after disconnect must work (fresh session path)
    EXPECT_TRUE(bridge.connectSession("mock://test").isEmpty());
    EXPECT_TRUE(bridge.connected());
    bridge.disconnectSession();
}

TEST(BridgeTest, InvalidUriReturnsError)
{
    QtTuneBridge bridge;
    EXPECT_FALSE(bridge.connectSession("j2534://dev0").isEmpty());   // no transport
    EXPECT_FALSE(bridge.connected());

    EXPECT_FALSE(bridge.connectSession("").isEmpty());               // bad URI
    EXPECT_FALSE(bridge.connected());
}

TEST(BridgeTest, FramesMarshalFromWorkerToGuiThread)
{
    QtTuneBridge bridge;

    // Connect to SOURCE MODEL's countChanged — proxy forwards changes automatically
    std::atomic<Qt::HANDLE> append_thread{nullptr};
    QObject::connect(
        static_cast<FrameListModel*>(bridge.model()->sourceModel()),
        &FrameListModel::countChanged,
        [&append_thread](int) {
            Qt::HANDLE current = QThread::currentThreadId();
            Qt::HANDLE expected = nullptr;
            append_thread.compare_exchange_strong(expected, current);
        });

    ASSERT_TRUE(bridge.connectSession("mock://test").isEmpty());
    spinLoop(300);   // ~6 frames at 50ms mock interval

    // Frames flowed...
    EXPECT_GT(bridge.frameCount(), 0);
    // ...and they landed on the thread running this event loop.
    EXPECT_EQ(append_thread.load(), QThread::currentThreadId());
    EXPECT_NE(append_thread.load(), nullptr);

    const int frames_before = bridge.frameCount();

    // Mid-stream disconnect: late-arriving frames must be dropped,
    // not crash, not append.
    bridge.disconnectSession();
    spinLoop(200);   // mock would have produced ~4 more frames
    EXPECT_EQ(bridge.frameCount(), frames_before);
}

TEST(BridgeTest, ClearFramesResets)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://test").isEmpty());
    spinLoop(200);
    ASSERT_GT(bridge.frameCount(), 0);

    bridge.clearFrames();
    EXPECT_EQ(bridge.frameCount(), 0);
    EXPECT_EQ(bridge.model()->rowCount(), 0);  // <-- changed from count()

    bridge.disconnectSession();
}

TEST(BridgeTest, SortProxyOrdersNewestFirstByDefault)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://sort-default").isEmpty());
    spinLoop(300);

    FrameSortProxy* proxy = bridge.model();
    EXPECT_GT(proxy->rowCount(), 2);      // <-- changed from count()
    EXPECT_TRUE(proxy->newestFirst());

    const quint64 first = proxy->data(proxy->index(0, 0),
        FrameListModel::TimestampRole).toULongLong();
    const quint64 last = proxy->data(proxy->index(proxy->rowCount() - 1, 0),
        FrameListModel::TimestampRole).toULongLong();
    EXPECT_GT(first, last);

    bridge.disconnectSession();
}

TEST(BridgeTest, SortProxyTogglesToOldestFirst)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://sort-toggle").isEmpty());
    spinLoop(300);

    FrameSortProxy* proxy = bridge.model();
    proxy->setNewestFirst(false);
    EXPECT_FALSE(proxy->newestFirst());

    const quint64 first = proxy->data(proxy->index(0, 0),
        FrameListModel::TimestampRole).toULongLong();
    const quint64 last = proxy->data(proxy->index(proxy->rowCount() - 1, 0),
        FrameListModel::TimestampRole).toULongLong();
    EXPECT_LT(first, last);

    // Toggle back — must re-sort to descending without reconnect
    proxy->setNewestFirst(true);

    // Capture BOTH ends fresh
    const quint64 firstAgain = proxy->data(proxy->index(0, 0),
        FrameListModel::TimestampRole).toULongLong();
    const quint64 lastAgain = proxy->data(proxy->index(proxy->rowCount() - 1, 0),
        FrameListModel::TimestampRole).toULongLong();

    // Top is the newest we've ever seen (>= because the stream kept ticking)
    EXPECT_GE(firstAgain, last);
    // Bottom is the oldest — at most the original ascending-order top,
    // and strictly older than the top of the list
    EXPECT_LE(lastAgain, first);      // 'first' = oldest captured earlier
    EXPECT_GT(firstAgain, lastAgain); // reversal actually happened

    bridge.disconnectSession();
}

TEST(BridgeTest, NotifyCountTracksFrameProcessing)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://notify").isEmpty());
    spinLoop(300);
    EXPECT_EQ(bridge.guiNotifyCount(), bridge.frameCount());
    EXPECT_GT(bridge.guiNotifyCount(), 0);
    bridge.disconnectSession();
}

TEST(BridgeTest, SignalBatchesAreCoalesced)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://coalesce").isEmpty());
    spinLoop(1100);   // ~22 frames at 50ms

    // ~11 batches at 10Hz, with generous scheduling slack
    EXPECT_GT(bridge.signalBatchCount(), 4);
    EXPECT_LT(bridge.signalBatchCount(), 25);
    // Bounded ranges, not exact equality — timing contract, not coincidence
    // (engineering note 2026-10-05, lesson 3)

    // Values actually flowed through the model
    EXPECT_GT(bridge.signalModel()->rowCount(), 0);

    bridge.disconnectSession();
}

TEST(BridgeTest, StreamingContinuesPastTenThousandFrames)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://longrun").isEmpty());
    spinLoop(150);   // let a few frames flow first

    const int before = bridge.frameCount();
    ASSERT_GT(before, 0);

    // Simulate the old freeze boundary: cumulative count just below 10k.
    // Under the OLD gate, the next frame would hit >= MaxFrames and
    // every subsequent frame would be dropped at source, forever.
    bridge.forceFrameCountForTesting(9999);

    spinLoop(600);   // ~12 more frames at 50ms

    // The stream must still be ALIVE: count advanced past the boundary.
    // With the in-flight gate, 9999 is just a number nobody consults.
    EXPECT_GT(bridge.frameCount(), 10000);

    // And no drops occurred — nothing was rejected at the boundary
    EXPECT_EQ(bridge.droppedFrames(), 0);

    // Liveness, not coincidence: the log model kept receiving rows too
    EXPECT_GT(bridge.model()->rowCount(), 0);

    bridge.disconnectSession();
}

TEST(BridgeTest, DispatchedShortFrameHonorsDataLength)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://short").isEmpty());

    // Unique timestamp so we can find OUR row among mock frames
    qttune_frame_t f{};
    f.timestamp_us = 777777;   // steady-clock µs from the mock won't collide
    f.channel_id = 0;
    f.data_length = 4;          // the whole point: NOT 8
    f.is_extended = 1;
    f.data[0] = 0x34;           // arbitrary RPM-looking bytes
    f.data[1] = 0x12;
    f.data[2] = 0xAA;
    f.data[3] = 0x55;

    bridge.dispatchFrameForTesting(f);
    spinLoop(200);              // let the queued dispatch + coalesce tick run

    // Find our row in the (sorted) proxy
    FrameSortProxy* proxy = bridge.model();
    int foundRow = -1;
    for (int r = 0; r < proxy->rowCount(); ++r) {
        if (proxy->data(proxy->index(r, 0),
                FrameListModel::TimestampRole).toULongLong() == 777777) {
            foundRow = r;
            break;
        }
    }
    ASSERT_NE(foundRow, -1) << "dispatched frame never reached the model";

    const QModelIndex idx = proxy->index(foundRow, 0);
    // DLC must reflect the ACTUAL length, not an assumed 8
    EXPECT_EQ(proxy->data(idx, FrameListModel::DlcRole).toUInt(), 4u);
    // Hex payload is exactly 4 bytes -> 8 hex chars, not 16
    EXPECT_EQ(proxy->data(idx, FrameListModel::PayloadRole).toString().size(), 8);
    EXPECT_TRUE(proxy->data(idx, FrameListModel::ExtendedRole).toBool());

    bridge.disconnectSession();
}

TEST(BridgeTest, BackpressureDropsAreCountedAndReported)
{
    QtTuneBridge bridge;
    ASSERT_TRUE(bridge.connectSession("mock://drops").isEmpty());

    // Lambda-counter instead of QSignalSpy: avoids a new Qt6::Test dependency
    int notifyCount = 0;
    int lastReported = -1;
    QObject::connect(&bridge, &QtTuneBridge::droppedFramesChanged,
                     [&](int dropped) { ++notifyCount; lastReported = dropped; });

    qttune_frame_t f{};
    f.data_length = 8;

    const int inject = QtTuneBridge::maxInFlightForTesting() + 500;

    // Synchronous flood from the test thread: no event processing happens,
    // so in-flight climbs monotonically to the cap and stays there.
    for (int i = 0; i < inject; ++i) {
        f.timestamp_us = 1000000 + static_cast<uint64_t>(i);
        bridge.dispatchFrameForTesting(f);
    }

    // Drops happened during saturation (loop 1001..1500 hit the gate).
    // GE not EQ: the mock worker may contribute a frame or two mid-loop.
    EXPECT_GE(bridge.droppedFrames(), 500);

    // Nothing could have been reported yet — the queue hasn't been processed
    EXPECT_EQ(notifyCount, 0);

    spinLoop(800);   // drain the 1000 queued invocations

    // Accounting survived the drain
    EXPECT_GE(bridge.droppedFrames(), 500);
    // The consolidated notification arrived with the final value
    EXPECT_GE(notifyCount, 1);
    EXPECT_EQ(lastReported, bridge.droppedFrames());
    // The queued frames were actually processed, not silently lost
    EXPECT_GT(bridge.frameCount(), 900);
    // Drained backlog frees the gate again — no permanent wedge
    // (the running mock resumes normal delivery after this)

    bridge.disconnectSession();
}
