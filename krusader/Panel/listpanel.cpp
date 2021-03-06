/***************************************************************************
                         listpanel.cpp
                      -------------------
copyright            : (C) 2000 by Shie Erlich & Rafi Yanai
e-mail               : krusader@users.sourceforge.net
web site             : http://krusader.sourceforge.net
---------------------------------------------------------------------------
Description
***************************************************************************

A

db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                              S o u r c e    F i l e

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/

#include "listpanel.h"

// QtCore
#include <QStringList>
#include <QList>
#include <QEvent>
#include <QMimeData>
#include <QTimer>
#include <QRegExp>
#include <QUrl>
// QtGui
#include <QBitmap>
#include <QKeyEvent>
#include <QPixmap>
#include <QDropEvent>
#include <QHideEvent>
#include <QShowEvent>
#include <QDrag>
#include <QImage>
// QtWidgets
#include <QHBoxLayout>
#include <QFrame>
#include <QMenu>
#include <QSplitter>
#include <QTabBar>

#include <KCoreAddons/KUrlMimeData>
#include <KI18n/KLocalizedString>
#include <KIconThemes/KIconLoader>
#include <KIO/DropJob>
#include <KIOCore/KMountPoint>
#include <KIOCore/KDiskFreeSpaceInfo>
#include <KWidgetsAddons/KCursor>
#include <KWidgetsAddons/KMessageBox>
#include <KIOFileWidgets/KFilePlacesModel>
#include <KIOWidgets/KUrlComboBox>

#include "../defaults.h"
#include "../krusader.h"
#include "../krslots.h"
#include "../kicons.h"
#include "../krusaderview.h"
#include "../krservices.h"
#include "../VFS/krpermhandler.h"
#include "../VFS/krarchandler.h"
#include "../MountMan/kmountman.h"
#include "../BookMan/krbookmarkbutton.h"
#include "../Dialogs/krdialogs.h"
#include "../Dialogs/krspwidgets.h"
#include "../Dialogs/percentalsplitter.h"
#include "../Dialogs/popularurls.h"
#include "../GUI/kcmdline.h"
#include "../GUI/dirhistorybutton.h"
#include "../GUI/mediabutton.h"
#include "../GUI/syncbrowsebutton.h"
#include "../UserAction/useractionpopupmenu.h"

#include "listpanelactions.h"
#include "viewactions.h"
#include "krpreviewpopup.h"
#include "panelpopup.h"
#include "panelfunc.h"
#include "krpopupmenu.h"
#include "krviewfactory.h"
#include "krcolorcache.h"
#include "krerrordisplay.h"
#include "krlayoutfactory.h"
#include "krsearchbar.h"
#include "dirhistoryqueue.h"


class ActionButton : public QToolButton
{
public:
    ActionButton(QWidget *parent, ListPanel *panel, QAction *action, QString text = QString()) :
            QToolButton(parent),  panel(panel), action(action) {
        setText(text);
        setAutoRaise(true);
        if(KConfigGroup(krConfig, "ListPanelButtons").readEntry("Icons", false) || text.isEmpty())
            setIcon(action->icon());
        setToolTip(action->toolTip());
    }

protected:
    virtual void mousePressEvent(QMouseEvent *) Q_DECL_OVERRIDE {
        panel->slotFocusOnMe();
        action->trigger();
    }

    ListPanel *panel;
    QAction *action;
};


/////////////////////////////////////////////////////
//      The list panel constructor       //
/////////////////////////////////////////////////////
ListPanel::ListPanel(QWidget *parent, AbstractPanelManager *manager, KConfigGroup cfg) :
        QWidget(parent), KrPanel(manager),
        panelType(-1), colorMask(255), compareMode(false),
        previewJob(0), inlineRefreshJob(0), searchBar(0), cdRootButton(0), cdUpButton(0),
        popupBtn(0), popup(0), vfsError(0), _locked(false)
{
    if(cfg.isValid())
        panelType = cfg.readEntry("Type", -1);
    if (panelType == -1)
        panelType = defaultPanelType();
    gui = this;
    func = new ListPanelFunc(this);
    _actions = krApp->listPanelActions();

    setAcceptDrops(true);

    QHash<QString, QWidget*> widgets;

#define ADD_WIDGET(widget) widgets.insert(#widget, widget);

    // media button
    mediaButton = new MediaButton(this);
    connect(mediaButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(mediaButton, SIGNAL(openUrl(const QUrl&)), func, SLOT(openUrl(const QUrl&)));
    connect(mediaButton, SIGNAL(newTab(const QUrl&)), SLOT(newTab(const QUrl&)));
    ADD_WIDGET(mediaButton);

    // status bar
    status = new KrSqueezedTextLabel(this);
    KConfigGroup group(krConfig, "Look&Feel");
    status->setFont(group.readEntry("Filelist Font", _FilelistFont));
    status->setAutoFillBackground(false);
    status->setText("");          // needed for initialization code!
    status->setWhatsThis(i18n("The statusbar displays information about the filesystem "
                              "which holds your current folder: total size, free space, "
                              "type of filesystem, etc."));
    ADD_WIDGET(status);

    // back button
    backButton = new ActionButton(this, this, _actions->actHistoryBackward);
    ADD_WIDGET(backButton);

    // forward button
    forwardButton = new ActionButton(this, this, _actions->actHistoryForward);
    ADD_WIDGET(forwardButton);


    // ... create the history button
    historyButton = new DirHistoryButton(func->history, this);
    connect(historyButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(historyButton, SIGNAL(gotoPos(int)), func, SLOT(historyGotoPos(int)));
    ADD_WIDGET(historyButton);

    // bookmarks button
    bookmarksButton = new KrBookmarkButton(this);
    connect(bookmarksButton, SIGNAL(aboutToShow()), this, SLOT(slotFocusOnMe()));
    connect(bookmarksButton, SIGNAL(openUrl(const QUrl&)), func, SLOT(openUrl(const QUrl&)));
    bookmarksButton->setWhatsThis(i18n("Open menu with bookmarks. You can also add "
                                       "current location to the list, edit bookmarks "
                                       "or add subfolder to the list."));
    ADD_WIDGET(bookmarksButton);

    // url input field
    urlNavigator = new KUrlNavigator(new KFilePlacesModel(this), QUrl(), this);
    urlNavigator->setWhatsThis(i18n("Name of folder where you are. You can also "
                                    "enter name of desired location to move there. "
                                    "Use of Net protocols like ftp or fish is possible."));
    // handle certain key events here in event filter
    urlNavigator->editor()->installEventFilter(this);
    urlNavigator->setUrlEditable(isNavigatorEditModeSet());
    urlNavigator->setShowFullPath(group.readEntry("Navigator Full Path", false));
    connect(urlNavigator, SIGNAL(returnPressed()), this, SLOT(slotFocusOnMe()));
    connect(urlNavigator, SIGNAL(urlChanged(QUrl)), func, SLOT(navigatorUrlChanged(QUrl)));
    connect(urlNavigator->editor()->lineEdit(), SIGNAL(editingFinished()), this, SLOT(resetNavigatorMode()));
    connect(urlNavigator, SIGNAL(tabRequested(QUrl)), this, SLOT(newTab(QUrl)));
    connect(urlNavigator, SIGNAL(urlsDropped(QUrl, QDropEvent*)), this, SLOT(handleDrop(QUrl, QDropEvent*)));
    ADD_WIDGET(urlNavigator);

    // toolbar
    QWidget * toolbar = new QWidget(this);
    QHBoxLayout * toolbarLayout = new QHBoxLayout(toolbar);
    toolbarLayout->setContentsMargins(0, 0, 0, 0);
    toolbarLayout->setSpacing(0);
    ADD_WIDGET(toolbar);

    vfsError = new KrErrorDisplay(this);
    vfsError->setWordWrap(true);
    vfsError->hide();
    ADD_WIDGET(vfsError);

    // client area
    clientArea = new QWidget(this);
    QVBoxLayout *clientLayout = new QVBoxLayout(clientArea);
    clientLayout->setSpacing(0);
    clientLayout->setContentsMargins(0, 0, 0, 0);
    ADD_WIDGET(clientArea);

    // totals label
    totals = new KrSqueezedTextLabel(this);
    totals->setFont(group.readEntry("Filelist Font", _FilelistFont));
    totals->setAutoFillBackground(false);
    totals->setWhatsThis(i18n("The totals bar shows how many files exist, "
                              "how many selected and the bytes math"));
    ADD_WIDGET(totals);

    // free space label
    freeSpace = new KrSqueezedTextLabel(this);
    freeSpace->setFont(group.readEntry("Filelist Font", _FilelistFont));
    freeSpace->setAutoFillBackground(false);
    freeSpace->setText("");
    freeSpace->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    ADD_WIDGET(freeSpace);

    // progress indicator for the preview job
    previewProgress = new QProgressBar(this);
    previewProgress->hide();
    ADD_WIDGET(previewProgress);

    // a cancel button for the inplace refresh mechanism
    inlineRefreshCancelButton = new QToolButton(this);
    inlineRefreshCancelButton->hide();
    inlineRefreshCancelButton->setIcon(krLoader->loadIcon("dialog-cancel", KIconLoader::Toolbar, 16));
    connect(inlineRefreshCancelButton, SIGNAL(clicked()), this, SLOT(inlineRefreshCancel()));
    ADD_WIDGET(inlineRefreshCancelButton);

    // button for changing the panel popup position in the panel
    popupPositionBtn = new QToolButton(this);
    popupPositionBtn->hide();
    popupPositionBtn->setAutoRaise(true);
    popupPositionBtn->setIcon(krLoader->loadIcon("exchange-positions", KIconLoader::Toolbar, 16));
    popupPositionBtn->setToolTip(i18n("Move popup panel clockwise"));
    connect(popupPositionBtn, &QToolButton::clicked, [this]() {
        // moving position clockwise
        setPopupPosition((popupPosition() + 1) % 4); });
    ADD_WIDGET(popupPositionBtn);

    // a quick button to open the popup panel
    popupBtn = new QToolButton(this);
    popupBtn->setAutoRaise(true);
    popupBtn->setIcon(krLoader->loadIcon("arrow-up", KIconLoader::Toolbar, 16));
    connect(popupBtn, SIGNAL(clicked()), this, SLOT(togglePanelPopup()));
    popupBtn->setToolTip(i18n("Open the popup panel"));
    ADD_WIDGET(popupBtn);

#undef ADD_WIDGET

    // toolbar buttons
    cdOtherButton = new ActionButton(toolbar, this, _actions->actCdToOther, "=");
    toolbarLayout->addWidget(cdOtherButton);

    cdUpButton = new ActionButton(toolbar, this, _actions->actDirUp, "..");
    toolbarLayout->addWidget(cdUpButton);

    cdHomeButton = new ActionButton(toolbar, this, _actions->actHome, "~");
    toolbarLayout->addWidget(cdHomeButton);

    cdRootButton = new ActionButton(toolbar, this, _actions->actRoot, "/");
    toolbarLayout->addWidget(cdRootButton);

    // ... creates the button for sync-browsing
    syncBrowseButton = new SyncBrowseButton(toolbar);
    syncBrowseButton->setAutoRaise(true);
    toolbarLayout->addWidget(syncBrowseButton);

    setButtons();

    // create a splitter to hold the view and the popup
    splt = new PercentalSplitter(clientArea);
    splt->setChildrenCollapsible(true);
    splt->setOrientation(Qt::Vertical);
    // expand vertical if splitter orientation is horizontal
    QSizePolicy sizePolicy = splt->sizePolicy();
    sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);
    splt->setSizePolicy(sizePolicy);
    clientLayout->addWidget(splt);

    // view
    createView();

    // search (in folder) bar
    searchBar = new KrSearchBar(view, clientArea);
    searchBar->hide();
    bool top = group.readEntry("Quicksearch Position", "bottom") == "top";
    clientLayout->insertWidget(top ? 0 : -1, searchBar);

    // create the layout
    KrLayoutFactory fact(this, widgets);
    QLayout *layout = fact.createLayout();

    if(!layout) { // fallback: create a layout by ourself
        QVBoxLayout *v = new QVBoxLayout;
        v->setContentsMargins(0, 0, 0, 0);
        v->setSpacing(0);

        QHBoxLayout *h = new QHBoxLayout;
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        h->addWidget(urlNavigator);
        h->addWidget(toolbar);
        h->addStretch();
        v->addLayout(h);

        h = new QHBoxLayout;
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        h->addWidget(mediaButton);
        h->addWidget(status);
        h->addWidget(backButton);
        h->addWidget(forwardButton);
        h->addWidget(historyButton);
        h->addWidget(bookmarksButton);
        v->addLayout(h);

        v->addWidget(vfsError);
        v->addWidget(clientArea);

        h = new QHBoxLayout;
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(0);
        h->addWidget(totals);
        h->addWidget(freeSpace);
        h->addWidget(previewProgress);
        h->addWidget(inlineRefreshCancelButton);
        h->addWidget(popupBtn);
        v->addLayout(h);

        layout = v;
    }

    setLayout(layout);

    connect(&KrColorCache::getColorCache(), SIGNAL(colorsRefreshed()), this, SLOT(refreshColors()));
    connect(krApp, SIGNAL(shutdown()), SLOT(inlineRefreshCancel()));
}

ListPanel::~ListPanel()
{
    inlineRefreshCancel();
    delete view;
    view = 0;
    delete func;
    delete status;
    delete bookmarksButton;
    delete totals;
    delete urlNavigator;
    delete cdRootButton;
    delete cdHomeButton;
    delete cdUpButton;
    delete cdOtherButton;
    delete syncBrowseButton;
//     delete layout;
}

void ListPanel::reparent(QWidget *parent, AbstractPanelManager *manager)
{
    setParent(parent);
    _manager = manager;
}

int ListPanel::defaultPanelType()
{
    KConfigGroup group(krConfig, "Look&Feel");
    return group.readEntry("Default Panel Type", KrViewFactory::defaultViewId());
}

bool ListPanel::isNavigatorEditModeSet()
{
    KConfigGroup group(krConfig, "Look&Feel");
    return group.readEntry("Navigator Edit Mode", false);
}

void ListPanel::createView()
{
    view = KrViewFactory::createView(panelType, splt, krConfig);
    view->init();
    view->setMainWindow(krApp);

    // KrViewFactory may create a different view type than requested
    panelType = view->instance()->id();

    if(this == ACTIVE_PANEL)
        view->prepareForActive();
    else
        view->prepareForPassive();
    view->refreshColors();

    splt->insertWidget(popupPosition() < 2 ? 1 : 0, view->widget());

    view->widget()->installEventFilter(this);

    connect(view->op(), SIGNAL(calcSpace(KrViewItem*)), func, SLOT(calcSpace(KrViewItem*)));
    connect(view->op(), SIGNAL(goHome()), func, SLOT(home()));
    connect(view->op(), SIGNAL(dirUp()), func, SLOT(dirUp()));
    connect(view->op(), SIGNAL(deleteFiles(bool)), func, SLOT(deleteFiles(bool)));
    connect(view->op(), SIGNAL(middleButtonClicked(KrViewItem *)), SLOT(newTab(KrViewItem *)));
    connect(view->op(), SIGNAL(currentChanged(KrViewItem *)), SLOT(updatePopupPanel(KrViewItem*)));
    connect(view->op(), SIGNAL(renameItem(const QString &, const QString &)),
            func, SLOT(rename(const QString &, const QString &)));
    connect(view->op(), SIGNAL(executed(const QString&)), func, SLOT(execute(const QString&)));
    connect(view->op(), SIGNAL(goInside(const QString&)), func, SLOT(goInside(const QString&)));
    connect(view->op(), SIGNAL(needFocus()), this, SLOT(slotFocusOnMe()));
    connect(view->op(), SIGNAL(selectionChanged()), this, SLOT(slotUpdateTotals()));
    connect(view->op(), SIGNAL(itemDescription(QString&)), krApp, SLOT(statusBarUpdate(QString&)));
    connect(view->op(), SIGNAL(contextMenu(const QPoint &)), this, SLOT(popRightClickMenu(const QPoint &)));
    connect(view->op(), SIGNAL(emptyContextMenu(const QPoint &)),
            this, SLOT(popEmptyRightClickMenu(const QPoint &)));
    connect(view->op(), SIGNAL(letsDrag(QStringList, QPixmap)), this, SLOT(startDragging(QStringList, QPixmap)));
    connect(view->op(), &KrViewOperator::gotDrop,
            this, [this](QDropEvent *event) {handleDrop(event, true); });
    connect(view->op(), SIGNAL(previewJobStarted(KJob*)), this, SLOT(slotPreviewJobStarted(KJob*)));
    connect(view->op(), SIGNAL(refreshActions()), krApp->viewActions(), SLOT(refreshActions()));
    connect(view->op(), SIGNAL(currentChanged(KrViewItem*)), func->history, SLOT(saveCurrentItem()));
    connect(view->op(), &KrViewOperator::goBack, func, &ListPanelFunc::historyBackward);
    connect(view->op(), &KrViewOperator::goForward, func, &ListPanelFunc::historyForward);

    view->setFiles(func->files());

    func->refreshActions();
}

void ListPanel::changeType(int type)
{
    if (panelType != type) {
        QString current = view->getCurrentItem();
        QList<QUrl> selection = view->selectedUrls();
        bool filterApplysToDirs = view->properties()->filterApplysToDirs;
        KrViewProperties::FilterSpec filter = view->filter();
        FilterSettings filterSettings = view->properties()->filterSettings;

        panelType = type;

        KrView *oldView = view;
        createView();
        searchBar->setView(view);
        delete oldView;

        view->setFilter(filter, filterSettings, filterApplysToDirs);
        view->setSelectionUrls(selection);
        view->setCurrentItem(current);
        view->makeItemVisible(view->getCurrentKrViewItem());
    }
}

int ListPanel::getProperties()
{
    int props = 0;
    if (syncBrowseButton->state() == SYNCBROWSE_CD)
        props |= PROP_SYNC_BUTTON_ON;
    if (_locked)
        props |= PROP_LOCKED;
    return props;
}

void ListPanel::setProperties(int prop)
{
    syncBrowseButton->setChecked(prop & PROP_SYNC_BUTTON_ON);
    _locked = (prop & PROP_LOCKED);
}

bool ListPanel::eventFilter(QObject * watched, QEvent * e)
{
    if(view && watched == view->widget()) {
        if(e->type() == QEvent::FocusIn && this != ACTIVE_PANEL && !isHidden())
            slotFocusOnMe();
        else if(e->type() == QEvent::ShortcutOverride) {
            QKeyEvent *ke = static_cast<QKeyEvent*>(e);
            if(ke->key() == Qt::Key_Escape && ke->modifiers() == Qt::NoModifier) {
                // if the cancel refresh action has no shortcut assigned,
                // we need this event ourselves to cancel refresh
                if(_actions->actCancelRefresh->shortcut().isEmpty()) {
                    e->accept();
                    return true;
                }
            }
        }
    }
    // handle URL navigator key events
    else if(watched == urlNavigator->editor()) {
        // override default shortcut for panel focus
        if(e->type() == QEvent::ShortcutOverride) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if ((ke->key() == Qt::Key_Escape) && (ke->modifiers() == Qt::NoModifier)) {
                e->accept(); // we will get the key press event now
                return true;
            }
        } else if(e->type() == QEvent::KeyPress) {
            QKeyEvent *ke = static_cast<QKeyEvent *>(e);
            if ((ke->key() == Qt::Key_Down) && (ke->modifiers() == Qt::ControlModifier)) {
                slotFocusOnMe();
                return true;
            } else if ((ke->key() == Qt::Key_Escape) && (ke->modifiers() == Qt::NoModifier)) {
                // reset navigator
                urlNavigator->editor()->setUrl(urlNavigator->locationUrl());
                slotFocusOnMe();
                return true;
            }
        }
    }
    return false;
}

void ListPanel::togglePanelPopup()
{
    if(!popup) {
        popup = new PanelPopup(splt, isLeft(), krApp);
        // fix vertical grow of splitter (and entire window) if its content
        // demands more space
        QSizePolicy sizePolicy = popup->sizePolicy();
        sizePolicy.setVerticalPolicy(QSizePolicy::Ignored);
        popup->setSizePolicy(sizePolicy);
        connect(this, SIGNAL(pathChanged(const QUrl&)), popup, SLOT(onPanelPathChange(const QUrl&)));
        connect(popup, SIGNAL(selection(const QUrl&)), SLOTS, SLOT(refresh(const QUrl&)));
        connect(popup, SIGNAL(hideMe()), this, SLOT(togglePanelPopup()));
    }

    if (popup->isHidden()) {
        if (popupSizes.count() > 0) {
            splt->setSizes(popupSizes);
        } else { // on the first time, resize to 50%
            QList<int> lst;
            lst << height() / 2 << height() / 2;
            splt->setSizes(lst);
        }

        popup->show();
        popupBtn->setIcon(krLoader->loadIcon("arrow-down", KIconLoader::Toolbar, 16));
        popupBtn->setToolTip(i18n("Close the popup panel"));
        popupPositionBtn->show();
    } else {
        popupSizes.clear();
        popupSizes = splt->sizes();
        popup->hide();
        popupBtn->setIcon(krLoader->loadIcon("arrow-up", KIconLoader::Toolbar, 16));
        popupBtn->setToolTip(i18n("Open the popup panel"));
        popupPositionBtn->hide();

        QList<int> lst;
        lst << height() << 0;
        splt->setSizes(lst);
        if (ACTIVE_PANEL)
            ACTIVE_PANEL->gui->slotFocusOnMe();
    }
}

QString ListPanel::realPath() const
{
    return _realPath.path();
}

void ListPanel::setButtons()
{
    KConfigGroup group(krConfig, "Look&Feel");

    mediaButton->setVisible(group.readEntry("Media Button Visible", true));
    backButton->setVisible(group.readEntry("Back Button Visible", false));
    forwardButton->setVisible(group.readEntry("Forward Button Visible", false));
    historyButton->setVisible(group.readEntry("History Button Visible", true));
    bookmarksButton->setVisible(group.readEntry("Bookmarks Button Visible", true));

    if (group.readEntry("Panel Toolbar visible", _PanelToolBar)) {
        cdRootButton->setVisible(group.readEntry("Root Button Visible", _cdRoot));
        cdHomeButton->setVisible(group.readEntry("Home Button Visible", _cdHome));
        cdUpButton->setVisible(group.readEntry("Up Button Visible", _cdUp));
        cdOtherButton->setVisible(group.readEntry("Equal Button Visible", _cdOther));
        syncBrowseButton->setVisible(group.readEntry("SyncBrowse Button Visible", _syncBrowseButton));
    } else {
        cdRootButton->hide();
        cdHomeButton->hide();
        cdUpButton->hide();
        cdOtherButton->hide();
        syncBrowseButton->hide();
    }
}

void ListPanel::slotUpdateTotals()
{
    totals->setText(view->statistics());
}

void ListPanel::compareDirs(bool otherPanelToo)
{
    // Performs a check in order to avoid that the next code is executed twice
    if (otherPanelToo == true) {
        // If both panels are showing the same directory
        if (_manager->currentPanel()->virtualPath() == otherPanel()->virtualPath()) {
            if (KMessageBox::warningContinueCancel(this,
                i18n("Warning: The left and the right side are showing the same folder.")) != KMessageBox::Continue) {
                return;
            }
        }
    }

    KConfigGroup pg(krConfig, "Private");
    int compareMode = pg.readEntry("Compare Mode", 0);
    KConfigGroup group(krConfig, "Look&Feel");
    bool selectDirs = group.readEntry("Mark Dirs", false);

    KrViewItem *item, *otherItem;

    for (item = view->getFirst(); item != 0; item = view->getNext(item)) {
        if (item->name() == "..")
            continue;

        for (otherItem = otherPanel()->view->getFirst(); otherItem != 0 && otherItem->name() != item->name() ;
                otherItem = otherPanel()->view->getNext(otherItem));

        bool isSingle = (otherItem == 0), isDifferent = false, isNewer = false;

        if (func->getVFile(item)->vfile_isDir() && !selectDirs) {
            item->setSelected(false);
            continue;
        }

        if (otherItem) {
            if (!func->getVFile(item)->vfile_isDir())
                isDifferent = ITEM2VFILE(otherPanel(), otherItem)->vfile_getSize() != func->getVFile(item)->vfile_getSize();
            isNewer = func->getVFile(item)->vfile_getTime_t() > ITEM2VFILE(otherPanel(), otherItem)->vfile_getTime_t();
        }

        switch (compareMode) {
        case 0:
            item->setSelected(isNewer || isSingle);
            break;
        case 1:
            item->setSelected(isNewer);
            break;
        case 2:
            item->setSelected(isSingle);
            break;
        case 3:
            item->setSelected(isDifferent || isSingle);
            break;
        case 4:
            item->setSelected(isDifferent);
            break;
        }
    }

    view->updateView();

    if (otherPanelToo)
        otherPanel()->gui->compareDirs(false);
}

void ListPanel::refreshColors()
{
    view->refreshColors();
    emit refreshColors(this == ACTIVE_PANEL);
}

void ListPanel::slotFocusOnMe(bool focus)
{
    if (focus && _manager->currentPanel() != this) {
        // ignore focus request if this panel is not shown
        return;
    }

    krApp->setUpdatesEnabled(false);

    if(focus) {
        emit activate();
        _actions->activePanelChanged();
        func->refreshActions();
        updatePopupPanel(view->getCurrentKrViewItem());
        view->prepareForActive();
        otherPanel()->gui->slotFocusOnMe(false);
    } else {
        // in case a new url was entered but not refreshed to,
        // reset url navigator to the current url
        urlNavigator->setLocationUrl(virtualPath());
        view->prepareForPassive();
    }

    urlNavigator->setActive(focus);
    refreshColors();
    emit refreshPathLabel();

    krApp->setUpdatesEnabled(true);
}

// this is used to start the panel
//////////////////////////////////////////////////////////////////
void ListPanel::start(QUrl url, bool immediate)
{
    QUrl virt(url);

    if (!virt.isValid())
        virt = QUrl::fromLocalFile(ROOT_DIR);
    if (virt.isLocalFile())
        _realPath = virt;
    else
        _realPath = QUrl::fromLocalFile(ROOT_DIR);

    if (immediate)
        func->immediateOpenUrl(virt, true);
    else
        func->openUrl(virt);

    setJumpBack(virt);
}

void ListPanel::slotStartUpdate()
{
    if (inlineRefreshJob)
        inlineRefreshListResult(0);

    if (this == ACTIVE_PANEL) {
        slotFocusOnMe();
    }

    setCursor(Qt::BusyCursor);

    if (func->files()->vfs_getType() == vfs::VFS_NORMAL)
        _realPath = virtualPath();
    urlNavigator->setLocationUrl(virtualPath());
    emit pathChanged(this);
    emit pathChanged(virtualPath());

    slotGetStats(virtualPath());
    if (compareMode)
        otherPanel()->view->refresh();

    // return cursor to normal arrow
    setCursor(Qt::ArrowCursor);
    slotUpdateTotals();
    krApp->popularUrls()->addUrl(virtualPath());

    searchBar->hideBar();
}

void ListPanel::slotGetStats(const QUrl &url)
{
    mediaButton->mountPointChanged(QString());
    freeSpace->setText(QString());

    if (!KConfigGroup(krConfig, "Look&Feel").readEntry("ShowSpaceInformation", true)) {
        if(func->files()->metaInformation().isEmpty())
            status->setText(i18n("Space information disabled"));
        else
            status->setText(func->files()->metaInformation());
        return ;
    }

    if (!url.isLocalFile()) {
        if(func->files()->metaInformation().isEmpty())
            status->setText(i18n("No space information on non-local filesystems"));
        else
            status->setText(func->files()->metaInformation());
        return ;
    }

    // check for special filesystems;
    QString path = url.path(); // must be local url
    if (path.left(4) == "/dev") {
        status->setText(i18n("No space information on [dev]"));
        return;
    }
#ifdef BSD
    if (path.left(5) == "/procfs") {     // /procfs is a special case - no volume information
        status->setText(i18n("No space information on [procfs]"));
        return;
    }
#else
    if (path.left(5) == "/proc") {     // /proc is a special case - no volume information
        status->setText(i18n("No space information on [proc]"));
        return;
    }
#endif

    KDiskFreeSpaceInfo info = KDiskFreeSpaceInfo::freeSpaceInfo(path);
    if(!info.isValid()) {
        status->setText(i18n("Space information unavailable"));
        return;
    }
    int perc = 0;
    if (info.size() != 0) { // make sure that if totalsize==0, then perc=0
        perc = (int)(((float)info.available() / (float)info.size()) * 100.0);
    }
    // mount point information - find it in the list first
    KMountPoint::List lst = KMountPoint::currentMountPoints();
    QString fstype = i18nc("Unknown file system type", "unknown");
    for (KMountPoint::List::iterator it = lst.begin(); it != lst.end(); ++it) {
        if ((*it)->mountPoint() == info.mountPoint()) {
            fstype = (*it)->mountType();
            break;
        }
    }

    QString stats = i18nc("%1=free space,%2=total space,%3=percentage of usage, "
                          "%4=mountpoint,%5=filesystem type",
                          "%1 free out of %2 (%3%) on %4 [(%5)]",
                          KIO::convertSize(info.available()),
                          KIO::convertSize(info.size()), perc,
                          info.mountPoint(), fstype);

    status->setText(stats);

    freeSpace->setText("    " + i18n("%1 free", KIO::convertSize(info.available())));
    mediaButton->mountPointChanged(info.mountPoint());
}

void ListPanel::handleDrop(QDropEvent *event, bool onView)
{
    // check what was dropped
    const QList<QUrl> urls = KUrlMimeData::urlsFromMimeData(event->mimeData());
    if (urls.isEmpty()) {
        event->ignore(); // not for us to handle!
        return;
    }

    // find dropping destination
    QString destinationDir = "";
    const bool dragFromThisPanel = event->source() == this;
    const KrViewItem *item = onView ? view->getKrViewItemAt(event->pos()) : 0;
    if (item) {
        const vfile *file = item->getVfile();
        if (file && !file->vfile_isDir() && dragFromThisPanel) {
            event->ignore(); // dragging on files in same panel, ignore
            return ;
        } else if (!file || file->vfile_isDir()) { // item is ".." dummy or a directory
            destinationDir = item->name();
        }
    } else if (dragFromThisPanel) {
        event->ignore(); // dragged from this panel onto an empty spot in this panel, ignore
        return ;
    }

    QUrl destination = QUrl(virtualPath());
    destination.setPath(destination.path() + '/' + destinationDir);

    func->files()->vfs_drop(destination, event);

    if(KConfigGroup(krConfig, "Look&Feel").readEntry("UnselectBeforeOperation",
                                                     _UnselectBeforeOperation)) {
        KrPanel *p = dragFromThisPanel ? this : otherPanel();
        p->view->saveSelection();
        p->view->unselectAll();
    }
}

void ListPanel::handleDrop(const QUrl &destination, QDropEvent *event)
{
    func->files()->vfs_drop(destination, event);
}

void ListPanel::slotJobResult(KJob *job)
{
    func->refresh(); // see new files if not on local filesystem
    if (job->error()) {
        slotVfsError(job->errorString());
    }
}

void ListPanel::vfs_refresh(KJob* /*job*/)
{
    if (func)
        func->refresh();
}

