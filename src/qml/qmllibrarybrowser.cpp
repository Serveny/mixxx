#include "qml/qmllibrarybrowser.h"

#include <QAbstractItemModel>
#include <QTimer>
#include <QUrl>

#include "library/library.h"
#include "library/sidebarmodel.h"
#include "library/trackmodel.h"
#include "moc_qmllibrarybrowser.cpp"
#include "qml/qmllibraryproxy.h"
#include "util/assert.h"

namespace mixxx {
namespace qml {

namespace {
// Track lists fill in batches (e.g. folders are read in a background thread),
// so changes are collected for this long before the list is sent again.
constexpr int kTracksChangedDelayMillis = 100;
} // namespace

QmlLibraryBrowser::QmlLibraryBrowser(QObject* pParent)
        : QObject(pParent) {
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return;
    }
    auto* pWorker = new QmlLibraryBrowserWorker(pLibrary);
    pWorker->moveToThread(pLibrary->thread());
    connect(pWorker,
            &QmlLibraryBrowserWorker::opened,
            this,
            &QmlLibraryBrowser::opened);
    connect(pWorker,
            &QmlLibraryBrowserWorker::tracksChanged,
            this,
            &QmlLibraryBrowser::tracksChanged);
    m_pWorker = pWorker;
}

QmlLibraryBrowser::~QmlLibraryBrowser() {
    if (m_pWorker) {
        m_pWorker->deleteLater();
    }
}

void QmlLibraryBrowser::open(const QList<int>& path) {
    QmlLibraryBrowserWorker* pWorker = m_pWorker;
    if (!pWorker) {
        return;
    }
    QMetaObject::invokeMethod(
            pWorker,
            [pWorker, path]() {
                pWorker->open(path);
            },
            Qt::QueuedConnection);
}

QmlLibraryBrowserWorker::QmlLibraryBrowserWorker(Library* pLibrary)
        : m_pLibrary(pLibrary),
          m_pTracksChangedTimer(new QTimer(this)),
          m_activating(false) {
    m_pTracksChangedTimer->setSingleShot(true);
    m_pTracksChangedTimer->setInterval(kTracksChangedDelayMillis);
    connect(m_pTracksChangedTimer,
            &QTimer::timeout,
            this,
            [this]() {
                emit tracksChanged(readTracks());
            });
    connect(m_pLibrary,
            &Library::showTrackModel,
            this,
            &QmlLibraryBrowserWorker::slotShowTrackModel);
}

void QmlLibraryBrowserWorker::open(const QList<int>& path) {
    SidebarModel* pSidebar = m_pLibrary->sidebarModel();
    VERIFY_OR_DEBUG_ASSERT(pSidebar) {
        return;
    }
    attachTrackModel(nullptr);

    QModelIndex index;
    QList<int> openedPath;
    QStringList labels;
    for (int row : path) {
        // Folders list their children lazily, on expansion.
        if (index.isValid() && pSidebar->rowCount(index) == 0) {
            pSidebar->doubleClicked(index);
        }
        const QModelIndex child = pSidebar->index(row, 0, index);
        if (!child.isValid()) {
            break;
        }
        index = child;
        openedPath.append(row);
        labels.append(index.data().toString());
    }

    if (index.isValid()) {
        // Always re-read the opened folder, so it shows its current subfolders.
        pSidebar->doubleClicked(index);
        // The item's track list is the one shown while activating it.
        m_activating = true;
        pSidebar->clicked(index);
        m_activating = false;
        // Not selected in Mixxx's sidebar: that expands the parent, and expanding a Computer folder
        // rebuilds its children, leaving `index` (and the sidebar's own selection) dangling.
    }

    QStringList children;
    for (int row = 0, count = pSidebar->rowCount(index); row < count; ++row) {
        children.append(pSidebar->index(row, 0, index).data().toString());
    }
    emit opened(openedPath, labels, children);
    emit tracksChanged(readTracks());
}

void QmlLibraryBrowserWorker::slotShowTrackModel(QAbstractItemModel* pModel) {
    if (m_activating) {
        attachTrackModel(pModel);
    }
}

void QmlLibraryBrowserWorker::attachTrackModel(QAbstractItemModel* pModel) {
    m_pTracksChangedTimer->stop();
    if (m_pTrackModel) {
        m_pTrackModel->disconnect(this);
    }
    m_pTrackModel = pModel;
    if (!pModel) {
        return;
    }
    const auto scheduleTracksChanged = [this]() {
        m_pTracksChangedTimer->start();
    };
    connect(pModel, &QAbstractItemModel::modelReset, this, scheduleTracksChanged);
    connect(pModel, &QAbstractItemModel::layoutChanged, this, scheduleTracksChanged);
    connect(pModel, &QAbstractItemModel::rowsInserted, this, scheduleTracksChanged);
    connect(pModel, &QAbstractItemModel::rowsRemoved, this, scheduleTracksChanged);
}

QVariantList QmlLibraryBrowserWorker::readTracks() const {
    QAbstractItemModel* pModel = m_pTrackModel;
    auto* pTrackModel = dynamic_cast<TrackModel*>(pModel);
    if (!pTrackModel) {
        return {};
    }
    const int titleColumn = pTrackModel->columnIndexFromSortColumnId(
            TrackModel::SortColumnId::Title);
    const int artistColumn = pTrackModel->columnIndexFromSortColumnId(
            TrackModel::SortColumnId::Artist);
    const int bpmColumn = pTrackModel->columnIndexFromSortColumnId(
            TrackModel::SortColumnId::Bpm);
    const int keyColumn = pTrackModel->columnIndexFromSortColumnId(
            TrackModel::SortColumnId::Key);
    const auto read = [pModel](int row, int column) {
        return column < 0 ? QVariant() : pModel->index(row, column).data();
    };

    QVariantList tracks;
    const int count = pModel->rowCount();
    tracks.reserve(count);
    for (int row = 0; row < count; ++row) {
        const QUrl url = pTrackModel->getTrackUrl(pModel->index(row, 0));
        QString title = read(row, titleColumn).toString();
        if (title.isEmpty()) {
            title = url.fileName();
        }
        tracks.append(QVariantMap{
                {QStringLiteral("title"), title},
                {QStringLiteral("artist"), read(row, artistColumn).toString()},
                {QStringLiteral("bpm"), read(row, bpmColumn).toDouble()},
                {QStringLiteral("key"), read(row, keyColumn).toString()},
                {QStringLiteral("url"), url},
        });
    }
    return tracks;
}

} // namespace qml
} // namespace mixxx
