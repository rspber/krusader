/***************************************************************************
                                 krsearchdialog.cpp
                             -------------------
    copyright            : (C) 2001 by Shie Erlich & Rafi Yanai
    email                : krusader@users.sourceforge.net
    web site		 : http://krusader.sourceforge.net
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

#include "../krusader.h"
#include "../panelmanager.h"
#include "../VFS/vfs.h"
#include "../krusaderview.h"
#include "../Panel/listpanel.h"
#include "../Panel/panelfunc.h"
#include "../Dialogs/krdialogs.h"
#include "../VFS/krpermhandler.h"
#include "../VFS/virt_vfs.h"
#include "../KViewer/krviewer.h"
#include "krsearchmod.h"
#include "krsearchdialog.h"

#include <time.h>
#include <kglobal.h>
#include <qregexp.h>
#include <qfontmetrics.h>
#include <kmessagebox.h>
#include <klocale.h>
#include <kpopupmenu.h>
#include <qcursor.h>
#include <qclipboard.h>
#include <qheader.h>

QString KrSearchDialog::lastSearchText = "";
int KrSearchDialog::lastSearchType = 0;
bool KrSearchDialog::lastSearchForCase = false;
bool KrSearchDialog::lastContainsWholeWord = false;
bool KrSearchDialog::lastContainsWithCase = true;
bool KrSearchDialog::lastSearchInSubDirs = true;
bool KrSearchDialog::lastSearchInArchives = false;
bool KrSearchDialog::lastFollowSymLinks = false;

// class starts here /////////////////////////////////////////
KrSearchDialog::KrSearchDialog( QString profile, QWidget* parent,  const char* name, bool modal, WFlags fl )
                : QDialog( parent, name, modal, fl ), query(0), searcher(0) 
{
  setCaption( i18n( "Krusader::Search" ) );
  
  QGridLayout* searchBaseLayout = new QGridLayout( this );
  searchBaseLayout->setSpacing( 6 );
  searchBaseLayout->setMargin( 11 );

  // creating the dialog buttons ( Search, Stop, Close )
  
  QHBoxLayout* buttonsLayout = new QHBoxLayout();
  buttonsLayout->setSpacing( 6 );
  buttonsLayout->setMargin( 0 );

  profileManager = new ProfileManager( "SearcherProfile", this, "profileManager" );
  buttonsLayout->addWidget( profileManager );
  
  QSpacerItem* spacer = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
  buttonsLayout->addItem( spacer );

  mainFeedToListBoxBtn = new QPushButton( this, "mainFeedToListBoxBtn" );
  mainFeedToListBoxBtn->setText( i18n( "Feed to listbox" ) );
  mainFeedToListBoxBtn->setEnabled(false);
  buttonsLayout->addWidget( mainFeedToListBoxBtn );
  
  mainSearchBtn = new QPushButton( this, "mainSearchBtn" );
  mainSearchBtn->setText( i18n( "Search" ) );
  mainSearchBtn->setDefault(true);
  buttonsLayout->addWidget( mainSearchBtn );

  mainStopBtn = new QPushButton( this, "mainStopBtn" );
  mainStopBtn->setEnabled( false );
  mainStopBtn->setText( i18n( "Stop" ) );
  buttonsLayout->addWidget( mainStopBtn );

  mainCloseBtn = new QPushButton( this, "mainCloseBtn" );
  mainCloseBtn->setText( i18n( "Close" ) );
  buttonsLayout->addWidget( mainCloseBtn );

  searchBaseLayout->addLayout( buttonsLayout, 1, 0 );

  // creating the searcher tabs
  
  searcherTabs = new QTabWidget( this, "searcherTabs" );

  generalFilter = new GeneralFilter( true, searcherTabs, "generalFilter" );
  searcherTabs->insertTab( generalFilter, i18n( "&General" ) );

  advancedFilter = new AdvancedFilter( searcherTabs, "advancedFilter" );
  searcherTabs->insertTab( advancedFilter, i18n( "&Advanced" ) );

  resultTab = new QWidget( searcherTabs, "resultTab" );
  resultLayout = new QGridLayout( resultTab );
  resultLayout->setSpacing( 6 );
  resultLayout->setMargin( 11 );

  // creating the result tab
    
  QHBoxLayout* resultLabelLayout = new QHBoxLayout();
  resultLabelLayout->setSpacing( 6 );
  resultLabelLayout->setMargin( 0 );

  foundLabel = new QLabel( resultTab, "foundLabel" );
  foundLabel->setSizePolicy( QSizePolicy( (QSizePolicy::SizeType)1, (QSizePolicy::SizeType)1, foundLabel->sizePolicy().hasHeightForWidth() ) );
  foundLabel->setFrameShape( QLabel::StyledPanel );
  foundLabel->setFrameShadow( QLabel::Sunken );
  foundLabel->setText( i18n( "Found 0 matches." ) );
  resultLabelLayout->addWidget( foundLabel );

  searchingLabel = new KSqueezedTextLabel( resultTab, "searchingLabel" );
  searchingLabel->setFrameShape( QLabel::StyledPanel );
  searchingLabel->setFrameShadow( QLabel::Sunken );
  searchingLabel->setText( "" );
  resultLabelLayout->addWidget( searchingLabel );

  resultLayout->addLayout( resultLabelLayout, 1, 0 );

  // creating the result list view

  resultsList = new QListView( resultTab, "resultsList" );
  resultsList->addColumn( i18n( "Name" ) );
  resultsList->addColumn( i18n( "Location" ) );
  resultsList->addColumn( i18n( "Size" ) );
  resultsList->addColumn( i18n( "Date" ) );
  resultsList->addColumn( i18n( "Permissions" ) );

  resultsList->setSorting(1); // sort by location
  
  // fix the results list
  // => make the results font smaller
  QFont resultsFont(  resultsList->font() );
  resultsFont.setPointSize(resultsFont.pointSize()-1);
  resultsList->setFont(resultsFont);

  resultsList->setAllColumnsShowFocus(true);
  for (int i=0; i<5; ++i) // don't let it resize automatically
    resultsList->setColumnWidthMode(i, QListView::Manual);
    
  int i=QFontMetrics(resultsList->font()).width("W");
  int j=QFontMetrics(resultsList->font()).width("0");
  j=(i>j ? i : j);
  
  resultsList->setColumnWidth(0, krConfig->readNumEntry("Name Width", j*14) );
  resultsList->setColumnWidth(1, krConfig->readNumEntry("Path Width", j*25) );
  resultsList->setColumnWidth(2, krConfig->readNumEntry("Size Width", j*6) );
  resultsList->setColumnWidth(3, krConfig->readNumEntry("Date Width", j*7) );
  resultsList->setColumnWidth(4, krConfig->readNumEntry("Perm Width", j*7) );
  resultsList->setColumnAlignment( 2, AlignRight );
  
  resultsList->header()->setStretchEnabled( true, 1 );
  
  resultLayout->addWidget( resultsList, 0, 0 );
  searcherTabs->insertTab( resultTab, i18n( "&Results" ) );

  searchBaseLayout->addWidget( searcherTabs, 0, 0 );

  // signals and slots connections
  
  connect( mainSearchBtn, SIGNAL( clicked() ), this, SLOT( startSearch() ) );
  connect( mainStopBtn, SIGNAL( clicked() ), this, SLOT( stopSearch() ) );
  connect( resultsList, SIGNAL( returnPressed(QListViewItem*) ), this, SLOT( resultClicked(QListViewItem*) ) );
  connect( resultsList, SIGNAL( doubleClicked(QListViewItem*) ), this, SLOT( resultClicked(QListViewItem*) ) );
  connect( resultsList, SIGNAL( rightButtonClicked(QListViewItem*,const QPoint&,int) ), this, SLOT( rightClickMenu(QListViewItem*, const QPoint&, int) ) );
  connect( mainCloseBtn, SIGNAL( clicked() ), this, SLOT( closeDialog() ) );
  connect( mainFeedToListBoxBtn, SIGNAL( clicked() ), this, SLOT( feedToListBox() ) );
  
  connect( profileManager, SIGNAL( loadFromProfile( QString ) ), generalFilter, SLOT( loadFromProfile( QString ) ) );
  connect( profileManager, SIGNAL( saveToProfile( QString ) ), generalFilter, SLOT( saveToProfile( QString ) ) );
  connect( profileManager, SIGNAL( loadFromProfile( QString ) ), advancedFilter, SLOT( loadFromProfile( QString ) ) );
  connect( profileManager, SIGNAL( saveToProfile( QString ) ), advancedFilter, SLOT( saveToProfile( QString ) ) );

  // tab order
  
  setTabOrder( mainSearchBtn, mainCloseBtn );
  setTabOrder( mainCloseBtn, mainStopBtn );
  setTabOrder( mainStopBtn, searcherTabs );
  setTabOrder( searcherTabs, resultsList );

  krConfig->setGroup( "Search" );
  int sx = krConfig->readNumEntry( "Window Width",  -1 );
  int sy = krConfig->readNumEntry( "Window Height",  -1 );
  
  if( sx != -1 && sy != -1 )
    resize( sx, sy );
  
  if( krConfig->readBoolEntry( "Window Maximized",  false ) )
      showMaximized();
  else  
      show();
  
  // disable the search action ... no 2 searchers !
  krFind->setEnabled(false);
  generalFilter->searchFor->setFocus();

  isSearching = closed = false;
  
  // finaly, load a profile of apply defaults:
  
  if ( profile.isEmpty() ) {
    // load the last used values
    generalFilter->searchFor->setEditText( lastSearchText );
    generalFilter->searchFor->lineEdit()->selectAll();
    generalFilter->ofType->setCurrentItem( lastSearchType );
    generalFilter->searchForCase->setChecked( lastSearchForCase );
    generalFilter->containsWholeWord->setChecked( lastContainsWholeWord );
    generalFilter->containsTextCase->setChecked( lastContainsWithCase );
    generalFilter->searchInDirs->setChecked( lastSearchInSubDirs );
    generalFilter->searchInArchives->setChecked( lastSearchInArchives );
    generalFilter->followLinks->setChecked( lastFollowSymLinks );
    generalFilter->searchIn->lineEdit()->setText( ACTIVE_PANEL->getPath() ); // the path in the active panel should be the default search location
  }
  else
    profileManager->loadByName( profile ); // important: call this _after_ you've connected profileManager ot the loadFromProfile!!

}

void KrSearchDialog::closeDialog( bool isAccept ) 
{
  // stop the search if it's on-going
  if (searcher!=0) {
    delete searcher;
    searcher = 0;
  }
  
  // saving the searcher state
  
  krConfig->setGroup( "Search" );
  
  krConfig->writeEntry("Window Width", sizeX );
  krConfig->writeEntry("Window Height", sizeY );
  krConfig->writeEntry("Window Maximized", isMaximized() );
  
  krConfig->writeEntry("Name Width",  resultsList->columnWidth( 0 ) );
  krConfig->writeEntry("Path Width",  resultsList->columnWidth( 1 ) );
  krConfig->writeEntry("Size Width",  resultsList->columnWidth( 2 ) );
  krConfig->writeEntry("Date Width",  resultsList->columnWidth( 3 ) );
  krConfig->writeEntry("Perm Width",  resultsList->columnWidth( 4 ) );
  
  lastSearchText = generalFilter->searchFor->currentText();
  lastSearchType = generalFilter->ofType->currentItem();
  lastSearchForCase = generalFilter->searchForCase->isChecked();
  lastContainsWholeWord = generalFilter->containsWholeWord->isChecked();
  lastContainsWithCase = generalFilter->containsTextCase->isChecked();
  lastSearchInSubDirs = generalFilter->searchInDirs->isChecked();
  lastSearchInArchives = generalFilter->searchInArchives->isChecked();
  lastFollowSymLinks = generalFilter->followLinks->isChecked();
  hide();  
  // re-enable the search action
  krFind->setEnabled(true);
  if( isAccept )
    QDialog::accept();
  else
    QDialog::reject();
  
  this->deleteLater();
}

void KrSearchDialog::reject() {
  closeDialog( false );
}

void KrSearchDialog::resizeEvent( QResizeEvent *e )
{   
  if( !isMaximized() )
  {
    sizeX = e->size().width();
    sizeY = e->size().height();
  }
}

void KrSearchDialog::found(QString what, QString where, KIO::filesize_t size, time_t mtime, QString perm){
  // convert the time_t to struct tm
  struct tm* t=localtime((time_t *)&mtime);
  QDateTime tmp(QDate(t->tm_year+1900, t->tm_mon+1, t->tm_mday), QTime(t->tm_hour, t->tm_min));
  new QListViewItem(resultsList, what, where.replace(QRegExp("\\\\"),"#"),
                    KRpermHandler::parseSize(size), KGlobal::locale()->formatDateTime(tmp), perm);
  QString totals = QString(i18n("Found %1 matches.")).arg(resultsList->childCount());
  foundLabel->setText(totals);
}

bool KrSearchDialog::gui2query() {
  // prepare the query ...
  /////////////////// names, locations and greps
  if (query!=0) { delete query; query = 0; }
  query = new KRQuery();

  if( !generalFilter->fillQuery( query ) )
  {
    searcherTabs->setCurrentPage(0); // set page to general
    return false;
  }
  
  if( !advancedFilter->fillQuery( query ) )
  {
    searcherTabs->setCurrentPage(1); // set page to general
    return false;
  }
  
  return true;
}

void KrSearchDialog::startSearch() {
  // first, informative messages
  if (generalFilter->searchInArchives->isChecked()) {
    KMessageBox::information(0, i18n("Since you chose to also search in archives, "
                                     "note the following limitations:\n"
                                     "You cannot search for text (grep) while doing"
                                     " a search that includes archives."), 0, "searchInArchives");
  }

  // prepare the query /////////////////////////////////////////////
  if (!gui2query()) return;
  // else implied
  generalFilter->queryAccepted();

  // prepare the gui ///////////////////////////////////////////////
  mainSearchBtn->setEnabled(false);
  mainCloseBtn->setEnabled(false);
  mainStopBtn->setEnabled(true);
  mainFeedToListBoxBtn->setEnabled(false);
  resultsList->clear(); 
  searchingLabel->setText("");
  foundLabel->setText(i18n("Found 0 matches."));
  searcherTabs->setCurrentPage(2); // show the results page
  qApp->processEvents();

  // start the search.
  if (searcher != 0) {
    delete searcher;
    searcher = 0;
  }
  searcher  = new KRSearchMod(query);
  connect(searcher, SIGNAL(searching(const QString&)),
          searchingLabel, SLOT(setText(const QString&)));
  connect(searcher, SIGNAL(found(QString,QString,KIO::filesize_t,time_t,QString)),
                this, SLOT(found(QString,QString,KIO::filesize_t,time_t,QString)));
  connect(searcher, SIGNAL(finished()), this, SLOT(stopSearch()));

  isSearching = true;
  searcher->start();
  isSearching = false;
  if( closed )
    emit closeDialog();
}

void KrSearchDialog::stopSearch() {
  if (searcher!=0) {
    searcher->stop();
    disconnect(searcher,0,0,0);
    delete query;
    query = 0;
  }

  // gui stuff
  mainSearchBtn->setEnabled(true);
  mainCloseBtn->setEnabled(true);
  mainStopBtn->setEnabled(false);
  if( resultsList->childCount() )
    mainFeedToListBoxBtn->setEnabled( true );
  searchingLabel->setText(i18n("Finished searching."));
}

void KrSearchDialog::resultClicked(QListViewItem* i) {
  ACTIVE_FUNC->openUrl(vfs::fromPathOrURL(i->text(1)),i->text(0));
  showMinimized();
}

void KrSearchDialog::closeEvent(QCloseEvent *e)
{                     /* if searching is in progress we must not close the window */
  if( isSearching )   /* because qApp->processEvents() is called by the searcher and */
  {                   /* at window desruction, the searcher object will be deleted */
    stopSearch();         /* instead we stop searching */
    closed = true;        /* and after stopping: startSearch can close the window */
    e->ignore();          /* ignoring the close event */
  }
  else
    QDialog::closeEvent( e );   /* if no searching, let QDialog handle the event */
}