void ListPanel::startDragging(QStringList names, QPixmap px)
{
    if (names.isEmpty()) {  // avoid dragging empty urls
        return;
    }

    QList<QUrl> urls = func->files()->vfs_getFiles(names);

    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData;
    drag->setPixmap(px);
    mimeData->setUrls(urls);
    drag->setMimeData(mimeData);

    drag->start();
}

// pops a right-click menu for items
void ListPanel::popRightClickMenu(const QPoint &loc)
{
    // run it, on the mouse location
    int j = QFontMetrics(font()).height() * 2;
    KrPopupMenu::run(QPoint(loc.x() + 5, loc.y() + j), this);
}

void ListPanel::popEmptyRightClickMenu(const QPoint &loc)
{
    KrPopupMenu::run(loc, this);
}

QString ListPanel::getCurrentName()
{
    QString name = view->getCurrentItem();
    if (name != "..")
        return name;
    else
        return QString();
}

void ListPanel::prepareToDelete()
{
    view->setNameToMakeCurrent(view->firstUnmarkedBelowCurrent());
}

void ListPanel::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Enter :
    case Qt::Key_Return :
        if (e->modifiers() & Qt::ControlModifier) {
            if (e->modifiers() & Qt::AltModifier) {
                vfile *vf = func->files()->vfs_search(view->getCurrentKrViewItem()->name());
                if (vf && vf->vfile_isDir())
                    newTab(vf->vfile_getUrl(), true);
            } else {
                SLOTS->insertFileName((e->modifiers() & Qt::ShiftModifier) != 0);
            }
        } else e->ignore();
        break;
    case Qt::Key_Right :
    case Qt::Key_Left :
        if (e->modifiers() == Qt::ControlModifier) {
            // user pressed CTRL+Right/Left - refresh other panel to the selected path if it's a
            // directory otherwise as this one
            if ((isLeft() && e->key() == Qt::Key_Right) || (!isLeft() && e->key() == Qt::Key_Left)) {
                QUrl newPath;
                KrViewItem *it = view->getCurrentKrViewItem();

                if (it->name() == "..") {
                    newPath = KIO::upUrl(func->files()->vfs_getOrigin());
                } else {
                    vfile *v = func->getVFile(it);
                    // If it's a directory different from ".."
                    if (v && v->vfile_isDir() && v->vfile_getName() != "..") {
                        newPath = v->vfile_getUrl();
                    } else {
                        // If it's a supported compressed file
                        if (v && KRarcHandler::arcSupported(v->vfile_getMime()))   {
                            newPath = func->browsableArchivePath(v->vfile_getUrl().fileName());
                        } else {
                            newPath = func->files()->vfs_getOrigin();
                        }
                    }
                }
                otherPanel()->func->openUrl(newPath);
            } else func->openUrl(otherPanel()->func->files() ->vfs_getOrigin());
            return ;
        } else
            e->ignore();
        break;
    case Qt::Key_Down :
        if (e->modifiers() == Qt::ControlModifier) {   // give the keyboard focus to the command line
            if (MAIN_VIEW->cmdLine()->isVisible())
                MAIN_VIEW->cmdLineFocus();
            else
                MAIN_VIEW->focusTerminalEmulator();
            return ;
        } else if (e->modifiers() == (Qt::ControlModifier | Qt::ShiftModifier)) {     // give the keyboard focus to TE
            MAIN_VIEW->focusTerminalEmulator();
        } else
            e->ignore();
        break;

    case Qt::Key_Up :
        if (e->modifiers() == Qt::ControlModifier) {   // give the keyboard focus to the url navigator
            editLocation();
            return ;
        } else
            e->ignore();
        break;

    case Qt::Key_Escape:
        inlineRefreshCancel();
        break;

    default:
        // if we got this, it means that the view is not doing
        // the quick search thing, so send the characters to the commandline, if normal key
        if (e->modifiers() == Qt::NoModifier)
            MAIN_VIEW->cmdLine()->addText(e->text());

        //e->ignore();
    }
}

