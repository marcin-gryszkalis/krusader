#include "krbriefview.h"
#include "krbriefviewitem.h"
#include "../defaults.h"
#include "krcolorcache.h"
#include "krselectionmode.h"
#include "../krusader.h"
#include "../kicons.h"
#include "../krslots.h"
#include "../Dialogs/krspecialwidgets.h"
#include "../VFS/krarchandler.h"

#define CANCEL_TWO_CLICK_RENAME {singleClicked = false;renameTimer.stop();}
#define VF	getVfile()

KrBriefView::KrBriefView( QWidget *parent, bool &left, KConfig *cfg, const char *name ):
	KIconView(parent, name), KrView( cfg ), _currDragItem( 0 ), currentlyRenamedItem( 0 ),
	    pressedItem( 0 ) {
	setWidget( this );
	_nameInKConfig = QString( "KrBriefView" ) + QString( ( left ? "Left" : "Right" ) );
	setItemTextPos( QIconView::Right );
	setArrangement( QIconView::TopToBottom );
	setWordWrapIconText( false );

	setBackgroundMode( PaletteBase );
}

KrBriefView::~KrBriefView() {
	delete _properties; _properties = 0;
	delete _operator; _operator = 0;
}

void KrBriefView::setup() {
   { // use the {} so that KConfigGroupSaver will work correctly!
      KConfigGroupSaver grpSvr( _config, "Look&Feel" );
      setFont( _config->readFontEntry( "Filelist Font", _FilelistFont ) );
      // decide on single click/double click selection
      if ( _config->readBoolEntry( "Single Click Selects", _SingleClickSelects ) ) {
         connect( this, SIGNAL( executed( QIconViewItem* ) ), this, SLOT( slotExecuted( QIconViewItem* ) ) );
      } else {
         connect( this, SIGNAL( clicked( QIconViewItem* ) ), this, SLOT( slotClicked( QIconViewItem* ) ) );
         connect( this, SIGNAL( doubleClicked( QIconViewItem* ) ), this, SLOT( slotDoubleClicked( QIconViewItem* ) ) );
      }

      // a change in the selection needs to update totals
      connect( this, SIGNAL( onItem( QIconViewItem* ) ), this, SLOT( slotItemDescription( QIconViewItem* ) ) );
      connect( this, SIGNAL( contextMenuRequested( QIconViewItem*, const QPoint& ) ),
               this, SLOT( handleContextMenu( QIconViewItem*, const QPoint& ) ) );
		connect( this, SIGNAL( rightButtonPressed(QIconViewItem*, const QPoint&)),
			this, SLOT(slotRightButtonPressed(QIconViewItem*, const QPoint&)));
      connect( this, SIGNAL( currentChanged( QIconViewItem* ) ), this, SLOT( setNameToMakeCurrent( QIconViewItem* ) ) );
      connect( this, SIGNAL( currentChanged( QIconViewItem* ) ), this, SLOT( transformCurrentChanged( QIconViewItem* ) ) );
      connect( this, SIGNAL( mouseButtonClicked ( int, QIconViewItem *, const QPoint & ) ),
               this, SLOT( slotMouseClicked ( int, QIconViewItem *, const QPoint & ) ) );
      connect( &KrColorCache::getColorCache(), SIGNAL( colorsRefreshed() ), this, SLOT( refreshColors() ) );
   }
   
   // determine basic settings for the view
   setAcceptDrops( true );
   setItemsMovable( false );

   // allow in-place renaming

   connect( this, SIGNAL( itemRenamed ( QIconViewItem * ) ), 
            this, SLOT( inplaceRenameFinished( QIconViewItem * ) ) );
   connect( &renameTimer, SIGNAL( timeout() ), this, SLOT( renameCurrentItem() ) );
   connect( &contextMenuTimer, SIGNAL (timeout()), this, SLOT (showContextMenu()));

	// TODO: connect( header(), SIGNAL(clicked(int)), this, SLOT(slotSortOrderChanged(int )));

   setSelectionMode( QIconView::Extended );

   setFocusPolicy( StrongFocus );
   restoreSettings();
   refreshColors();

   CANCEL_TWO_CLICK_RENAME;	
}

void KrBriefView::resizeEvent ( QResizeEvent * resEvent )
{
   QIconView::resizeEvent( resEvent );

   setGridX( width() / 3 );

   // QT bug, it's important for recalculating the bounding rectangle
   for( QIconViewItem * item = firstItem(); item; item = item->nextItem() )
   {
      QString txt = item->text();
      item->setText( "" );
      item->setText( txt );
   }

   arrangeItemsInGrid();
}

// if vfile passes the filter, create an item, otherwise, drop it
KrViewItem *KrBriefView::preAddItem( vfile *vf ) {
   bool isDir = vf->vfile_isDir();
   if ( !isDir || ( isDir && ( _properties->filter & KrViewProperties::ApplyToDirs ) ) ) {
      switch ( _properties->filter ) {
            case KrViewProperties::All :
               break;
            case KrViewProperties::Custom :
            if ( !_properties->filterMask.match( vf ) ) return 0;
            break;
            case KrViewProperties::Dirs:
            if ( !vf->vfile_isDir() ) return 0;
            break;
            case KrViewProperties::Files:
            if ( vf->vfile_isDir() ) return 0;
            break;
            case KrViewProperties::ApplyToDirs :
            break; // no-op, stop compiler complaints
      }
   }
   // passed the filter ...
	return new KrBriefViewItem( this, lastItem(), vf );
}

