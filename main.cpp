#include <kaboutdata.h>
#include <kapplication.h>
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
    aboutData.addAuthor( ki18n("Charles Samuels"), KLocalizedString(), "charles@kde.org" );
    
    KCmdLineArgs::init( argc, argv, &aboutData );
    
    KCmdLineOptions options;
    KCmdLineArgs::addCmdLineOptions( options );
    KCmdLineArgs *const args = KCmdLineArgs::parsedArgs();
    
    KApplication app;
    
    Meow::MainWindow *dlg = new Meow::MainWindow;
    dlg->show();
    
    return app.exec();
}

// kate: space-indent off; replace-tabs off;
