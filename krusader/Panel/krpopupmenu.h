/*****************************************************************************
 * Copyright (C) 2003 Shie Erlich <erlich@users.sourceforge.net>             *
 * Copyright (C) 2003 Rafi Yanai <yanai@users.sourceforge.net>               *
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

#ifndef KRPOPUPMENU_H
#define KRPOPUPMENU_H

// QtWidgets
#include <QMenu>

#include <KService/KService>
#include <KIOWidgets/KFileItemActions>

#include "krpreviewpopup.h"

class KActionCollection;
class KrPanel;

// should be renamed to KrContextMenu or similar
class KrPopupMenu : public QMenu
{
    Q_OBJECT
public:
    static void run(const QPoint &pos, KrPanel *panel);

protected:
    KrPopupMenu(KrPanel *thePanel, QWidget *parent = 0);
    ~KrPopupMenu();
    void performAction(int id);
    void addEmptyMenuEntries(); // adds the choices for a menu without selected items
    void addCreateNewMenu(); // adds a "create new" submenu

    enum ID {
        OPEN_ID,
        BROWSE_ID,
        OPEN_WITH_ID,
        OPEN_KONQ_ID,
        OPEN_TERM_ID,
        OPEN_TAB_ID,
        PREVIEW_ID,
        KONQ_MENU_ID,
        CHOOSE_ID,
        DELETE_ID,
        COPY_ID,
        MOVE_ID,
        RENAME_ID,
        PROPERTIES_ID,
        MOUNT_ID,
        UNMOUNT_ID,
        TRASH_ID,
        NEW_LINK_ID,
        NEW_SYMLINK_ID,
        REDIRECT_LINK_ID,
        EMPTY_TRASH_ID,
        RESTORE_TRASHED_FILE_ID,
        SYNC_SELECTED_ID,
        SEND_BY_EMAIL_ID,
        LINK_HANDLING_ID,
        EJECT_ID,
        COPY_CLIP_ID,
        MOVE_CLIP_ID,
        PASTE_CLIP_ID,
        MKDIR_ID,
        NEW_TEXT_FILE_ID,
        CREATE_NEW_ID,
        SERVICE_LIST_ID // ALWAYS KEEP THIS ONE LAST!!!
    };

private:
    KrPanel *panel;
    bool empty, multipleSelections;
    QMenu openWith, linkPopup, createNewPopup;
    KrPreviewPopup preview;
    KActionCollection *actions;
    KFileItem *_item;
    KFileItemList _items;
    KService::List offers;
    KFileItemActions fileItemActions;
};

#endif