bool KrBriefView::preDelItem(KrViewItem *item) {
   if( item ) {
      KrBriefViewItem * viewItem = dynamic_cast<KrBriefViewItem*>( item );
      if( viewItem == currentlyRenamedItem ) {
         currentlyRenamedItem->cancelRename();
         currentlyRenamedItem = 0;
      }
   }

   return true;
}

void KrBriefView::addItems( vfs *v, bool addUpDir ) {
   QIconViewItem * item = firstItem();
   QIconViewItem * currentItem = item;

   // add the up-dir arrow if needed
   if ( addUpDir ) {
      new KrBriefViewItem( this, ( QIconViewItem* ) 0L, ( vfile* ) 0L );
   }


   // text for updating the status bar
   QString statusText = QString("%1/  ").arg( v->vfs_getOrigin().fileName() ) + i18n("Directory");

   bool as = sortDirection();
   setSorting( false, as ); // disable sorting*/

   for ( vfile * vf = v->vfs_getFirstFile(); vf != 0 ; vf = v->vfs_getNextFile() ) {
      //size = KRpermHandler::parseSize( vf->vfile_getSize() );
      //name = vf->vfile_getName();
      bool isDir = vf->vfile_isDir();
      if ( !isDir || ( isDir && ( _properties->filter & KrViewProperties::ApplyToDirs ) ) ) {
         switch ( _properties->filter ) {
               case KrViewProperties::All :
               break;
               case KrViewProperties::Custom :
               if ( !_properties->filterMask.match( vf ) )
                  continue;
               break;
               case KrViewProperties::Dirs:
               if ( !vf->vfile_isDir() )
                  continue;
               break;
               case KrViewProperties::Files:
               if ( vf->vfile_isDir() )
                  continue;
               break;

               case KrViewProperties::ApplyToDirs :
               break; // no-op, stop compiler complaints
         }
      }

      KrBriefViewItem *bvitem = new KrBriefViewItem( this, item, vf );
      _dict.insert( vf->vfile_getName(), bvitem );
      if ( isDir )
         ++_numDirs;
      else
         _countSize += bvitem->VF->vfile_getSize();
      ++_count;
      // if the item should be current - make it so
      if ( bvitem->name() == nameToMakeCurrent() )
      {
         currentItem = static_cast<QIconViewItem*>(bvitem);
         statusText = bvitem->description();
      }

      //cnt++;
   }


   // re-enable sorting
   setSorting( true, as );
   sort( as );

   if ( !currentItem )
      currentItem = firstItem();
   KIconView::setCurrentItem( currentItem );
   ensureItemVisible( currentItem );

   op()->emitItemDescription( statusText );
}

void KrBriefView::delItem( const QString &name ) {
   KrView::delItem( name );
   arrangeItemsInGrid();
}

void KrBriefView::setCurrentItem( const QString& name ) {
   KrBriefViewItem * it = dynamic_cast<KrBriefViewItem*>(_dict[ name ]);
   if ( it )
      KIconView::setCurrentItem( it );
}

void KrBriefView::clear() {
   if( currentlyRenamedItem ) {
      currentlyRenamedItem->cancelRename();
      currentlyRenamedItem = 0;
   }

   op()->emitSelectionChanged(); /* to avoid rename crash at refresh */
   KIconView::clear();
   KrView::clear();
}

void KrBriefView::slotClicked( QIconViewItem *item ) {
   if ( !item ) return ;

   if ( !modifierPressed ) {
      if ( singleClicked && !renameTimer.isActive() ) {
         KConfig * config = KGlobal::config();
         config->setGroup( "KDE" );
         int doubleClickInterval = config->readNumEntry( "DoubleClickInterval", 400 );

         int msecsFromLastClick = clickTime.msecsTo( QTime::currentTime() );

         if ( msecsFromLastClick > doubleClickInterval && msecsFromLastClick < 5 * doubleClickInterval ) {
            singleClicked = false;
            renameTimer.start( doubleClickInterval, true );
            return ;
         }
      }

      CANCEL_TWO_CLICK_RENAME;
      singleClicked = true;
      clickTime = QTime::currentTime();
      clickedItem = item;
   }
}

void KrBriefView::slotDoubleClicked( QIconViewItem *item ) {
   CANCEL_TWO_CLICK_RENAME;
   if ( !item )
      return ;
   QString tmp = dynamic_cast<KrViewItem*>( item ) ->name();
   op()->emitExecuted(tmp);
}

void KrBriefView::prepareForActive() {
   KrView::prepareForActive();
   setFocus();
   slotItemDescription( currentItem() );
}