void ListPanel::showEvent(QShowEvent *e)
{
    panelActive();
    QWidget::showEvent(e);
}

void ListPanel::hideEvent(QHideEvent *e)
{
    panelInactive();
    QWidget::hideEvent(e);
}

void ListPanel::panelActive()
{
    // don't refresh when not active (ie: hidden, application isn't focused ...)
//     if (!
         func->files()->vfs_enableRefresh(true)
//        )
//         func->popErronousUrl()
                ;
}

void ListPanel::panelInactive()
{
    func->files()->vfs_enableRefresh(false);
}

void ListPanel::slotPreviewJobStarted(KJob *job)
{
    previewJob = job;
    previewProgress->setValue(0);
    previewProgress->setFormat(i18n("loading previews: %p%"));
    previewProgress->show();
    inlineRefreshCancelButton->show();
    previewProgress->setMaximumHeight(inlineRefreshCancelButton->height());
    connect(job, SIGNAL(percent(KJob*, unsigned long)), SLOT(slotPreviewJobPercent(KJob*, unsigned long)));
    connect(job, SIGNAL(result(KJob*)), SLOT(slotPreviewJobResult(KJob*)));
}

void ListPanel::slotPreviewJobPercent(KJob* /*job*/, unsigned long percent)
{
    previewProgress->setValue(percent);
}

