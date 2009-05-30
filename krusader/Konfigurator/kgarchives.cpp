/***************************************************************************
                         kgarchives.cpp  -  description
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

#include <QtGui/QPushButton>
#include <QGridLayout>

#include "kgarchives.h"
#include "krresulttable.h"
#include "krresulttabledialog.h"

#include "searchobject.h"
#include "../defaults.h"
#include "../krusader.h"
#include "../VFS/krarchandler.h"

KgArchives::KgArchives( bool first, QWidget* parent ) :
      KonfiguratorPage( first, parent )
{
  QWidget *innerWidget = new QFrame( this );
  setWidget( innerWidget );
  setWidgetResizable( true );
  QGridLayout *kgArchivesLayout = new QGridLayout( innerWidget );
  kgArchivesLayout->setSpacing( 6 );

  //  -------------------------- GENERAL GROUPBOX ----------------------------------

  QGroupBox *generalGrp = createFrame( i18n( "General" ), innerWidget );
  QGridLayout *generalGrid = createGridLayout( generalGrp );

  addLabel( generalGrid, 0, 0, i18n( "Krusader transparently handles the following types of archives:" ),
            generalGrp );

  KONFIGURATOR_CHECKBOX_PARAM packers[] =
  //   cfg_class  cfg_name   default   text             restart tooltip
    {{"Archives","Do Tar",   _DoTar,   i18n( "Tar" ),   false,  ""},
     {"Archives","Do GZip",  _DoGZip,  i18n( "GZip" ),  false,  ""},
     {"Archives","Do LZMA",  _DoLZMA,  i18n( "LZMA" ),  false,  ""},
     {"Archives","Do BZip2", _DoBZip2, i18n( "BZip2" ), false,  ""},
     {"Archives","Do UnZip", _DoUnZip, i18n( "Zip" ),   false,  ""},
     {"Archives","Do UnRar", _DoUnRar, i18n( "Rar" ),   false,  ""},
     {"Archives","Do Unarj", _DoArj,   i18n( "Arj" ),   false,  ""},
     {"Archives","Do RPM",   _DoRPM,   i18n( "Rpm" ),   false,  ""},
     {"Archives","Do UnAce", _DoUnAce, i18n( "Ace" ),   false,  ""},
     {"Archives","Do Lha",   _DoLha,   i18n( "Lha" ),   false,  ""},
     {"Archives","Do DEB",   _DoDEB,   i18n( "Deb" ),   false,  ""},
     {"Archives","Do 7z",    _Do7z,    i18n( "7zip" ),  false,  ""}
    };

  cbs = createCheckBoxGroup( 3, 0, packers, 12, generalGrp );
  generalGrid->addWidget( cbs, 1, 0 );

  addLabel( generalGrid, 2, 0, i18n( "The archives that are \"greyed-out\" were unavailable on your\nsystem last time Krusader checked. If you wish Krusader to\nsearch again, click the 'Auto Configure' button." ),
            generalGrp );

  QWidget *hboxWidget = new QWidget( generalGrp );
  QHBoxLayout *hbox = new QHBoxLayout( hboxWidget );

  QWidget * spcer1 = createSpacer( hboxWidget );
  hbox->addWidget( spcer1 );

  QPushButton *btnAutoConfigure = new QPushButton( i18n( "Auto Configure" ), hboxWidget );
  hbox->addWidget( btnAutoConfigure );

  QWidget *spcer2 = createSpacer( hboxWidget );
  hbox->addWidget( spcer2 );

  generalGrid->addWidget( hboxWidget, 3, 0 );
  connect( btnAutoConfigure, SIGNAL( clicked() ), this, SLOT( slotAutoConfigure() ) );
 
  kgArchivesLayout->addWidget( generalGrp, 0 ,0 );

  //  ------------------------ FINE-TUNING GROUPBOX --------------------------------

  QGroupBox *fineTuneGrp = createFrame( i18n( "Fine-Tuning" ), innerWidget );
  QGridLayout *fineTuneGrid = createGridLayout( fineTuneGrp );

  KONFIGURATOR_CHECKBOX_PARAM finetuners[] =
  //   cfg_class  cfg_name                  default           text                                          restart ToolTip
    {//{"Archives","Allow Move Into Archive", _MoveIntoArchive, i18n( "Allow moving into archives" ),         false,  i18n( "This action can be tricky, since system failure during the process\nmight result in misplaced files. If this happens,\nthe files are stored in a temp directory inside /tmp." )},
     {"Archives","Test Archives",           _TestArchives,    i18n( "Test archive after packing" ), false,  i18n( "Check the archive's integrity after packing it." )},
     {"Archives","Test Before Unpack",      _TestBeforeUnpack,i18n( "Test archive before unpacking" ), false,  i18n( "Some corrupted archives might cause a crash; therefore, testing is suggested." )}};

  KonfiguratorCheckBoxGroup *finetunes = createCheckBoxGroup( 1, 0, finetuners, 2, fineTuneGrp );

  disableNonExistingPackers();
  fineTuneGrid->addWidget( finetunes, 1, 0 );

  kgArchivesLayout->addWidget( fineTuneGrp, 1 ,0 );
  
  if( first )
    slotAutoConfigure();

}


void KgArchives::slotAutoConfigure()
{
  KrResultTableDialog* dia = new KrResultTableDialog(this, KrResultTableDialog::Archiver, i18n("Search results"), i18n("Searching for packers..."),
    "package", i18n("Make sure to install new packers in your <code>$PATH</code> (e.g. /usr/bin)"));
  dia->exec();

  disableNonExistingPackers();
}

void KgArchives::disableNonExistingPackers()
{
  #define PS(x) lst.contains(x)>0

  QStringList lst=KRarcHandler::supportedPackers(); // get list of availble packers
  cbs->find( "Do Tar" )->setEnabled(PS("tar"));
  cbs->find( "Do GZip" )->setEnabled(PS("gzip"));
  cbs->find( "Do BZip2" )->setEnabled(PS("bzip2"));
  cbs->find( "Do LZMA" )->setEnabled(PS("lzma"));
  cbs->find( "Do UnZip" )->setEnabled(PS("unzip"));
  cbs->find( "Do Lha" )->setEnabled(PS("lha"));
  cbs->find( "Do RPM" )->setEnabled(PS("rpm") || PS("cpio"));
  cbs->find( "Do UnRar" )->setEnabled(PS("unrar") || PS("rar") );
  cbs->find( "Do UnAce" )->setEnabled(PS("unace"));
  cbs->find( "Do Unarj" )->setEnabled(PS("unarj") || PS("arj") );
  cbs->find( "Do DEB" )->setEnabled(PS("dpkg") && PS("tar") );
  cbs->find( "Do 7z" )->setEnabled( PS("7z") );

  KConfigGroup group( krConfig, "Archives" );
  group.writeEntry( "Supported Packers", lst );
}

bool KgArchives::apply()
{
  KConfigGroup group( krConfig, "Archives" );
  group.writeEntry("Supported Packers",KRarcHandler::supportedPackers());
  return KonfiguratorPage::apply();
}

void KgArchives::setDefaults()
{
  KConfigGroup group( krConfig, "Archives" );
  group.writeEntry("Supported Packers",KRarcHandler::supportedPackers());
  return KonfiguratorPage::setDefaults();
}

#include "kgarchives.moc"