void KrBriefView::prepareForPassive() {
   KrView::prepareForPassive();
   CANCEL_TWO_CLICK_RENAME;
   KConfigGroupSaver grpSvr( _config, "Look&Feel" );
   if ( _config->readBoolEntry( "New Style Quicksearch", _NewStyleQuicksearch ) ) {
      if ( MAIN_VIEW ) {
         if ( ACTIVE_PANEL ) {
            if ( ACTIVE_PANEL->quickSearch ) {
               if ( ACTIVE_PANEL->quickSearch->isShown() ) {
                  stopQuickSearch( 0 );
               }
            }
         }
      }
   }
}

void KrBriefView::initProperties() {
	// TODO: move this to a general location, maybe KrViewProperties constructor ?
	_properties = new KrViewProperties;
	KConfigGroupSaver grpSvr( _config, "Look&Feel" );
	_properties->displayIcons = _config->readBoolEntry( "With Icons", _WithIcons );
	_properties->sortMode = static_cast<KrViewProperties::SortSpec>( KrViewProperties::Name |
		KrViewProperties::Descending | KrViewProperties::DirsFirst );
	if ( !_config->readBoolEntry( "Case Sensative Sort", _CaseSensativeSort ) )
      _properties->sortMode = static_cast<KrViewProperties::SortSpec>( _properties->sortMode |
				 KrViewProperties::IgnoreCase );
	_properties->localeAwareCompareIsCaseSensitive = QString( "a" ).localeAwareCompare( "B" ) > 0; // see KDE bug #40131
}

void KrBriefView::slotRightButtonPressed(QIconViewItem*, const QPoint& point) {
	op()->emitEmptyContextMenu(point);
}

void KrBriefView::refreshColors() {
   krConfig->setGroup("Colors");
   bool kdeDefault = krConfig->readBoolEntry("KDE Default"); 
   if ( !kdeDefault ) {
      // KDE default is not choosen: set the background color (as this paints the empty areas) and the alternate color
      bool isActive = hasFocus();
      if ( MAIN_VIEW && ACTIVE_PANEL && ACTIVE_PANEL->view )
         isActive = ( static_cast<KrView *>( this ) == ACTIVE_PANEL->view );
      QColorGroup cg;
      KrColorCache::getColorCache().getColors(cg, KrColorItemType(KrColorItemType::File, false, isActive, false, false));
      setPaletteBackgroundColor( cg.background() );
   } else {
      // KDE default is choosen: set back the background color
      setPaletteBackgroundColor( KGlobalSettings::baseColor() );
   }
}

void KrBriefView::makeItemVisible( const KrViewItem *item ) {
//	qApp->processEvents();  // Please don't remove the comment. Causes crash if it is inserted!
	ensureItemVisible( (QIconViewItem *)( static_cast<const KrBriefViewItem*>( item ) ) ); 
}

void KrBriefView::initOperator() {
	_operator = new KrViewOperator(this, this);
	// QIconView emits selection changed, so chain them to operator
	connect(this, SIGNAL(selectionChanged()), _operator, SIGNAL(selectionChanged()));
}

KrViewItem *KrBriefView::getKrViewItemAt( const QPoint & vp ) {
   return dynamic_cast<KrViewItem*>( KIconView::findItem( vp ) );
}

QString KrBriefView::getCurrentItem() const {
   QIconViewItem * it = currentItem();
   if ( !it )
      return QString::null;
   else
      return dynamic_cast<KrViewItem*>( it ) ->name();
}

void KrBriefView::slotItemDescription( QIconViewItem * item ) {
   KrViewItem * it = static_cast<KrBriefViewItem*>( item );
   if ( !it )
      return ;
   QString desc = it->description();
   op()->emitItemDescription(desc);
}

void KrBriefView::handleQuickSearchEvent( QKeyEvent * e ) {
   switch ( e->key() ) {
         case Key_Insert:
         {
            QKeyEvent ev = QKeyEvent( QKeyEvent::KeyPress, Key_Space, 0, 0 );
            KIconView::keyPressEvent( & ev );
            ev = QKeyEvent( QKeyEvent::KeyPress, Key_Down, 0, 0 );
            keyPressEvent( & ev );
            break;
         }
         case Key_Home:
         {
            QIconView::setCurrentItem( firstItem() );
            QKeyEvent ev = QKeyEvent( QKeyEvent::KeyPress, Key_Down, 0, 0 );
            keyPressEvent( & ev );
            break;
         }
         case Key_End:
         {
            QIconView::setCurrentItem( firstItem() );
            QKeyEvent ev = QKeyEvent( QKeyEvent::KeyPress, Key_Up, 0, 0 );
            keyPressEvent( & ev );
            break;
         }
   }
}

void KrBriefView::slotCurrentChanged( QIconViewItem * item ) {
   CANCEL_TWO_CLICK_RENAME;
   if ( !item )
      return ;
   _nameToMakeCurrent = static_cast<KrBriefViewItem*>( item ) ->name();
}