void KrSearchDialog::keyPressEvent(QKeyEvent *e)
{
  if( isSearching && e->key() == Key_Escape ) /* at searching we must not close the window */
  {
    stopSearch();         /* so we simply stop searching */
    return;
  }
  if( resultsList->hasFocus() )
  {
    if( e->key() == Key_F4 )
    {
      if (!generalFilter->containsText->currentText().isEmpty() && QApplication::clipboard()->text() != generalFilter->containsText->currentText())
        QApplication::clipboard()->setText(generalFilter->containsText->currentText());
      editCurrent();
      return;
    }
    else if( e->key() == Key_F3 )
    {
      if (!generalFilter->containsText->currentText().isEmpty() && QApplication::clipboard()->text() != generalFilter->containsText->currentText())
        QApplication::clipboard()->setText(generalFilter->containsText->currentText());
      viewCurrent();
      return;
    }
  }

  QDialog::keyPressEvent( e );
}

void KrSearchDialog::editCurrent()
{
  QListViewItem *current = resultsList->currentItem();
  if( current )
  {
    QString name = current->text(1);
    name += (name.endsWith( "/" ) ? current->text(0) : "/" + current->text(0) );
    KURL url = vfs::fromPathOrURL( name );
    KrViewer::edit( url, true );
  }
}

