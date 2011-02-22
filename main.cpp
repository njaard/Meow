#include <kaboutdata.h>
#include <kuniqueapplication.h>
#include <kcmdlineargs.h>
#include <klocale.h>

#include "mainwindow.h"

int main( int argc, char *argv[] )
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

// kate: space-indent off; replace-tabs off;