void KrBriefView::contentsMousePressEvent( QMouseEvent * e ) {
   bool callDefaultHandler = true, processEvent = true, selectionChanged = false;
   pressedItem = 0;

   QIconViewItem * oldCurrent = currentItem();
   QIconViewItem *newCurrent = findItem( e->pos() );
   if (e->button() == RightButton)
   {
	if (KrSelectionMode::getSelectionHandler()->rightButtonSelects() || 
		(((e->state() & ShiftButton) || (e->state() & ControlButton))) && KrSelectionMode::getSelectionHandler()->shiftCtrlRightButtonSelects())
     {
       if (KrSelectionMode::getSelectionHandler()->rightButtonPreservesSelection() && !(e->state() & ShiftButton)
          && !(e->state() & ControlButton) && !(e->state() & AltButton))
       {
         if (newCurrent)
         {
           if (KrSelectionMode::getSelectionHandler()->showContextMenu() >= 0)
           {
             swushSelects = !newCurrent->isSelected();
             lastSwushPosition = newCurrent;
           }
           newCurrent->setSelected(!newCurrent->isSelected(), true);
           newCurrent->repaint();
			  selectionChanged = true;
         }
         callDefaultHandler = false;
         processEvent = false;
         e->accept();
       }
     }
     else
     {
       callDefaultHandler = false;
       processEvent = false;
       e->accept();
     }
   }
   if (e->button() == LeftButton)
   {
     dragStartPos = e->pos();
	  if (KrSelectionMode::getSelectionHandler()->leftButtonSelects() || 
	  		(((e->state() & ShiftButton) || (e->state() & ControlButton))) &&
			KrSelectionMode::getSelectionHandler()->shiftCtrlLeftButtonSelects())
     {
       if (KrSelectionMode::getSelectionHandler()->leftButtonPreservesSelection() && !(e->state() & ShiftButton)
          && !(e->state() & ControlButton) && !(e->state() & AltButton))
       {
         if (newCurrent)
         {
           newCurrent->setSelected(!newCurrent->isSelected(), true);
           newCurrent->repaint();
			  selectionChanged = true;
         }
         callDefaultHandler = false;
         processEvent = false;
         e->accept();
       }
     }
     else
     {
       callDefaultHandler = false;
       processEvent = false;
       e->accept();
     }
   }

   modifierPressed = false;
   if ( (e->state() & ShiftButton) || (e->state() & ControlButton) || (e->state() & AltButton) ) {
      CANCEL_TWO_CLICK_RENAME;
      modifierPressed = true;
   }

   // stop quick search in case a mouse click occured
   KConfigGroupSaver grpSvr( _config, "Look&Feel" );
   if ( _config->readBoolEntry( "New Style Quicksearch", _NewStyleQuicksearch ) ) {
      if ( MAIN_VIEW ) {
         if ( ACTIVE_PANEL ) {
            if ( ACTIVE_PANEL->quickSearch ) {
               if ( ACTIVE_PANEL->quickSearch->isShown() ) {
                  stopQuickSearch( 0 );
               }
            }
         }
      }
   }

   if ( !_focused )
   	op()->emitNeedFocus();
   setFocus();

   if (processEvent && ( (e->state() & ShiftButton) || (e->state() & ControlButton) || (e->state() & AltButton) ) && !KrSelectionMode::getSelectionHandler()->useQTSelection()){
      if ( oldCurrent && newCurrent && oldCurrent != newCurrent && e->state() & ShiftButton ) {
         int oldPos = oldCurrent->index();
         int newPos = newCurrent->index();
         QIconViewItem *top = 0, *bottom = 0;
         if ( oldPos > newPos ) {
            top = newCurrent;
            bottom = oldCurrent;
         } else {
            top = oldCurrent;
            bottom = newCurrent;
         }
         while( top )
         {
            if ( !top->isSelected() ) {
               top->setSelected( true, true );
               selectionChanged = true;
            }
            if ( top == bottom )
               break;
            top = top->nextItem();
         }
         QIconView::setCurrentItem( newCurrent );
         callDefaultHandler = false;
      }
      if( e->state() & ShiftButton )
         callDefaultHandler = false;
   }
	
	if (selectionChanged)
		updateView(); // don't call triggerUpdate directly!
	
   if (callDefaultHandler)
   {
     dragStartPos = QPoint( -1, -1 );

     QString name = QString::null;    // will the file be deleted by the mouse event?
     if( newCurrent )                 // save the name of the file
       name = static_cast<KrBriefViewItem*>( newCurrent ) ->name();

     KIconView::contentsMousePressEvent( e );

     if( name.isEmpty() || _dict.find( name ) == 0 ) // is the file still valid?
       newCurrent = 0;                // if not, don't do any crash...
   } else {
     // emitting the missing signals from QIconView::contentsMousePressEvent();
     // the right click signal is not emitted as it is used for selection

     QPoint vp = contentsToViewport( e->pos() );

     if( !newCurrent ) {
       emit pressed( pressedItem = newCurrent );
       emit pressed( newCurrent, viewport()->mapToGlobal( vp ) );
     }

     emit mouseButtonPressed( e->button(), newCurrent, viewport()->mapToGlobal( vp ) );
   }

   //   if (i != 0) // comment in, if click sould NOT select
   //     setSelected(i, FALSE);
   if (newCurrent) QIconView::setCurrentItem(newCurrent);

   if ( ACTIVE_PANEL->quickSearch->isShown() ) {
      ACTIVE_PANEL->quickSearch->hide();
      ACTIVE_PANEL->quickSearch->clear();
      krDirUp->setEnabled( true );
   }
   if ( OTHER_PANEL->quickSearch->isShown() ) {
      OTHER_PANEL->quickSearch->hide();
      OTHER_PANEL->quickSearch->clear();
      krDirUp->setEnabled( true );
   }
}

