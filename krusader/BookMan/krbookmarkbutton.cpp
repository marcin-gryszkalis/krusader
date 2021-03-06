/*****************************************************************************
 * Copyright (C) 2002 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2002 Rafi Yanai <yanai@users.sourceforge.net>               *
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

#include "krbookmarkbutton.h"
#include "krbookmarkhandler.h"
#include "../krglobal.h"

// QtGui
#include <QPixmap>
// QtWidgets
#include <QAction>
#include <QMenu>

#include <KI18n/KLocalizedString>
#include <KIconThemes/KIconLoader>

KrBookmarkButton::KrBookmarkButton(QWidget *parent): QToolButton(parent)
{
    setAutoRaise(true);
    setIcon(QIcon::fromTheme("bookmarks"));
    setText(i18n("BookMan II"));
    setToolTip(i18n("BookMan II"));
    setPopupMode(QToolButton::InstantPopup);
    setAcceptDrops(false);

    acmBookmarks = new KActionMenu(QIcon::fromTheme("bookmarks"), i18n("Bookmarks"), this);
    acmBookmarks->setDelayed(false);
    // TODO KF5 : explicit cast as QMenu doesn't have those methods
    //(acmBookmarks->menu())->setKeyboardShortcutsEnabled(true);
    //(acmBookmarks->menu())->setKeyboardShortcutsExecute(true);

    setMenu(acmBookmarks->menu());
    connect(acmBookmarks->menu(), SIGNAL(aboutToShow()), this, SLOT(populate()));
    connect(acmBookmarks->menu(), SIGNAL(aboutToShow()), this, SIGNAL(aboutToShow()));
    populate();
}

void KrBookmarkButton::populate()
{
    krBookMan->populate(static_cast<QMenu*>(menu()));
}

void KrBookmarkButton::showMenu()
{
    populate();
    menu()->exec(mapToGlobal(QPoint(0, height())));
}

