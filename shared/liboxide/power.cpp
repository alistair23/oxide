#include "power.h"
#include "debug.h"

#include <QObject>
#include <QDir>

using Oxide::SysObject;


QList<SysObject>* _batteries = nullptr;
QList<SysObject>* _chargers = nullptr;

void _setup(){
    if(_batteries != nullptr && _chargers != nullptr){
        return;
    }
    if(_batteries != nullptr){
        _batteries->clear();
    }else{
        _batteries = new QList<SysObject>();
    }
    if(_chargers != nullptr){
        _chargers->clear();
    }else{
        _chargers = new QList<SysObject>();
    }
    QDir dir("/sys/class/power_supply");
    O_DEBUG("Looking for batteries and chargers...");
    for(auto& path : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable)){
        O_DEBUG(("  Checking " + path + "...").toStdString().c_str());
        SysObject item(dir.path() + "/" + path);
        if(!item.hasProperty("type")){
            O_DEBUG("    Missing type property");
            continue;
        }
        if(item.hasProperty("present") && !item.intProperty("present")){
            O_DEBUG("    Either missing present property, or battery is not present");
            continue;
        }
        auto type = item.strProperty("type");
        if(type == "Battery"){
            O_DEBUG("    Found Battery!");
            _batteries->append(item);
        }else if(type == "USB" || type == "USB_CDP"){
            O_DEBUG("    Found Charger!");
            _chargers->append(item);
        }else{
            O_DEBUG("    Unknown type");
        }
    }
    if(_chargers->empty()){
        for(SysObject& battery : *_batteries){
            _chargers->append(battery);
        }
    }
}
int _batteryInt(const QString& property){
    int result = 0;
    for(SysObject battery : *Oxide::Power::batteries()){
        result += battery.intProperty(property.toStdString());
    }
    return result;
}
int _batteryIntMax(const QString& property){
    int result = 0;
    for(SysObject battery : *Oxide::Power::batteries()){
        int value = battery.intProperty(property.toStdString());
        if(value > result){
            result = value;
        }
    }
    return result;
}
int _chargerInt(const QString& property){
    int result = 0;
    for(SysObject charger : *Oxide::Power::chargers()){
        result += charger.intProperty(property.toStdString());
    }
    return result;
}
static const QSet<QString> _batteryAlertState {
    "Overheat",
    "Dead",
    "Over voltage",
    "Unspecified failure",
    "Cold",
    "Watchdog timer expire",
    "Safety timer expire",
    "Over current"
};
static const QSet<QString> _batteryWarnState {
    "Unknown",
    "Warm",
    "Cool",
    "Hot"
};

namespace Oxide::Power {
    const QList<SysObject>* batteries(){
        _setup();
        return _batteries;
    }
    const QList<SysObject>* chargers(){
        _setup();
        return _chargers;
    }
    int batteryLevel(){ return _batteryInt("capacity") / _batteries->length(); }
    int batteryTemperature(){ return _batteryIntMax("temp") / 10; }
    bool batteryCharging(){
        for(SysObject battery : *Oxide::Power::batteries()){
            if(battery.strProperty("status") == "Charging"){
                return true;
            }
        }
        return false;
    }
    bool batteryPresent(){ return _batteryInt("present"); }
    QList<QString> batteryWarning(){
        QList<QString> warnings;
        for(SysObject battery : *Oxide::Power::batteries()){
            auto status = battery.strProperty("status");
            if(status == "Unknown" || status == ""){
                warnings.append(QString("Unknown status"));
            }
            if(!battery.hasProperty("health")){
                continue;
            }
            auto health = QString(battery.strProperty("health").c_str());
            if(_batteryWarnState.contains(health)){
                warnings.append(health);
            }
        }
        return warnings;
    }
    QList<QString> batteryAlert(){
        QList<QString> alerts;
        for(SysObject battery : *Oxide::Power::batteries()){
            if(battery.hasProperty("health")){
                auto health = QString(battery.strProperty("health").c_str());
                if(_batteryAlertState.contains(health)){
                    alerts.append(health);
                }
            }
            if(!battery.hasProperty("temp")){
                continue;
            }
            auto temp = battery.intProperty("temp");
            if(battery.hasProperty("temp_alert_max") && temp > battery.intProperty("temp_alert_max")){
                alerts.append(QString("Overheat"));
            }
            if(battery.hasProperty("temp_alert_min") && temp < battery.intProperty("temp_alert_min")){
                alerts.append(QString("Cold"));
            }
        }
        return alerts;
    }
    bool batteryHasWarning(){ return batteryWarning().length(); }
    bool batteryHasAlert(){ return batteryAlert().length(); }
    bool chargerConnected(){ return batteryCharging() || _chargerInt("online"); }
}