void KrBriefView::contentsMouseReleaseEvent( QMouseEvent * e ) {
  if (e->button() == RightButton)
    contextMenuTimer.stop();
  KIconView::contentsMouseReleaseEvent( e );

  if( pressedItem ) {
    QPoint vp = contentsToViewport( e->pos() );
    QIconViewItem *newCurrent = findItem( e->pos() );

    if( pressedItem == newCurrent ) {
      // emitting the missing signals from QIconView::contentsMouseReleaseEvent();
      // the right click signal is not emitted as it is used for selection

      if( !newCurrent ) {
        emit clicked( newCurrent );
        emit clicked( newCurrent, viewport()->mapToGlobal( vp ) );
      }

      emit mouseButtonClicked( e->button(), newCurrent, viewport()->mapToGlobal( vp ) );
    }

    pressedItem = 0;
  }
}

void KrBriefView::contentsMouseMoveEvent ( QMouseEvent * e ) {
   if ( ( singleClicked || renameTimer.isActive() ) && findItem( e->pos() ) != clickedItem )
      CANCEL_TWO_CLICK_RENAME;
   if ( dragStartPos != QPoint( -1, -1 ) &&
        e->state() & LeftButton && ( dragStartPos - e->pos() ).manhattanLength() > QApplication::startDragDistance() )
      startDrag();
   if (KrSelectionMode::getSelectionHandler()->rightButtonPreservesSelection() 
      && KrSelectionMode::getSelectionHandler()->rightButtonSelects() 
      && KrSelectionMode::getSelectionHandler()->showContextMenu() >= 0 && e->state() == Qt::RightButton)
      {
         QIconViewItem *newItem = findItem( e->pos() );
         e->accept();
         if (newItem != lastSwushPosition && newItem)
         {
           // is the new item above or below the previous one?
           QIconViewItem * above = newItem;
           QIconViewItem * below = newItem;
           for (;(above || below) && above != lastSwushPosition && below != lastSwushPosition;)
           {
             if (above)
               above = above->nextItem();
             if (below)
               below = below->prevItem();
           }
           if (above && (above == lastSwushPosition))
           {
             for (; above != newItem; above = above->prevItem())
               above->setSelected(swushSelects,true);
             newItem->setSelected(swushSelects,true);
             lastSwushPosition = newItem;
             updateView();
           }
           else if (below && (below == lastSwushPosition))
           {
             for (; below != newItem; below = below->nextItem())
               below->setSelected(swushSelects,true);
             newItem->setSelected(swushSelects,true);
             lastSwushPosition = newItem;
             updateView();
           }
           contextMenuTimer.stop();
         }
         // emitting the missing signals from QIconView::contentsMouseMoveEvent();
         if( newItem )
           emit onItem( newItem );
         else
           emit onViewport();
      }
      else
         KIconView::contentsMouseMoveEvent( e );
}

void KrBriefView::handleContextMenu( QIconViewItem * it, const QPoint & pos ) {
   if ( !_focused )
      op()->emitNeedFocus();
   setFocus();

   if ( !it )
      return ;
   if ( static_cast<KrBriefViewItem*>( it ) ->
         name() == ".." )
      return ;
   int i = KrSelectionMode::getSelectionHandler()->showContextMenu();
   contextMenuPoint = QPoint( pos.x(), pos.y() );
   if (i < 0)
     showContextMenu();
   else if (i > 0)
     contextMenuTimer.start(i, true);
}

void KrBriefView::showContextMenu()
{
	if (lastSwushPosition)
		lastSwushPosition->setSelected(true);
	op()->emitContextMenu( contextMenuPoint );
}

bool KrBriefView::acceptDrag( QDropEvent* ) const {
   return true;
}

void KrBriefView::contentsDropEvent( QDropEvent * e ) {
   _currDragItem = 0;
   op()->emitGotDrop(e);
   e->ignore();
   KIconView::contentsDropEvent( e );                   
}

void KrBriefView::contentsDragMoveEvent( QDragMoveEvent * e ) {
   _currDragItem = getKrViewItemAt( e->pos() );
   if( _currDragItem && !_currDragItem->VF->vfile_isDir() )
     _currDragItem = 0;
   
   KIconView::contentsDragMoveEvent( e );
}