void ListPanel::slotPreviewJobResult(KJob* /*job*/)
{
    previewJob = 0;
    previewProgress->hide();
    if(!inlineRefreshJob)
        inlineRefreshCancelButton->hide();
}

void ListPanel::slotJobStarted(KIO::Job* job)
{
    // disable the parts of the panel we don't want touched
    status->setEnabled(false);
    urlNavigator->setEnabled(false);
    cdRootButton->setEnabled(false);
    cdHomeButton->setEnabled(false);
    cdUpButton->setEnabled(false);
    cdOtherButton->setEnabled(false);
    popupBtn->setEnabled(false);
    if(popup)
        popup->setEnabled(false);
    bookmarksButton->setEnabled(false);
    historyButton->setEnabled(false);
    syncBrowseButton->setEnabled(false);

    // connect to the job interface to provide in-panel refresh notification
    connect(job, SIGNAL(infoMessage(KJob*, const QString &)),
            SLOT(inlineRefreshInfoMessage(KJob*, const QString &)));
    connect(job, SIGNAL(percent(KJob*, unsigned long)),
            SLOT(inlineRefreshPercent(KJob*, unsigned long)));
    connect(job, SIGNAL(result(KJob*)),
            this, SLOT(inlineRefreshListResult(KJob*)));

    inlineRefreshJob = job;

    totals->setText(i18n(">> Reading..."));
    inlineRefreshCancelButton->show();
}

