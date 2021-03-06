/*****************************************************************************
 * Copyright (C) 2002 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2002 Rafi Yanai <yanai@users.sourceforge.net>               *
 * Copyright (C) 2010 Jan Lepper <dehtris@yahoo.de>                          *
 *                                                                           *
 * This program is free software; you can redistribute it and/or modify      *
 * it under the terms of the GNU General Public License as published by      *
 * the Free Software Foundation; either version 2 of the License, or         *
 * (at your option) any later version.                                       *
 *                                                                           *
 * This package is distributed in the hope that it will be useful,           *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of            *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the             *
 * GNU General Public License for more details.                              *
 *                                                                           *
 * You should have received a copy of the GNU General Public License         *
 * along with this package; if not, write to the Free Software               *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA *
 *****************************************************************************/

#include "krinterview.h"

#include "krcolorcache.h"
#include "krmousehandler.h"
#include "krpreviews.h"
#include "krvfsmodel.h"
#include "krviewitem.h"
#include "../VFS/vfilecontainer.h"

KrInterView::KrInterView(KrViewInstance &instance, KConfig *cfg,
                         QAbstractItemView *itemView) :
        KrView(instance, cfg), _itemView(itemView), _mouseHandler(0)
{
    _model = new KrVfsModel(this);

    // fix the context menu problem
    int j = QFontMetrics(_itemView->font()).height() * 2;
    _mouseHandler = new KrMouseHandler(this, j);
}

KrInterView::~KrInterView()
{
    // any references to the model should be cleared ar this point,
    // but sometimes for some reason it is still referenced by
    // QPersistentModelIndex instances held by QAbstractItemView and/or QItemSelectionModel(child object) -
    // so schedule _model for later deletion
    _model->clear();
    _model->deleteLater();
    _model = 0;
    delete _mouseHandler;
    _mouseHandler = 0;
    QHashIterator< vfile *, KrViewItem *> it(_itemHash);
    while (it.hasNext())
        delete it.next().value();
    _itemHash.clear();
}

void KrInterView::selectRegion(KrViewItem *i1, KrViewItem *i2, bool select)
{
    vfile* vf1 = (vfile *)i1->getVfile();
    QModelIndex mi1 = _model->vfileIndex(vf1);
    vfile* vf2 = (vfile *)i2->getVfile();
    QModelIndex mi2 = _model->vfileIndex(vf2);

    if (mi1.isValid() && mi2.isValid()) {
        int r1 = mi1.row();
        int r2 = mi2.row();

        if (r1 > r2) {
            int t = r1;
            r1 = r2;
            r2 = t;
        }

        op()->setMassSelectionUpdate(true);
        for (int row = r1; row <= r2; row++)
            setSelected(_model->vfileAt(_model->index(row, 0)), select);
        op()->setMassSelectionUpdate(false);

        redraw();

    } else if (mi1.isValid() && !mi2.isValid())
        i1->setSelected(select);
    else if (mi2.isValid() && !mi1.isValid())
        i2->setSelected(select);
}

void KrInterView::intSetSelected(const vfile* vf, bool select)
{
    if(select)
        _selection.insert(vf);
    else
        _selection.remove(vf);
}

bool KrInterView::isSelected(const QModelIndex &ndx)
{
    return isSelected(_model->vfileAt(ndx));
}

KrViewItem* KrInterView::findItemByName(const QString &name)
{
    if (!_model->ready())
        return 0;

    QModelIndex ndx = _model->nameIndex(name);
    if (!ndx.isValid())
        return 0;
    return getKrViewItem(ndx);
}

QString KrInterView::getCurrentItem() const
{
    if (!_model->ready())
        return QString();

    vfile *vf = _model->vfileAt(_itemView->currentIndex());
    if (vf == 0)
        return QString();
    return vf->vfile_getName();
}

KrViewItem* KrInterView::getCurrentKrViewItem()
{
    if (!_model->ready())
        return 0;

    return getKrViewItem(_itemView->currentIndex());
}

KrViewItem* KrInterView::getFirst()
{
    if (!_model->ready())
        return 0;

    return getKrViewItem(_model->index(0, 0, QModelIndex()));
}

KrViewItem* KrInterView::getLast()
{
    if (!_model->ready())
        return 0;

    return getKrViewItem(_model->index(_model->rowCount() - 1, 0, QModelIndex()));
}

KrViewItem* KrInterView::getNext(KrViewItem *current)
{
    vfile* vf = (vfile *)current->getVfile();
    QModelIndex ndx = _model->vfileIndex(vf);
    if (ndx.row() >= _model->rowCount() - 1)
        return 0;
    return getKrViewItem(_model->index(ndx.row() + 1, 0, QModelIndex()));
}

KrViewItem* KrInterView::getPrev(KrViewItem *current)
{
    vfile* vf = (vfile *)current->getVfile();
    QModelIndex ndx = _model->vfileIndex(vf);
    if (ndx.row() <= 0)
        return 0;
    return getKrViewItem(_model->index(ndx.row() - 1, 0, QModelIndex()));
}

KrViewItem* KrInterView::getKrViewItemAt(const QPoint &vp)
{
    if (!_model->ready())
        return 0;

    return getKrViewItem(_itemView->indexAt(vp));
}

KrViewItem *KrInterView::findItemByVfile(vfile *vf) {
    return getKrViewItem(vf);
}