void KrBriefView::imStartEvent(QIMEvent* e)
{
  if ( ACTIVE_PANEL->quickSearch->isShown() ) {
    ACTIVE_PANEL->quickSearch->myIMStartEvent( e );
    return ;
  }else {
    KConfigGroupSaver grpSvr( _config, "Look&Feel" );
    if ( !_config->readBoolEntry( "New Style Quicksearch", _NewStyleQuicksearch ) )
      KIconView::imStartEvent( e );
    else {
							// first, show the quicksearch if its hidden
      if ( ACTIVE_PANEL->quickSearch->isHidden() ) {
        ACTIVE_PANEL->quickSearch->show();
								// hack: if the pressed key requires a scroll down, the selected
								// item is "below" the quick search window, as the icon view will
								// realize its new size after the key processing. The following line
								// will resize the icon view immediately.
        ACTIVE_PANEL->layout->activate();
								// second, we need to disable the dirup action - hack!
        krDirUp->setEnabled( false );
      }
							// now, send the key to the quicksearch
      ACTIVE_PANEL->quickSearch->myIMStartEvent( e );
    }
  }
}

void KrBriefView::imEndEvent(QIMEvent* e)
{
  if ( ACTIVE_PANEL->quickSearch->isShown() ) {
    ACTIVE_PANEL->quickSearch->myIMEndEvent( e );
    return ;
  }
}

void KrBriefView::imComposeEvent(QIMEvent* e)
{
  if ( ACTIVE_PANEL->quickSearch->isShown() ) {
    ACTIVE_PANEL->quickSearch->myIMComposeEvent( e );
    return ;
  }
}