void ListPanel::inlineRefreshCancel()
{
    if (inlineRefreshJob) {
        disconnect(inlineRefreshJob, 0, this, 0);
        inlineRefreshJob->kill(KJob::EmitResult);
        inlineRefreshListResult(0);
    }
    if(previewJob) {
        disconnect(previewJob, 0, this, 0);
        previewJob->kill(KJob::EmitResult);
        slotPreviewJobResult(0);
    }
}

void ListPanel::inlineRefreshPercent(KJob*, unsigned long perc)
{
    QString msg = i18n(">> Reading: %1 % complete...", perc);
    totals->setText(msg);
}

void ListPanel::inlineRefreshInfoMessage(KJob*, const QString &msg)
{
    totals->setText(i18n(">> Reading: %1", msg));
}

void ListPanel::inlineRefreshListResult(KJob*)
{
    if(inlineRefreshJob)
        disconnect(inlineRefreshJob, 0, this, 0);
    inlineRefreshJob = 0;
    // reenable everything
    status->setEnabled(true);
    urlNavigator->setEnabled(true);
    cdRootButton->setEnabled(true);
    cdHomeButton->setEnabled(true);
    cdUpButton->setEnabled(true);
    cdOtherButton->setEnabled(true);
    popupBtn->setEnabled(true);
    if(popup)
        popup->setEnabled(true);
    bookmarksButton->setEnabled(true);
    historyButton->setEnabled(true);
    syncBrowseButton->setEnabled(true);

    if(!previewJob)
        inlineRefreshCancelButton->hide();
}

