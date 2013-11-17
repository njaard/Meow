#ifndef MEOW_CONFIG_DEVICES_H
#define MEOW_CONFIG_DEVICES_H

#include "configdialog.h"

namespace Meow
{

class Player;

class ConfigDevices : public ConfigWidget
{
    Q_OBJECT
    struct ConfigDevicesPrivate;
    ConfigDevicesPrivate *d;

public:
    ConfigDevices(QWidget *parent, Player *p);
    ~ConfigDevices();
    
    virtual void load();
    virtual void apply();
};



}

#endif