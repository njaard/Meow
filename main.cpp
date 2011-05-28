#ifdef MEOW_WITH_KDE
#include "mainwindow.h"

#include <kaboutdata.h>
#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>

int main( int argc, char **argv )
{
	KAboutData aboutData(
			"meow", 0, ki18n( "Meow" ),
			"1.0", ki18n("Cutest Music Player Ever"), KAboutData::License_BSD,
			ki18n("(c) 2008, Charles Samuels")
		);
	
	aboutData.addAuthor( ki18n("Charles Samuels"), ki18n("Developer"), "charles@kde.org" );
	aboutData.addAuthor( ki18n("Allan Sandfeld Jensen"), ki18n("aKode"));
	
	KCmdLineArgs::init( argc, argv, &aboutData );
	
	KUniqueApplication app;
	
	Meow::MainWindow *dlg = new Meow::MainWindow;
	dlg->show();
	
	return app.exec();
}

#else
#include "mainwindow-qt.h"

#include <qapplication.h>

int main( int argc, char **argv )
{
	QApplication app(argc, argv);
	
	Meow::MainWindow *dlg = new Meow::MainWindow;
	dlg->show();
	
	app.setQuitOnLastWindowClosed(true);
	return app.exec();
}

#endif

// kate: space-indent off; replace-tabs off;