void ListPanel::jumpBack()
{
    func->openUrl(_jumpBackURL);
}

void ListPanel::setJumpBack(QUrl url)
{
    _jumpBackURL = url;
}

void ListPanel::slotVfsError(QString msg)
{
    refreshColors();
    vfsError->setText(i18n("Error: %1", msg));
    vfsError->show();
}

void ListPanel::showButtonMenu(QToolButton *b)
{
    if(this != ACTIVE_PANEL)
        slotFocusOnMe();

    if(b->isHidden())
        b->menu()->exec(mapToGlobal(clientArea->pos()));
    else
        b->click();
}

void ListPanel::openBookmarks()
{
    showButtonMenu(bookmarksButton);
}

void ListPanel::openHistory()
{
    showButtonMenu(historyButton);
}

void ListPanel::openMedia()
{
    showButtonMenu(mediaButton);
}

void ListPanel::rightclickMenu()
{
    if (view->getCurrentKrViewItem())
        popRightClickMenu(mapToGlobal(view->getCurrentKrViewItem()->itemRect().topLeft()));
}

void ListPanel::toggleSyncBrowse()
{
    syncBrowseButton->toggle();
}

void ListPanel::editLocation()
{
    urlNavigator->setUrlEditable(true);
    urlNavigator->setFocus();
    urlNavigator->editor()->lineEdit()->selectAll();
}

