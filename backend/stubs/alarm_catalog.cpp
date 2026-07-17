#include "alarm_catalog.h"

std::vector<AlarmDef> AlarmCatalog::getAllDefinitions() {
    std::vector<AlarmDef> defs;

    // ==================== 下位机1：锅炉 ====================
    {
        AlarmDef d;
        d.id = "锅炉-3-temp_high"; d.description = "锅炉-温度过高";
        d.originalLevel = EventLevel::Emergency;
        defs.push_back(d);
    }
    {
        AlarmDef d;
        d.id = "锅炉-3-pressure_low"; d.description = "锅炉-压力过低";
        d.originalLevel = EventLevel::Serious;
        defs.push_back(d);
    }
    {
        AlarmDef d;
        d.id = "锅炉-5-overload"; d.description = "锅炉-过载";
        d.originalLevel = EventLevel::Emergency;
        defs.push_back(d);
    }

    // ==================== 下位机2：冷却塔 ====================
    {
        AlarmDef d;
        d.id = "冷却塔-1-vibration"; d.description = "冷却塔-振动异常";
        d.originalLevel = EventLevel::Serious;
        defs.push_back(d);
    }
    {
        AlarmDef d;
        d.id = "冷却塔-1-water_level"; d.description = "冷却塔-水位过低";
        d.originalLevel = EventLevel::General;
        defs.push_back(d);
    }

    // ==================== 下位机3：电网 ====================
    {
        AlarmDef d;
        d.id = "电网-2-voltage_high"; d.description = "电网-电压过高";
        d.originalLevel = EventLevel::Emergency;
        defs.push_back(d);
    }
    {
        AlarmDef d;
        d.id = "电网-2-current_high"; d.description = "电网-电流过高";
        d.originalLevel = EventLevel::Serious;
        defs.push_back(d);
    }

    // ==================== 下位机4 ====================
    {
        AlarmDef d;
        d.id = "水泵-0-device_offline"; d.description = "水泵-通信中断";
        d.originalLevel = EventLevel::Serious;
        defs.push_back(d);
    }

    return defs;
}
