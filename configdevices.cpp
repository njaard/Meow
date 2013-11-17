#include "configdevices.h"
#include "player.h"

#include <qcombobox.h>
#include <qgridlayout.h>
#include <qlabel.h>

#ifdef MEOW_WITH_KDE
#include <klocale.h>
#include <kconfig.h>
#include <kconfiggroup.h>
#else
#include <qsettings.h>
#define i18n tr
#endif

struct Meow::ConfigDevices::ConfigDevicesPrivate
{
    QComboBox *devices;
    Player *player;
};

Meow::ConfigDevices::ConfigDevices(QWidget *parent, Meow::Player *player)
    : ConfigWidget(parent)
{
    d = new ConfigDevicesPrivate;
    d->player = player;
    
    QGridLayout *layout = new QGridLayout(this);
    
    QLabel *const label = new QLabel(i18n("Output device:"), this);
    layout->addWidget(label, 0, 0);
    
    d->devices = new QComboBox(this);
    layout->addWidget(d->devices, 1, 0);
    
    layout->setRowStretch(2, 2);
}

Meow::ConfigDevices::~ConfigDevices()
{
    delete d;
}

void Meow::ConfigDevices::load()
{
    d->devices->clear();
    const std::string current = d->player->currentDevice();
    
    d->devices->addItem(i18n("Default Device"), "");
    
    int atIndex =1, currentIndex=0;
    for (const std::pair<std::string,std::string> &s : d->player->devices())
    {
        if (s.first == current)
            currentIndex = atIndex;
        d->devices->addItem(QString::fromUtf8(s.second.c_str()), QString::fromUtf8(s.first.c_str()));
        atIndex++;
    }
    d->devices->setCurrentIndex(currentIndex);
}

void Meow::ConfigDevices::apply()
{
    const QString qdevice =
        d->devices->itemData(d->devices->currentIndex()).toString();
    const std::string device = qdevice.toUtf8().constData();
    d->player->setCurrentDevice(device);
    
#ifdef MEOW_WITH_KDE
    KConfigGroup meow = KGlobal::config()->group("state");
    meow.writeEntry("device", qdevice);
#else
    QSettings conf;
    conf.setValue("state/device", qdevice);
#endif

}
