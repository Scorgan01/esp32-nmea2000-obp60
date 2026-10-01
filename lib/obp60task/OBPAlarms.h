#pragma once
#include "Pagedata.h"
#include "obp60task.h"
#include <unordered_map>

class Alarms {
public:
    enum AlarmState {
        INACTIVE,
        ACTIVE,
        SNOOZE
    };

    enum SignalType {
        MESSAGE,
        LED,
        BUZZER
    };

    struct tAlarm {
        GwApi::BoatValue* boatValue = nullptr; // the monitored boat value
        int configSlot = 0; // the user config slot of the alarm definition
        bool alarmSet = false; // Alarm user setting [OFF, ON]
        double highLimit = 0; // Upper threshold for alarm
        double lowLimit = 0; // Lower threshold for alarm
        bool hitHighLimit = false; // marks that high threshold was reached
        bool hitLowLimit = false; // marks that low threshold was reached
        AlarmState state = INACTIVE; // Alarm state
        ulong almDelay = 0; // Alarm delay time setting for this alarm in millis
        ulong pendingSince = 0;    // millis() when alarm condition was first detected, 0 = no pending condition
        bool pendingHigh = false;  // pending condition was caused by high limit
        bool pendingLow = false;   // pending condition was caused by low limit
        ulong snzTimer = 0; // Snooze time setting for this alarm in millis
        ulong snzTime = 0; // Current snooze time in millis
        ulong suspTime = 0; // Alarm suspend time in millis
        SignalType signal = MESSAGE; // how to signal MESSAGE | LED | BUZZER (for future use)
        uint8_t alarmTime = 0; // seconds until alarm disappeares without user interaction (for future use)
    };

private:
    static constexpr size_t MAX_ALARMS = 2; // max. number of alarm definitions
    static constexpr double HYSTERESIS = 0.04; // 4 percent hysteresis

    std::vector<tAlarm> alarmList; // array for list of boatValues with alarms specified
    BoatValueList& m_boatValueList;
    CommonData* m_common;
    GwConfigHandler* m_config;
    GwLog* logger;
    GwApi* m_api;

    // User settings for various boat data formats
    static String lengthFormat; // [m|ft]
    static String distanceFormat; // [m|km|nm]
    static String speedFormat; // [m/s|km/h|kn]
    static String windspeedFormat; // [m/s|km/h|kn|bft]
    static String tempFormat; // [K|C|F]
    static uint buzzerPower; // [0..100]
    static ulong alrmSuspTime; // time in millis for each alarm being suspended after confirmation

    static double convertValueToSI(const double value, const String& valueName, const String& valueFormat); // Convert boat values from user input format to internal standard SI format

public:
    Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log, GwApi* api);
    ~Alarms() = default;
    void readConfig(GwConfigHandler* config);
    std::vector<tAlarm>* getAlarmList() { return &alarmList; } // Get full list of defined alarms
    int getNoOfAlarms() { return alarmList.size(); } // Get number of user defined alarms
    tAlarm* getAlarm(int index); // Get alarm data for <index>
    tAlarm* getActiveAlarm(); // Get 1st active alarm in list
    void checkAlarms(); // Test current boat values for alarm conditions and manage alarm flag
    int countAlarms(); // Count no. of active alarms
    bool isAlarm(); // Test for any currently active alarm
    bool isAlarm(tAlarm& alarm){ return getAlarmState(alarm) == ACTIVE; } // Test if <alarm> is currently active
    void setAlarmState(tAlarm& alarm, AlarmState state) { alarm.state = state; } // Set alarm state [ACTIVE, INACTIVE, SNOOZE]
    AlarmState getAlarmState(const tAlarm& alarm) const { return alarm.state; }
    bool hitHighLimit (const tAlarm& alarm) const { return alarm.hitHighLimit; }
    bool hitLowLimit (const tAlarm& alarm) const { return alarm.hitLowLimit; }
    bool suspTimeOver(const tAlarm& alarm) const;
    bool snoozeTimeOver(const tAlarm& alarm) const;
    void activateAlarm(tAlarm& alarm) { setAlarmState(alarm, ACTIVE); }
    void suspendAlarm(tAlarm& alarm);
    void snoozeAlarm(tAlarm& alarm);
};
