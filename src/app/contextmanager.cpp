/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <agateau@kde.org>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "contextmanager.moc"

// Qt
#include <QItemSelectionModel>
#include <QTimer>
#include <QUndoGroup>

// KDE
#include <KDebug>
#include <KFileItem>

// Local
#include "sidebar.h"
#include "abstractcontextmanageritem.h"
#include <lib/document/documentfactory.h>
#include <lib/semanticinfo/sorteddirmodel.h>

namespace Gwenview
{

struct ContextManagerPrivate
{
    QList<AbstractContextManagerItem*> mList;
    SortedDirModel* mDirModel;
    QItemSelectionModel* mSelectionModel;
    KUrl mCurrentDirUrl;
    KUrl mCurrentUrl;

    bool mSelectedFileItemListNeedsUpdate;
    QSet<QByteArray> mQueuedSignals;
    KFileItemList mSelectedFileItemList;

    QTimer* mQueuedSignalsTimer;

    void queueSignal(const QByteArray& signal)
    {
        mQueuedSignals << signal;
        mQueuedSignalsTimer->start();
    }

    void updateSelectedFileItemList()
    {
        if (!mSelectedFileItemListNeedsUpdate) {
            return;
        }
        mSelectedFileItemList.clear();
        QItemSelection selection = mSelectionModel->selection();
        Q_FOREACH(const QModelIndex & index, selection.indexes()) {
            mSelectedFileItemList << mDirModel->itemForIndex(index);
        }

        // At least add current url if it's valid (it may not be in
        // the list if we are viewing a non-browsable url, for example
        // using http protocol)
        if (mSelectedFileItemList.isEmpty() && mCurrentUrl.isValid()) {
            KFileItem item(KFileItem::Unknown, KFileItem::Unknown, mCurrentUrl);
            mSelectedFileItemList << item;
        }

        mSelectedFileItemListNeedsUpdate = false;
    }
};

ContextManager::ContextManager(SortedDirModel* dirModel, QItemSelectionModel* selectionModel, QObject* parent)
: QObject(parent)
, d(new ContextManagerPrivate)
{
    d->mQueuedSignalsTimer = new QTimer(this);
    d->mQueuedSignalsTimer->setInterval(100);
    d->mQueuedSignalsTimer->setSingleShot(true);
    connect(d->mQueuedSignalsTimer, SIGNAL(timeout()),
            SLOT(emitQueuedSignals()));

    d->mDirModel = dirModel;
    connect(d->mDirModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)),
            SLOT(slotDirModelDataChanged(QModelIndex,QModelIndex)));

    d->mSelectionModel = selectionModel;
    connect(d->mSelectionModel, SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(slotSelectionChanged()));
    connect(d->mSelectionModel, SIGNAL(currentChanged(QModelIndex,QModelIndex)),
            SLOT(slotCurrentChanged(QModelIndex)));

    d->mSelectedFileItemListNeedsUpdate = false;
}

ContextManager::~ContextManager()
{
    qDeleteAll(d->mList);
    delete d;
}

void ContextManager::addItem(AbstractContextManagerItem* item)
{
    d->mList << item;
}

void ContextManager::setCurrentUrl(const KUrl& currentUrl)
{
    if (d->mCurrentUrl == currentUrl) {
        return;
    }

    d->mCurrentUrl = currentUrl;
    Document::Ptr doc = DocumentFactory::instance()->load(currentUrl);
    QUndoGroup* undoGroup = DocumentFactory::instance()->undoGroup();
    undoGroup->addStack(doc->undoStack());
    undoGroup->setActiveStack(doc->undoStack());

    d->queueSignal("selectionChanged");
}

KFileItemList ContextManager::selectedFileItemList() const
{
    d->updateSelectedFileItemList();
    return d->mSelectedFileItemList;
}

void ContextManager::setCurrentDirUrl(const KUrl& url)
{
    if (url.equals(d->mCurrentDirUrl, KUrl::CompareWithoutTrailingSlash)) {
        return;
    }
    d->mCurrentDirUrl = url;
    currentDirUrlChanged();
}

KUrl ContextManager::currentDirUrl() const
{
    return d->mCurrentDirUrl;
}

KUrl ContextManager::currentUrl() const
{
    return d->mCurrentUrl;
}

SortedDirModel* ContextManager::dirModel() const
{
    return d->mDirModel;
}

void ContextManager::slotDirModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    // Data change can happen in the following cases:
    // - items have been renamed
    // - item bytes have been modified
    // - item meta info has been retrieved or modified
    //
    // If a selected item is affected, schedule emission of a
    // selectionDataChanged() signal. Don't emit it directly to avoid spamming
    // the context items in case of a mass change.
    QModelIndexList selectionList = d->mSelectionModel->selectedIndexes();
    if (selectionList.isEmpty()) {
        return;
    }

    QModelIndexList changedList;
    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        changedList << d->mDirModel->index(row, 0);
    }

    QModelIndexList& shortList = selectionList;
    QModelIndexList& longList = changedList;
    if (shortList.length() > longList.length()) {
        qSwap(shortList, longList);
    }
    Q_FOREACH(const QModelIndex & index, shortList) {
        if (longList.contains(index)) {
            d->mSelectedFileItemListNeedsUpdate = true;
            d->queueSignal("selectionDataChanged");
            return;
        }
    }
}

void ContextManager::slotSelectionChanged()
{
    d->mSelectedFileItemListNeedsUpdate = true;
    d->queueSignal("selectionChanged");
}

void Gwenview::ContextManager::slotCurrentChanged(const QModelIndex& index)
{
    KUrl url = d->mDirModel->urlForIndex(index);
    setCurrentUrl(url);
}

void ContextManager::emitQueuedSignals()
{
    Q_FOREACH(const QByteArray & signal, d->mQueuedSignals) {
        QMetaObject::invokeMethod(this, signal.data());
    }
    d->mQueuedSignals.clear();
}

} // namespace
