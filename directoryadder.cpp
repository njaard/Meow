#include "directoryadder.h"

#include <qtimer.h>
#include <qfileinfo.h>

#ifdef MEOW_WITH_KDE
#include <kfileitem.h>

Meow::DirectoryAdder::DirectoryAdder(QObject *parent)
	: QObject(parent)
{
	listJob=0;
}

void Meow::DirectoryAdder::add(const KUrl &dir)
{
	if (QFileInfo(dir.path()).isFile())
	{
		emit addFile(dir);
		emit done();
	}
	else
	{
		pendingAddDirectories.append(dir);
		addNextPending();
	}
}

void Meow::DirectoryAdder::addNextPending()
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
				listJob, SIGNAL(result(KJob*)),
				SLOT(slotResult(KJob*))
			);
		connect(
				listJob, SIGNAL(redirection(KIO::Job *, const KUrl &)),
				SLOT(slotRedirection(KIO::Job *, const KUrl &))
			);
		pendingAddDirectories.erase(pendingIt);
	}
}

void Meow::DirectoryAdder::slotResult(KJob *job)
{
	listJob= 0;
	addNextPending();
	if (!listJob)
		emit done();
}

void Meow::DirectoryAdder::slotEntries(KIO::Job *, const KIO::UDSEntryList &entries)
{
	for (KIO::UDSEntryList::ConstIterator it = entries.begin(); it != entries.end(); ++it)
	{
		KFileItem file(*it, currentJobUrl, false /* no mimetype detection */, true);
		emit addFile(file.url());
	}

}

void Meow::DirectoryAdder::slotRedirection(KIO::Job *, const KUrl & url)
{
	currentJobUrl= url;
}

#else

Meow::DirectoryAdder::DirectoryAdder(QObject *parent)
	: QObject(parent)
{
}

void Meow::DirectoryAdder::add(const QUrl &dir)
{
	if (QFileInfo(dir.path()).isFile())
	{
		emit addFile(dir);
		emit done();
	}
	else
	{
		pendingAddDirectories.append(dir);
		addNextPending();
	}
}

void Meow::DirectoryAdder::addNextPending()
{
	if (!pendingAddDirectories.isEmpty() && !busy)
	{
		currentIterator.reset(new QDirIterator(
				QDir(pendingAddDirectories.takeFirst().toLocalFile()),
				QDirIterator::Subdirectories
			));
		busy=true;
	}
	QTimer::singleShot(100, this, SLOT(processMore()));
}

void Meow::DirectoryAdder::processMore()
{
	int c=10;
	while (currentIterator->hasNext() && c--)
	{
		currentIterator->next();
		emit addFile(currentIterator->filePath());
	}
	
	if (currentIterator->hasNext())
		QTimer::singleShot(100, this, SLOT(processMore()));
	else
	{
		busy = false;
		emit done();
	}
}

#endif

// kate: space-indent off; replace-tabs off;
