#include "directoryadder.h"

#include <kfileitem.h>

KittenPlayer::DirectoryAdder::DirectoryAdder(const KUrl &dir, QObject *parent)
	: QObject(parent)
{
	listJob=0;
	add(dir);
}

void KittenPlayer::DirectoryAdder::add(const KUrl &dir)
{
	pendingAddDirectories.append(dir);
	addNextPending();
}

void KittenPlayer::DirectoryAdder::addNextPending()
{
	KUrl::List::Iterator pendingIt= pendingAddDirectories.begin();
	if (!listJob && (pendingIt!= pendingAddDirectories.end()))
	{
		currentJobUrl= *pendingIt;
		listJob = KIO::listRecursive(currentJobUrl, KIO::HideProgressInfo, false);
		connect(
				listJob, SIGNAL(entries(KIO::Job*, const KIO::UDSEntryList&)),
				SLOT(slotEntries(KIO::Job*, const KIO::UDSEntryList&))
			);
		connect(
				listJob, SIGNAL(result(KIO::Job*)),
				SLOT(slotResult(KIO::Job*))
			);
		connect(
				listJob, SIGNAL(redirection(KIO::Job *, const KUrl &)),
				SLOT(slotRedirection(KIO::Job *, const KUrl &))
			);
		pendingAddDirectories.erase(pendingIt);
	}
}

void KittenPlayer::DirectoryAdder::slotResult(KIO::Job *job)
{
	listJob= 0;
	if (job && job->error())
		job->showErrorDialog();
	addNextPending();
	if (!listJob)
		emit done();
}

void KittenPlayer::DirectoryAdder::slotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
{
	for (KIO::UDSEntryList::ConstIterator it = entries.begin(); it != entries.end(); ++it)
	{
		KFileItem file(*it, currentJobUrl, false /* no mimetype detection */, true);
		emit addFile(file.url());
	}

}

void KittenPlayer::DirectoryAdder::slotRedirection(KIO::Job *, const KUrl & url)
{
	currentJobUrl= url;
}

// kate: space-indent off; replace-tabs off;
