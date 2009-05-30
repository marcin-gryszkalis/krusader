#ifndef KICONS_H
#define KICONS_H

#include <QtGui/QIcon>
#include <QtGui/QPixmap>
#include <kiconloader.h>
#include <kicontheme.h>

// can be used everywhere - EXCEPT in KFileList related calls, loads the icon according to
// KIconLoader::Desktop settings and resizes it to a smaller size. If this is used to toolbuttons,
// the the icon is resized again to fit into the toolbutton or menu.
// IMPORTANT: this SHOULD NOT BE USED for actions. If creating an action, just state the file-name
// of the icon to allow automatic resizing when needed.
#define LOADICON(W, X) QIcon(krLoader->loadIcon(X,KIconLoader::Desktop)).pixmap(W->style()->pixelMetric(QStyle::PM_SmallIconSize),QIcon::Normal)

// used only for calls within the kfilelist framework, handles icon sizes
QPixmap FL_LOADICON(QString name);

extern  const char * no_xpm[];
extern  const char * yes_xpm[];
extern  const char * link_xpm[];
extern  const char * black_xpm[];
extern  const char * yellow_xpm[];
extern  const char * green_xpm[];
extern  const char * red_xpm[];
extern  const char * white_xpm[];
extern  const char * blue_xpm[];

#endif