void KrSearchDialog::viewCurrent()
{
  QListViewItem *current = resultsList->currentItem();
  if( current )
  {
    QString name = current->text(1);
    name += (name.endsWith( "/" ) ? current->text(0) : "/" + current->text(0) );
    KURL url = vfs::fromPathOrURL( name );
    KrViewer::view( url );
  }
}

void KrSearchDialog::rightClickMenu(QListViewItem *item, const QPoint&, int)
{
  // these are the values that will exist in the menu
  #define EDIT_FILE_ID        110
  #define VIEW_FILE_ID        111
  //////////////////////////////////////////////////////////
  if (!item)
    return;

  // create the menu
  KPopupMenu popup;
  popup.insertTitle(i18n("Krusader Search"));

  popup.insertItem(i18n("View File (F3)"),VIEW_FILE_ID);
  popup.insertItem(i18n("Edit File (F4)"),EDIT_FILE_ID);

  int result=popup.exec(QCursor::pos());

  // check out the user's option
  switch (result)
  {
    case VIEW_FILE_ID:
      viewCurrent();
      break;
    case EDIT_FILE_ID:
      editCurrent();
      break;
    default:    // the user clicked outside of the menu
      break;
  }
}

void KrSearchDialog::feedToListBox()
{
  static int listBoxNum = 1;
  
  KURL::List urlList;
  QListViewItem * item = resultsList->firstChild();
  while( item )
  {
    QString name = item->text(1);
    name += (name.endsWith( "/" ) ? item->text(0) : "/" + item->text(0) );
    urlList.push_back( vfs::fromPathOrURL( name ) );
    item = item->nextSibling();
  }
  KURL url = KURL::fromPathOrURL(QString("virt:/")+ i18n("Search results")+QString( " %1" ).arg( listBoxNum++ ));
  virt_vfs v(0,true);
  v.vfs_refresh( url );
  v.vfs_addFiles( &urlList, KIO::CopyJob::Copy, 0 );
  //ACTIVE_FUNC->openUrl(url);  
  ACTIVE_MNG->slotNewTab(url.prettyURL());
  closeDialog();
}

#include "krsearchdialog.moc"
