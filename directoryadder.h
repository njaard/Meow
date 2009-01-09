#ifndef MEOW_DIRECTORYADDER_H
#define MEOW_DIRECTORYADDER_H

#include <qobject.h>

#include <kurl.h>
#include <kio/job.h>

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
	KUrl::List pendingAddDirectories;
	KIO::ListJob *listJob;
	KUrl currentJobUrl;

public:
	DirectoryAdder(const KUrl &dir, QObject *parent);
	
public slots:
	void add(const KUrl &dir);

signals:
	void done();
	void addFile(const KUrl &file);

private slots:
	void slotResult(KJob *job);
	void slotEntries(KIO::Job *job, const KIO::UDSEntryList &entries);
	void slotRedirection(KIO::Job *, const KUrl & url);

private:
	void addNextPending();
};

}

#endif
// kate: space-indent off; replace-tabs off;