void KrBriefView::keyPressEvent( QKeyEvent * e ) {
   if ( !e || !firstItem() )
      return ; // subclass bug
   if ( ACTIVE_PANEL->quickSearch->isShown() ) {
      ACTIVE_PANEL->quickSearch->myKeyPressEvent( e );
      return ;
   }
   switch ( e->key() ) {
         case Key_Up :
         if ( e->state() == ControlButton ) { // let the panel handle it - jump to the Location Bar
            e->ignore();
            break;
         } else if (!KrSelectionMode::getSelectionHandler()->useQTSelection()) {
            QIconViewItem * i = currentItem();
            if ( !i ) break;
            if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
            i = i->prevItem();
         	if ( i ) {
					QIconView::setCurrentItem( i );
					QIconView::ensureItemVisible( i );
				}
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Down :
         if ( e->state() == ControlButton || e->state() == ( ControlButton | ShiftButton ) ) { // let the panel handle it - jump to command line
            e->ignore();
            break;
         } else if (!KrSelectionMode::getSelectionHandler()->useQTSelection()){
            QIconViewItem * i = currentItem();
            if ( !i ) break;
            if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
            i = i->nextItem();
         if ( i ) {QIconView::setCurrentItem( i ); QIconView::ensureItemVisible( i ); }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Next:  if (!KrSelectionMode::getSelectionHandler()->useQTSelection()){
            QIconViewItem * i = currentItem(), *j;
            if ( !i ) break;
            QRect r( i->rect() );
            if ( !r.height() ) break;
            for ( int page = visibleHeight() / r.height() - 1; page > 0 && ( j = i->nextItem() ); --page )
               i = j;
            if ( i ) {QIconView::setCurrentItem( i ); QIconView::ensureItemVisible( i ); }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Prior:  if (!KrSelectionMode::getSelectionHandler()->useQTSelection()){
            QIconViewItem * i = currentItem(), *j;
            if ( !i ) break;
            QRect r( i->rect() );
            if ( !r.height() ) break;
            for ( int page = visibleHeight() / r.height() - 1; page > 0 && ( j = i->prevItem() ); --page )
               i = j;
            if ( i ) {QIconView::setCurrentItem( i ); QIconView::ensureItemVisible( i ); }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Home:  if (!KrSelectionMode::getSelectionHandler()->useQTSelection()){
            if ( e->state() & ShiftButton )  /* Shift+Home */
            {
               clearSelection();
               KIconView::keyPressEvent( e );
               op()->emitSelectionChanged();
               arrangeItemsInGrid();
               break;
            } else {
               QIconViewItem * i = firstItem();
               if ( i ) {QIconView::setCurrentItem( i ); QIconView::ensureItemVisible( i ); }
            }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_End:  if (!KrSelectionMode::getSelectionHandler()->useQTSelection()){
            if ( e->state() & ShiftButton )  /* Shift+End */
            {
               clearSelection();
               KIconView::keyPressEvent( e );
               op()->emitSelectionChanged();
               arrangeItemsInGrid();
               break;
            } else {
               QIconViewItem *i = firstItem(), *j;
               while ( ( j = i->nextItem() ) )
                  i = j;
               while ( ( j = i->nextItem() ) )
                  i = j;
            if ( i ) {QIconView::setCurrentItem( i ); QIconView::ensureItemVisible( i ); }
               break;
            }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Enter :
         case Key_Return : {
            if ( e->state() & ControlButton )         // let the panel handle it
               e->ignore();
            else {
               KrViewItem * i = getCurrentKrViewItem();
               QString tmp = i->name();
               op()->emitExecuted(tmp);
            }
            break;
         }
         case Key_QuoteLeft :          // Terminal Emulator bugfix
         if ( e->state() == ControlButton ) { // let the panel handle it
            e->ignore();
            break;
         } else {          // a normal click - do a lynx-like moving thing
            SLOTS->home(); // ask krusader to move up a directory
            return ;         // safety
         }
         break;
         case Key_Right :
         if ( e->state() == ControlButton ) { // let the panel handle it
            e->ignore();
            break;
         } else if (!KrSelectionMode::getSelectionHandler()->useQTSelection()) {
            QIconViewItem *i = currentItem();
            QIconViewItem *newCurrent = 0;

            if ( !i ) break;

            int minY = i->y() - i->height() / 2;
            int minX  = i->width() / 2 + i->x();

            if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );

            while( i && i->x() <= minX )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
              i = i->nextItem();
            }

            while( i && i->y() < minY )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
              i = i->nextItem();
            }

            if( i )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
            }

            if( newCurrent )
            {
              QIconView::setCurrentItem( newCurrent );
              QIconView::ensureItemVisible( newCurrent );
            }
         } else KIconView::keyPressEvent(e);
         break;
         case Key_Backspace :                         // Terminal Emulator bugfix
         if ( e->state() == ControlButton || e->state() == ShiftButton ) { // let the panel handle it
            e->ignore();
            break;
         } else {          // a normal click - do a lynx-like moving thing
            SLOTS->dirUp(); // ask krusader to move up a directory
            return ;         // safety
         }
         case Key_Left :
         if ( e->state() == ControlButton ) { // let the panel handle it
            e->ignore();
            break;
         } else if (!KrSelectionMode::getSelectionHandler()->useQTSelection()) {
            QIconViewItem *i = currentItem();
            QIconViewItem *newCurrent = 0;

            if ( !i ) break;

            int maxY = i->y() + i->height() / 2;
            int maxX  = i->x() - i->width() / 2;

            if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );

            while( i && i->x() >= maxX )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
              i = i->prevItem();
            }

            while( i && i->y() > maxY )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
              i = i->prevItem();
            }
            if( i )
            {
              if ( e->state() == ShiftButton ) setSelected( i, !i->isSelected(), true );
              newCurrent = i;
            }

            if( newCurrent )
            {
              QIconView::setCurrentItem( newCurrent );
              QIconView::ensureItemVisible( newCurrent );
            }
         } else KIconView::keyPressEvent(e);
         break;

         case Key_Delete :                   // kill file
         SLOTS->deleteFiles( e->state() == ShiftButton || e->state() == ControlButton );
				
         break ;
         case Key_Insert : {
            {
               QIconViewItem *i = currentItem();
               if( !i )
                  break;

               if (KrSelectionMode::getSelectionHandler()->insertMovesDown())
               {
                  setSelected( i, !i->isSelected(), true );
                  if( i->nextItem() )
                  {
                     QIconView::setCurrentItem( i->nextItem() );
                     QIconView::ensureItemVisible( i->nextItem() );
                  }
               }
               else
               {
                  setSelected( i, !i->isSelected(), true );
               }
            }
            break ;
         }
         case Key_Space : {
            {
               QIconViewItem *i = currentItem();
               if( !i )
                  break;

               if (KrSelectionMode::getSelectionHandler()->spaceMovesDown())
               {
                  setSelected( i, !i->isSelected(), true );
                  if( i->nextItem() )
                  {
                     QIconView::setCurrentItem( i->nextItem() );
                     QIconView::ensureItemVisible( i->nextItem() );
                  }
               }
               else
               {
                  setSelected( i, !i->isSelected(), true );
               }
            }
            break ;
         }
         case Key_A :                 // mark all
         if ( e->state() == ControlButton ) {
            KIconView::keyPressEvent( e );
            updateView();
            break;
         }
         default:
         if ( e->key() == Key_Escape ) {
            QIconView::keyPressEvent( e ); return ; // otherwise the selection gets lost??!??
         }
         // if the key is A..Z or 1..0 do quick search otherwise...
         if ( e->text().length() > 0 && e->text() [ 0 ].isPrint() )       // better choice. Otherwise non-ascii characters like  can not be the first character of a filename
            /*         if ( ( e->key() >= Key_A && e->key() <= Key_Z ) ||
                           ( e->key() >= Key_0 && e->key() <= Key_9 ) ||
                           ( e->key() == Key_Backspace ) ||
                           ( e->key() == Key_Down ) ||
                           ( e->key() == Key_Period ) ) */{ 
            // are we doing quicksearch? if not, send keys to panel
            //if ( _config->readBoolEntry( "Do Quicksearch", _DoQuicksearch ) ) {
               // are we using krusader's classic quicksearch, or wincmd style?
               {
						KConfigGroupSaver grpSvr( _config, "Look&Feel" );
						if ( !_config->readBoolEntry( "New Style Quicksearch", _NewStyleQuicksearch ) )
							KIconView::keyPressEvent( e );
						else {
							// first, show the quicksearch if its hidden
							if ( ACTIVE_PANEL->quickSearch->isHidden() ) {
								ACTIVE_PANEL->quickSearch->show();
								// hack: if the pressed key requires a scroll down, the selected
								// item is "below" the quick search window, as the icon view will
								// realize its new size after the key processing. The following line
								// will resize the icon view immediately.
								ACTIVE_PANEL->layout->activate();
								// second, we need to disable the dirup action - hack!
								krDirUp->setEnabled( false );
							}
							// now, send the key to the quicksearch
							ACTIVE_PANEL->quickSearch->myKeyPressEvent( e );
						}
					}
            //} else
            //   e->ignore(); // send to panel
         } else {
            if ( ACTIVE_PANEL->quickSearch->isShown() ) {
               ACTIVE_PANEL->quickSearch->hide();
               ACTIVE_PANEL->quickSearch->clear();
               krDirUp->setEnabled( true );
            }
            KIconView::keyPressEvent( e );
         }
   }
   // emit the new item description
   slotItemDescription( currentItem() ); // actually send the QIconViewItem
}
// overridden to make sure EXTENTION won't be lost during rename
void KrBriefView::rename( QIconViewItem * item ) {
   currentlyRenamedItem = dynamic_cast< KrBriefViewItem * >( item );
   currentlyRenamedItem->rename();
   //TODO: renameLineEdit() ->selectAll();
}