void ListPanel::showSearchBar()
{
    searchBar->showBar();
}

void ListPanel::showSearchFilter()
{
    searchBar->showBar(KrSearchBar::MODE_FILTER);
}

void ListPanel::saveSettings(KConfigGroup cfg, bool saveHistory)
{
    QUrl url = virtualPath();
    url.setPassword(QString()); // make sure no password is saved
    cfg.writeEntry("Url", url.toString());
    cfg.writeEntry("Type", getType());
    cfg.writeEntry("Properties", getProperties());
    if(saveHistory)
        func->history->save(KConfigGroup(&cfg, "History"));
    view->saveSettings(KConfigGroup(&cfg, "View"));

    // splitter/popup state
    if (popup && !popup->isHidden()) {
        popup->saveSettings(KConfigGroup(&cfg, "PanelPopup"));
        cfg.writeEntry("PopupPosition", popupPosition());
        cfg.writeEntry("SplitterSizes", splt->saveState());
        cfg.writeEntry("PopupPage", popup->currentPage());
    } else {
        cfg.deleteEntry("PopupPosition");
        cfg.deleteEntry("SplitterSizes");
        cfg.deleteEntry("PopupPage");
    }
}

void ListPanel::restoreSettings(KConfigGroup cfg)
{
    changeType(cfg.readEntry("Type", defaultPanelType()));

    setProperties(cfg.readEntry("Properties", 0));
    view->restoreSettings(KConfigGroup(&cfg, "View"));

    func->files()->vfs_enableRefresh(true);

    _realPath = QUrl::fromLocalFile(ROOT_DIR);

    if(func->history->restore(KConfigGroup(&cfg, "History")))
        func->refresh();
    else {
        QUrl url(cfg.readEntry("Url", ROOT_DIR));
        if (!url.isValid())
            url = QUrl::fromLocalFile(ROOT_DIR);
        func->openUrl(url);
    }

    setJumpBack(func->history->currentUrl());

    if (cfg.hasKey("PopupPosition")) { // popup was visible, restore
        togglePanelPopup(); // create and show
        popup->restoreSettings(KConfigGroup(&cfg, "PanelPopup"));
        setPopupPosition(cfg.readEntry("PopupPosition", 42 /* dummy */));
        splt->restoreState(cfg.readEntry("SplitterSizes", QByteArray()));
        popup->setCurrentPage(cfg.readEntry("PopupPage", 0));
    }
}

