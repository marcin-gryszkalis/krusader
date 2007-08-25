/***************************************************************************
                    kurllistrequester.cpp  -  description
                             -------------------
    copyright            : (C) 2005 by Csaba Karai
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

#include "kurllistrequester.h"
#include "../VFS/vfs.h"
#include <qpixmap.h>
#include <qcursor.h>
#include <qlayout.h>
//Added by qt3to4:
#include <Q3GridLayout>
#include <QKeyEvent>
#include <kfiledialog.h>
#include <kmenu.h>
#include <kiconloader.h>
#include <klocale.h>
#include <kmessagebox.h>

#define DELETE_ITEM_ID    100

KURLListRequester::KURLListRequester( QWidget *parent, const char * name ) : QWidget( parent, name )
{
  KIconLoader *iconLoader = new KIconLoader();
  QPixmap imageAdd = iconLoader->loadIcon( "1downarrow", K3Icon::Panel, 16 );
  QPixmap imageFolder = iconLoader->loadIcon( "folder", K3Icon::Panel, 16 );
    
  // Creating the widget
  
  Q3GridLayout *urlListRequesterGrid = new Q3GridLayout( this );
  urlListRequesterGrid->setSpacing( 0 );
  urlListRequesterGrid->setMargin( 0 );
    
  urlLineEdit = new KLineEdit( this, "urlLineEdit" );
  urlListRequesterGrid->addWidget( urlLineEdit, 0, 0 );
      
  urlListBox = new Q3ListBox( this, "urlListBox" );
  urlListBox->setSelectionMode( Q3ListBox::Extended );
  urlListRequesterGrid->addMultiCellWidget( urlListBox, 1, 1, 0, 2 );

  urlAddBtn = new QToolButton( this, "urlAddBtn" );
  urlAddBtn->setText( "" );
  urlAddBtn->setPixmap( imageAdd );
  urlListRequesterGrid->addWidget( urlAddBtn, 0, 1 );
    
  urlBrowseBtn = new QToolButton( this, "urlBrowseBtn" );
  urlBrowseBtn->setText( "" );
  urlBrowseBtn->setPixmap( imageFolder );
  urlListRequesterGrid->addWidget( urlBrowseBtn, 0, 2 );

  // add shell completion
    
  completion.setMode( KUrlCompletion::FileCompletion );
  urlLineEdit->setCompletionObject( &completion );
  
  // connection table
  
  connect( urlAddBtn, SIGNAL( clicked() ), this, SLOT( slotAdd() ) );
  connect( urlBrowseBtn, SIGNAL( clicked() ), this, SLOT( slotBrowse() ) );
  connect( urlLineEdit, SIGNAL( returnPressed(const QString&) ), this, SLOT( slotAdd() ) );
  connect( urlListBox, SIGNAL( rightButtonClicked ( Q3ListBoxItem *, const QPoint & ) ), this,
                       SLOT( slotRightClicked( Q3ListBoxItem * ) ) );
}

void KURLListRequester::slotAdd()
{
  QString text = urlLineEdit->text().simplified();
  if( text.length() )
  {  
    QString error = QString();    
    emit checkValidity( text, error );
    
    if( !error.isNull() )
      KMessageBox::error( this, error );
    else
    {  
      urlListBox->insertItem( text );
      urlLineEdit->clear();
    }
  }
}

void KURLListRequester::slotBrowse()
{
  KUrl url = KFileDialog::getExistingURL( QString(), this );
  if( !url.isEmpty())
    urlLineEdit->setText( vfs::pathOrUrl( url ) );
  urlLineEdit->setFocus();
}

void KURLListRequester::keyPressEvent(QKeyEvent *e)
{
  if( e->key() == Qt::Key_Delete )
  {
    if( urlListBox->hasFocus() )
    {
      deleteSelectedItems();
      return;
    }
  }

  QWidget::keyPressEvent( e );
}

void KURLListRequester::deleteSelectedItems()
{
  int i=0;
  Q3ListBoxItem *item;

  while( (item = urlListBox->item(i)) )
  {
    if( item->isSelected() )
    {
      urlListBox->removeItem( i );
      continue;
    }
    i++;
  }
}

void KURLListRequester::slotRightClicked( Q3ListBoxItem *item )
{
  if( item == 0 )
    return;
    
  KMenu popupMenu( this );
  popupMenu.insertItem( i18n( "Delete" ), DELETE_ITEM_ID );
  
  switch( popupMenu.exec( QCursor::pos() ) )
  {
  case DELETE_ITEM_ID:
    if( item->isSelected() )
      deleteSelectedItems();
    else
      urlListBox->removeItem( urlListBox->index( item ) );
    break;
  }
}

KUrl::List KURLListRequester::urlList()
{
  KUrl::List urls;
  
  QString text = urlLineEdit->text().simplified();
  if (!text.isEmpty())
  {
    QString error = QString();
    emit checkValidity( text, error );
    if( error.isNull() )
      urls.append( vfs::fromPathOrUrl( text ) );
  }
    
  Q3ListBoxItem *item = urlListBox->firstItem();
  while ( item )
  {    
    QString text = item->text().simplified();
    
    QString error = QString();
    emit checkValidity( text, error );    
    if( error.isNull() )
      urls.append( vfs::fromPathOrUrl( text ) );
      
    item = item->next();
  }
    
  return urls;
}

void KURLListRequester::setUrlList( KUrl::List urlList )
{
  urlLineEdit->clear();
  urlListBox->clear();

  KUrl::List::iterator it;
    
  for ( it = urlList.begin(); it != urlList.end(); ++it )
    urlListBox->insertItem( vfs::pathOrUrl(*it) );
}

#include "kurllistrequester.moc"