void KrBriefView::renameCurrentItem() {
   QString newName, fileName;

	// handle inplace renaming, if possible
	
   KrBriefViewItem *it = static_cast<KrBriefViewItem*>(getCurrentKrViewItem());
   if ( it )
      fileName = it->name();
   else
      return ; // quit if no current item available
   // don't allow anyone to rename ..
   if ( fileName == ".." )
      return ;

   rename( static_cast<QIconViewItem*>( it ) );
   // if applicable, select only the name without extension
/* TODO:
   KConfigGroupSaver svr(krConfig,"Look&Feel");
   if (!krConfig->readBoolEntry("Rename Selects Extension", true)) {
     if (it->hasExtension() && !it->VF->vfile_isDir() ) 
       renameLineEdit()->setSelection(0, it->name().findRev(it->extension())-1);
   }*/
}

void KrBriefView::inplaceRenameFinished( QIconViewItem * it ) {
   if ( !it ) { // major failure - call developers
      krOut << "Major failure at inplaceRenameFinished(): item is null" << endl;
      return;
   }

   KrBriefViewItem *item = dynamic_cast<KrBriefViewItem *>( it );
   if( item->text() != item->name() )
      op()->emitRenameItem( item->name(), item->text() );

   currentlyRenamedItem = 0;
   setFocus();
}

// TODO: move the whole quicksearch mess out of here and into krview
void KrBriefView::quickSearch( const QString & str, int direction ) {
   KrViewItem * item = getCurrentKrViewItem();
   if (!item)
      return;
   KConfigGroupSaver grpSvr( _config, "Look&Feel" );
   bool caseSensitive = _config->readBoolEntry( "Case Sensitive Quicksearch", _CaseSensitiveQuicksearch );
   if ( !direction ) {
      if ( caseSensitive ? item->name().startsWith( str ) : item->name().lower().startsWith( str.lower() ) )
         return ;
      direction = 1;
   }
   KrViewItem * startItem = item;
   while ( true ) {
      item = ( direction > 0 ) ? getNext( item ) : getPrev( item );
      if ( !item )
         item = ( direction > 0 ) ? getFirst() : getLast();
      if ( item == startItem )
         return ;
      if ( caseSensitive ? item->name().startsWith( str ) : item->name().lower().startsWith( str.lower() ) ) {
			setCurrentItem( item->name() );
			makeItemVisible( item );
         return ;
      }
   }
}

void KrBriefView::stopQuickSearch( QKeyEvent * e ) {
   if( ACTIVE_PANEL && ACTIVE_PANEL->quickSearch ) {
     ACTIVE_PANEL->quickSearch->hide();
     ACTIVE_PANEL->quickSearch->clear();
     krDirUp->setEnabled( true );
     if ( e )
        keyPressEvent( e );
   }
}

void KrBriefView::setNameToMakeCurrent( QIconViewItem * it ) {
	if (!it) return;
   KrView::setNameToMakeCurrent( static_cast<KrBriefViewItem*>( it ) ->name() );
}

void KrBriefView::slotMouseClicked( int button, QIconViewItem * item, const QPoint& ) {
   pressedItem = 0; // if the signals are emitted, don't emit twice at contentsMouseReleaseEvent
   if ( button == Qt::MidButton )
      emit middleButtonClicked( dynamic_cast<KrViewItem *>( item ) );
}

bool KrBriefView::event( QEvent *e ) {
   modifierPressed = false;

   switch ( e->type() ) {
         case QEvent::Timer:
         case QEvent::MouseMove:
         case QEvent::MouseButtonPress:
         case QEvent::MouseButtonRelease:
         break;
         default:
         CANCEL_TWO_CLICK_RENAME;
   }
   if( e->type() == QEvent::Wheel )
   {
      if ( !_focused )
         op()->emitNeedFocus();
      setFocus();
   }
   return KIconView::event( e );
}
