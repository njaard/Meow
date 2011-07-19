#ifndef MEOW_DIRECTORYADDER_H
#define MEOW_DIRECTORYADDER_H

#include <qobject.h>

#ifdef MEOW_WITH_KDE
#include <kurl.h>
#include <kio/job.h>
#else
#include <qdiriterator.h>
#include <qurl.h>
#include <memory>
#endif

namespace Meow
{

/**
 * Adds a directory to
 * emits @ref done() when finished so you
 * can delete it
 **/
class DirectoryAdder : public QObject
{
	Q_OBJECT
#ifdef MEOW_WITH_KDE
	KUrl::List pendingAddDirectories;
	KIO::ListJob *listJob;
	KUrl currentJobUrl;
#else
	QList<QUrl> pendingAddDirectories;
	std::auto_ptr<QDirIterator> currentIterator;
	bool busy;
#endif
public:
	DirectoryAdder(QObject *parent);
	
public slots:
#ifdef MEOW_WITH_KDE
	void add(const KUrl &dir);
#else
	void add(const QUrl &dir);
#endif

signals:
	void done();
#ifdef MEOW_WITH_KDE
	void addFile(const KUrl &file);
#else
	void addFile(const QUrl &file);
#endif

#ifdef MEOW_WITH_KDE
private slots:
	void slotResult(KJob *job);
	void slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries);
	void slotRedirection(KIO::Job *, const KUrl & url);
#else
private slots:
	void processMore();
#endif

private:
	void addNextPending();
};

}

#endif
// kate: space-indent off; replace-tabs off;
