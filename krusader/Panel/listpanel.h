/***************************************************************************
                               listpanel.h
                            -------------------
   begin                : Thu May 4 2000
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

                                                    H e a d e r    F i l e

***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************/


#ifndef LISTPANEL_H
#define LISTPANEL_H

// QtCore
#include <QString>
#include <QDir>
#include <QList>
#include <QEvent>
#include <QPointer>
#include <QUrl>
// QtGui
#include <QPixmap>
#include <QPixmapCache>
#include <QIcon>
#include <QDropEvent>
#include <QKeyEvent>
#include <QShowEvent>
#include <QHideEvent>
// QtWidgets
#include <QProgressBar>
#include <QWidget>
#include <QLabel>
#include <QLayout>
#include <QToolButton>
#include <QGridLayout>

#include <KIOCore/KFileItem>
#include <KCompletion/KLineEdit>
#include <KIOFileWidgets/KUrlNavigator>

#include "krpanel.h"
#include "krviewitem.h"
#include "../Dialogs/krsqueezedtextlabel.h"

#define PROP_SYNC_BUTTON_ON               1
#define PROP_LOCKED                       2

class KrView;
class KrSearchBar;
class DirHistoryButton;
class MediaButton;
class PanelPopup;
class SyncBrowseButton;
class KrBookmarkButton;
class ListPanelFunc;
class QSplitter;
class KrErrorDisplay;
class ListPanelActions;

class ListPanel : public QWidget, public KrPanel
{
    friend class ListPanelFunc;
    Q_OBJECT
public:
#define ITEM2VFILE(PANEL_PTR, KRVIEWITEM)  PANEL_PTR->func->files()->vfs_search(KRVIEWITEM->name())
#define NAME2VFILE(PANEL_PTR, STRING_NAME) PANEL_PTR->func->files()->vfs_search(STRING_NAME)
    // constructor create the panel, but DOESN'T fill it with data, use start()
    ListPanel(QWidget *parent, AbstractPanelManager *manager, KConfigGroup cfg = KConfigGroup());
    ~ListPanel();

    virtual void otherPanelChanged() Q_DECL_OVERRIDE;

    void start(QUrl url = QUrl(), bool immediate = false);

    void reparent(QWidget *parent, AbstractPanelManager *manager);

    int getType() {
        return panelType;
    }
    void changeType(int);
    bool isLocked() {
        return _locked;
    }
    void setLocked(bool lck) {
        _locked = lck;
    }

    ListPanelActions *actions() {
        return _actions;
    }
    QString realPath() const;
    QString getCurrentName();
    void getSelectedNames(QStringList* fileNames) {
        view->getSelectedItems(fileNames);
    }
    void setButtons();
    void setJumpBack(QUrl url);

    int  getProperties();
    void setProperties(int);

    void getFocusCandidates(QVector<QWidget*> &widgets);

    void saveSettings(KConfigGroup cfg, bool saveHistory);
    void restoreSettings(KConfigGroup cfg);

public slots:
    void popRightClickMenu(const QPoint&);
    void popEmptyRightClickMenu(const QPoint &);
    void compareDirs(bool otherPanelToo = true);
    void slotFocusOnMe(bool focus = true);
    void slotUpdateTotals();
    void slotStartUpdate(); // react to file changes in vfs (path change or refresh)
    void slotGetStats(const QUrl &url); // get the disk-free stats
    void togglePanelPopup();
    void panelActive(); // called when the panel becomes active
    void panelInactive(); // called when panel becomes inactive
    void vfs_refresh(KJob *job);
    void refreshColors();
    void inlineRefreshCancel();

    void openMedia();
    void openHistory();
    void openBookmarks();
    void rightclickMenu();
    void toggleSyncBrowse();
    void editLocation();
    void showSearchBar();
    void showSearchFilter();
    void jumpBack();
    void setJumpBack() {
        setJumpBack(virtualPath());
    }

    ///////////////////////// service functions - called internally ////////////////////////
    void prepareToDelete();                   // internal use only

protected:
    virtual void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    virtual void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE {
        slotFocusOnMe();
    }
    virtual void showEvent(QShowEvent *) Q_DECL_OVERRIDE;
    virtual void hideEvent(QHideEvent *) Q_DECL_OVERRIDE;
    virtual bool eventFilter(QObject * watched, QEvent * e) Q_DECL_OVERRIDE;

    void showButtonMenu(QToolButton *b);
    void createView();
    void updateButtons();

    static int defaultPanelType();
    static bool isNavigatorEditModeSet(); // return the navigator edit mode setting

protected slots:
    void updatePopupPanel(KrViewItem *item);
    void handleDrop(QDropEvent *event, bool onView = false); // handle drops on frame or view
    void handleDrop(const QUrl &destination, QDropEvent *event); // handle drops with destination
    void startDragging(QStringList, QPixmap);
    void slotJobResult(KJob *job);
    void slotPreviewJobStarted(KJob *job);
    void slotPreviewJobPercent(KJob *job, unsigned long percent);
    void slotPreviewJobResult(KJob *job);
    // those handle the in-panel refresh notifications
    void slotJobStarted(KIO::Job* job);
    void inlineRefreshInfoMessage(KJob* job, const QString &msg);
    void inlineRefreshListResult(KJob* job);
    void inlineRefreshPercent(KJob*, unsigned long);
    void slotVfsError(QString msg);
    void newTab(KrViewItem *item);
    void newTab(const QUrl &url, bool nextToThis = false) {
        _manager->newTab(url, nextToThis ? this : 0);
    }
    void resetNavigatorMode(); // set navigator mode after focus was lost

signals:
    void signalStatus(QString msg); // emmited when we need to update the status bar
    void pathChanged(ListPanel *panel); // directory changed or refreshed
    void pathChanged(const QUrl &url); // directory changed or refreshed
    void activate(); // emitted when the user changes panels
    void finishedDragging(); // NOTE: currently not used
    void refreshColors(bool active);
    // emitted when we have to update the path label width
    void refreshPathLabel();

protected:
    int panelType;
    QUrl _realPath; // named with _ to keep realPath() compatibility
    QUrl _jumpBackURL;
    int colorMask;
    bool compareMode;
    //FilterSpec    filter;
    KJob *previewJob;
    KIO::Job *inlineRefreshJob;
    ListPanelActions *_actions;

    QPixmap currDragPix;
    QWidget *clientArea;
    QSplitter *splt;
    KUrlNavigator* urlNavigator;
    KrSearchBar* searchBar;
    QToolButton *backButton, *forwardButton;
    QToolButton *cdRootButton;
    QToolButton *cdHomeButton;
    QToolButton *cdUpButton;
    QToolButton *cdOtherButton;
    QToolButton *popupPositionBtn;
    QToolButton *popupBtn;
    PanelPopup *popup; // lazy initialized
    KrBookmarkButton *bookmarksButton;
    KrSqueezedTextLabel *status, *totals, *freeSpace;

    QProgressBar *previewProgress;
    DirHistoryButton* historyButton;
    MediaButton *mediaButton;
    SyncBrowseButton *syncBrowseButton;
    QToolButton *inlineRefreshCancelButton;
    KrErrorDisplay *vfsError;

private:
    bool handleDropInternal(QDropEvent *event, const QString &dir);
    int popupPosition() const; // 0: West, 1: North, 2: East, 3: South
    void setPopupPosition(int);

private:
    bool _locked;
    QList<int> popupSizes;
};

#endif
