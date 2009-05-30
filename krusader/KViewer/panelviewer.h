#ifndef _SUPERVIEW_H
#define _SUPERVIEW_H

#include <kurl.h>
#include <QtCore/QString>
#include <qstackedwidget.h>
#include <kparts/part.h>
#include <kio/job.h>
#include <QtCore/QHash>
#include <QtGui/QLabel>

#include "krviewer.h"

class KTemporaryFile;

class PanelViewerBase: public QStackedWidget {
	Q_OBJECT

public:
	PanelViewerBase( QWidget *parent = 0 );
	virtual ~PanelViewerBase();
	inline KUrl url() const { return curl; }
	inline void setUrl( KUrl url ) { emit urlChanged( this, url ); curl = url; }
	inline KParts::ReadOnlyPart* part() const { return cpart; }
	virtual bool isModified() { return false; }
	virtual bool isEditor() = 0;

public slots:
	virtual KParts::ReadOnlyPart* openUrl( const KUrl&, KrViewer::Mode=KrViewer::Generic ){ return 0;} 
	virtual bool closeUrl(){ return false; }
	virtual bool queryClose() { return true; }

signals:
	void openUrlRequest( const KUrl &url );
	void urlChanged( PanelViewerBase *, const KUrl & );
	void partDestroyed( PanelViewerBase * );

protected slots:
	void slotCPartDestroyed() { emit partDestroyed( this ); }

protected:
	QHash<QString, QPointer<KParts::ReadOnlyPart> > *mimes;
	QPointer<KParts::ReadOnlyPart> cpart;

	QString cmimetype;
	KUrl curl;
	QLabel *fallback;

};

class PanelViewer: public PanelViewerBase {
	Q_OBJECT
public slots:
	KParts::ReadOnlyPart* openUrl( const KUrl &url, KrViewer::Mode mode=KrViewer::Generic );
	bool closeUrl();

public:
	PanelViewer( QWidget *parent = 0 );
	~PanelViewer();

	virtual bool isEditor() { return false; }

protected:
	KParts::ReadOnlyPart *getPart( QString mimetype );
	KParts::ReadOnlyPart*  getHexPart();
	void oldHexViewer(KTemporaryFile& tmpFile);
};

class PanelEditor: public PanelViewerBase {
	Q_OBJECT
public:
	virtual bool isModified();
	virtual bool isEditor() { return true; }

public slots:
	KParts::ReadOnlyPart* openUrl( const KUrl &url, KrViewer::Mode mode=KrViewer::Generic );
	bool closeUrl();
	bool queryClose();
	void slotStatResult( KJob* job );

public:
	PanelEditor( QWidget *parent = 0 );
	~PanelEditor();

protected:
	KParts::ReadWritePart* getPart( QString mimetype );

	bool busy;
	KIO::UDSEntry entry;
};

#endif
