#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QVariantList>

class QAbstractItemModel;
class QTimer;
class Library;

namespace mixxx {
namespace qml {

class QmlLibraryBrowserWorker;

/// Browses Mixxx's library sidebar (Tracks, Playlists, Crates, Computer, History, ...) the way
/// controller screens do: one level at a time, following the item into Mixxx's library view.
///
/// Unlike the other QML library types, this is safe to use from a QML engine outside the GUI
/// thread (controller screens): the library is only accessed in its own thread, and results are
/// delivered as plain values through signals.
class QmlLibraryBrowser : public QObject {
    Q_OBJECT
    QML_NAMED_ELEMENT(LibraryBrowser)

  public:
    explicit QmlLibraryBrowser(QObject* pParent = nullptr);
    ~QmlLibraryBrowser() override;

    /// Expands and activates the sidebar item at `path` (child rows from the top level; empty for
    /// the top level itself). Answered by `opened` and then `tracksChanged`. If `path` no longer
    /// exists, its longest existing prefix is opened instead.
    Q_INVOKABLE void open(const QList<int>& path);

  signals:
    /// `labels` names each item along `path`, `children` the labels of the opened item's children.
    void opened(const QList<int>& path, const QStringList& labels, const QStringList& children);
    /// Tracks of the opened item, as maps with title, artist, bpm, key (display text), keyId
    /// (ChromaticKey), url, played (in this session) and coverArtUrl; empty if the item has no
    /// track list. Emitted again whenever that list changes (folders load asynchronously).
    void tracksChanged(const QVariantList& tracks);

  private:
    QPointer<QmlLibraryBrowserWorker> m_pWorker;
};

/// Does the work of QmlLibraryBrowser in the library's thread.
class QmlLibraryBrowserWorker : public QObject {
    Q_OBJECT

  public:
    explicit QmlLibraryBrowserWorker(Library* pLibrary);

    void open(const QList<int>& path);

  signals:
    void opened(const QList<int>& path, const QStringList& labels, const QStringList& children);
    void tracksChanged(const QVariantList& tracks);

  private slots:
    void slotShowTrackModel(QAbstractItemModel* pModel);

  private:
    void attachTrackModel(QAbstractItemModel* pModel);
    QVariantList readTracks() const;

    Library* const m_pLibrary;
    QTimer* const m_pTracksChangedTimer;
    QPointer<QAbstractItemModel> m_pTrackModel;
    bool m_activating;
};

} // namespace qml
} // namespace mixxx