void ListPanel::updatePopupPanel(KrViewItem *item)
{
    // which panel to display on?
    PanelPopup *p;
    if(popup && !popup->isHidden())
        p = popup;
    else if(otherPanel()->gui->popup && !otherPanel()->gui->popup->isHidden())
        p = otherPanel()->gui->popup;
    else
        return;

    p->update(item ? func->files()->vfs_search(item->name()) : 0);
}

void ListPanel::otherPanelChanged()
{
    func->syncURL = QUrl();
}

void ListPanel::getFocusCandidates(QVector<QWidget*> &widgets)
{
    if(urlNavigator->editor()->isVisible())
        widgets << urlNavigator->editor();
    if(view->widget()->isVisible())
        widgets << view->widget();
    if(popup && popup->isVisible())
        widgets << popup;
}

void ListPanel::updateButtons()
{
    backButton->setEnabled(func->history->canGoBack());
    forwardButton->setEnabled(func->history->canGoForward());
    historyButton->setEnabled(func->history->count() > 1);
    cdRootButton->setEnabled(!virtualPath().matches(QUrl::fromLocalFile(ROOT_DIR),
                                                    QUrl::StripTrailingSlash));
    cdUpButton->setEnabled(!func->files()->isRoot());
    cdHomeButton->setEnabled(!func->atHome());
}

void ListPanel::newTab(KrViewItem *it)
{
    if (!it)
        return;
    else if (it->name() == "..") {
        newTab(KIO::upUrl(virtualPath()), true);
    } else if (ITEM2VFILE(this, it)->vfile_isDir()) {
        QUrl url = virtualPath();
        url = url.adjusted(QUrl::StripTrailingSlash);
        url.setPath(url.path() + '/' + (it->name()));
        newTab(url, true);
    }
}

void ListPanel::resetNavigatorMode()
{
    if (isNavigatorEditModeSet())
        return;

    // set to "navigate" mode if url wasn't changed
    if (urlNavigator->uncommittedUrl().matches(virtualPath(), QUrl::StripTrailingSlash)) {
        // NOTE: this also sets focus to the navigator
        urlNavigator->setUrlEditable(false);
        slotFocusOnMe();
    }
}

int ListPanel::popupPosition() const
{
    int pos = splt->orientation() == Qt::Vertical ? 1 : 0;
    return pos + (qobject_cast<PanelPopup*>(splt->widget(0)) == NULL ? 2 : 0);
}

void ListPanel::setPopupPosition(int pos)
{
    splt->setOrientation(pos % 2 == 0 ? Qt::Horizontal : Qt::Vertical);
    if ((pos < 2) != (qobject_cast<PanelPopup*>(splt->widget(0)) != NULL)) {
        splt->insertWidget(0, splt->widget(1)); // swapping widgets in splitter
    }
}
