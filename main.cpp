#ifdef MEOW_WITH_KDE
#include "mainwindow.h"

#include <kaboutdata.h>
#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>

#include <vector>

int main( int argc, char **argv )
{
	KAboutData aboutData(
			"meow", 0, ki18n( "Meow" ),
			MEOW_VERSION, ki18n("A cute music player"), KAboutData::License_GPL_V3,
			ki18n("(c) 2008-2013, Charles Samuels. (c) 1994-2010 others")
		);
	
	aboutData.addAuthor( ki18n("Charles Samuels"), ki18n("Developer"), "charles@meowplayer.org" );
	aboutData.addAuthor( ki18n("Allan Sandfeld Jensen"), ki18n("Most of Akode backend"));
	aboutData.addAuthor( ki18n("Stefan Gehn, Charles Samuels"), ki18n("Portions of playback controller"));
	aboutData.setBugAddress( "bugs@meowplayer.org");
	aboutData.setProgramIconName( "speaker" );

	aboutData.addCredit( ki18n("Josh Coalson"), ki18n("FLAC decoder"));
	aboutData.addCredit( ki18n("the Xiph.Org Foundation"), ki18n("Vorbis decoder, Opus decoder"));
	aboutData.addCredit( ki18n("Michael Hipp and the mpg123 project"), ki18n("mp3 decoder"));
	aboutData.addCredit( ki18n("The Musepack Development Team"), ki18n("Musepack decoder"));
	aboutData.addCredit( ki18n("Peter Pawlowski"), ki18n("Musepack decoder"));
	aboutData.addCredit( ki18n("Ross P. Johnson"), ki18n("Posix threads library for Windows"));
	aboutData.addCredit( ki18n("The Public Domain"), ki18n("SQLite"));
	aboutData.addCredit( ki18n("Kittens Everywhere"));
	
	KCmdLineArgs::init( argc, argv, &aboutData );
	KCmdLineOptions options;
	options.add("+[file]", ki18n("Media files to load and play"));
	KCmdLineArgs::addCmdLineOptions(options);

	KUniqueApplication app;
	
	KCmdLineArgs *const args = KCmdLineArgs::parsedArgs();
	std::vector<KUrl> files;
	files.reserve(args->count());
	for (int i=0; i < args->count(); i++)
		files.push_back(args->url(i));

	Meow::MainWindow *dlg = new Meow::MainWindow(files.size());
	for (unsigned i=0; i < files.size(); ++i)
	{
		if (i == 0)
			dlg->addAndPlayFile(files[i]);
		else
			dlg->addFile(files[i]);
	}

	dlg->show();
	
	return app.exec();
}

#else
#include "mainwindow-qt.h"

#include <qapplication.h>
#include <qurl.h>
#include <qfile.h>

#include <vector>

int main( int argc, char **argv )
{
	QApplication app(argc, argv);
	app.setWindowIcon(QIcon(":/meow.png"));
	QCoreApplication::setOrganizationName("meowplayer.org");
	QCoreApplication::setOrganizationDomain("meowplayer.org");
	QCoreApplication::setApplicationName("Meow");

	Q_INIT_RESOURCE(icons);
	bool dashHandling=true;
	std::vector<QUrl> files;
	files.reserve(argc-1);
	for (int i=1; i < argc; i++)
	{
		if (dashHandling && argv[i][0] == '-')
		{
			if (argv[i][1])
				dashHandling = false;
			continue;
		}
		files.push_back(QUrl::fromLocalFile(QFile::decodeName(argv[i])));
	}
	
	Meow::MainWindow *dlg = new Meow::MainWindow(!files.empty());
	for (unsigned i=0; i < files.size(); ++i)
	{
		if (i == 0)
			dlg->addAndPlayFile(files[i]);
		else
			dlg->addFile(files[i]);
	}
	
	dlg->show();
	
	app.setQuitOnLastWindowClosed(true);
	return app.exec();
}

#endif

// kate: space-indent off; replace-tabs off;
