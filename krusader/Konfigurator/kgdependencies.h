/***************************************************************************
                        kgdependencies.h  -  description
                             -------------------
    copyright            : (C) 2004 by Csaba Karai
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

#ifndef __KGDEPENDENCIES_H__
#define __KGDEPENDENCIES_H__

#include "konfiguratorpage.h"
//Added by qt3to4:
#include <Q3GridLayout>

class QTabWidget;


class KgDependencies : public KonfiguratorPage
{
  Q_OBJECT

public:
  KgDependencies( bool first, QWidget* parent=0,  const char* name=0 );

  virtual int activeSubPage();
  
private:
  void addApplication( QString name, Q3GridLayout *grid, int row, QWidget *parent, int page, QString additionalList=QString::null );
  
public slots:
  void slotApply( QObject *obj, QString cls, QString name );
  
private:
  QTabWidget *tabWidget;
};

#endif /* __KGDEPENDENCIES_H__ */