KrViewItem * KrInterView::getKrViewItem(vfile *vf)
{
    QHash<vfile *, KrViewItem*>::iterator it = _itemHash.find(vf);
    if (it == _itemHash.end()) {
        KrViewItem * newItem =  new KrViewItem(vf, this);
        _itemHash[ vf ] = newItem;
        return newItem;
    }
    return *it;
}

KrViewItem * KrInterView::getKrViewItem(const QModelIndex & ndx)
{
    if (!ndx.isValid())
        return 0;
    vfile * vf = _model->vfileAt(ndx);
    if (vf == 0)
        return 0;
    else
        return getKrViewItem(vf);
}

void KrInterView::makeCurrentVisible()
{
    _itemView->scrollTo(_itemView->currentIndex());
}

void KrInterView::makeItemVisible(const KrViewItem *item)
{
    if (item == 0)
        return;

    vfile* vf = (vfile *)item->getVfile();
    QModelIndex ndx = _model->vfileIndex(vf);
    if (ndx.isValid())
        _itemView->scrollTo(ndx);
}

bool KrInterView::isItemVisible(const KrViewItem *item)
{
    return _itemView->viewport()->rect().contains(item->itemRect());
}

void KrInterView::setCurrentItem(const QString& name)
{
    QModelIndex ndx = _model->nameIndex(name);
    if (ndx.isValid())
        _itemView->setCurrentIndex(ndx);
}

void KrInterView::setCurrentKrViewItem(KrViewItem *item)
{
    if (item == 0) {
        _itemView->setCurrentIndex(QModelIndex());
        return;
    }
    vfile* vf = (vfile *)item->getVfile();
    QModelIndex ndx = _model->vfileIndex(vf);
    if (ndx.isValid() && ndx.row() != _itemView->currentIndex().row()) {
        _mouseHandler->cancelTwoClickRename();
        _itemView->setCurrentIndex(ndx);
    }
}

void KrInterView::sort()
{
    _model->sort();
}

void KrInterView::clear()
{
    _selection.clear();
    _itemView->clearSelection();
    _itemView->setCurrentIndex(QModelIndex());
    _model->clear();
    QHashIterator< vfile *, KrViewItem *> it(_itemHash);
    while (it.hasNext())
        delete it.next().value();
    _itemHash.clear();

    KrView::clear();
}

void KrInterView::populate(const QList<vfile*> &vfiles, vfile *dummy)
{
    _model->populate(vfiles, dummy);
    _itemView->setCurrentIndex(_model->index(0, 0));
}

KrViewItem* KrInterView::preAddItem(vfile *vf)
{
    QModelIndex idx = _model->addItem(vf);
    if(_model->rowCount() == 1) // if this is the fist item to be added, make it current
        _itemView->setCurrentIndex(idx);
    return getKrViewItem(idx);
}

void KrInterView::preDelItem(KrViewItem *item)
{
    setSelected(item->getVfile(), false);
    QModelIndex ndx = _model->removeItem((vfile *)item->getVfile());
    if (ndx.isValid())
        _itemView->setCurrentIndex(ndx);
    _itemHash.remove((vfile *)item->getVfile());
}

void KrInterView::preUpdateItem(vfile *vf)
{
    _model->updateItem(vf);
}

void KrInterView::prepareForActive()
{
    KrView::prepareForActive();
    _itemView->setFocus();
    KrViewItem * current = getCurrentKrViewItem();
    if (current != 0) {
        QString desc = current->description();
        op()->emitItemDescription(desc);
    }
}

void KrInterView::prepareForPassive()
{
    KrView::prepareForPassive();
    _mouseHandler->cancelTwoClickRename();
    //if ( renameLineEdit() ->isVisible() )
    //renameLineEdit() ->clearFocus();
}

void KrInterView::redraw()
{
    _itemView->viewport()->update();
}

void KrInterView::refreshColors()
{
    QPalette p(_itemView->palette());
    KrColorGroup cg;
    KrColorCache::getColorCache().getColors(cg, KrColorItemType(KrColorItemType::File,
        false, _focused, false, false));
    p.setColor(QPalette::Text, cg.text());
    p.setColor(QPalette::Base, cg.background());
    _itemView->setPalette(p);
    redraw();
}

void KrInterView::showContextMenu(const QPoint &point)
{
    showContextMenu(_itemView->viewport()->mapToGlobal(point));
}

void KrInterView::sortModeUpdated(int column, Qt::SortOrder order)
{
    KrView::sortModeUpdated(static_cast<KrViewProperties::ColumnType>(column),
                            order == Qt::DescendingOrder);
}

KIO::filesize_t KrInterView::calcSize()
{
    KIO::filesize_t size = 0;
    foreach(vfile *vf, _model->vfiles()) {
        size += vf->vfile_getSize();
    }
    return size;
}

KIO::filesize_t KrInterView::calcSelectedSize()
{
    KIO::filesize_t size = 0;
    foreach(const vfile *vf, _selection) {
        size += vf->vfile_getSize();
    }
    return size;
}

QList<QUrl> KrInterView::selectedUrls()
{
    QList<QUrl> list;
    foreach(const vfile *vf, _selection) {
        list << vf->vfile_getUrl();
    }
    return list;
}

void KrInterView::setSelectionUrls(const QList<QUrl> urls)
{
    op()->setMassSelectionUpdate(true);

    _selection.clear();

    foreach(const QUrl &url, urls) {
        QModelIndex idx = _model->indexFromUrl(url);
        if(idx.isValid())
            setSelected(_model->vfileAt(idx), true);
    }

    op()->setMassSelectionUpdate(false);
}
