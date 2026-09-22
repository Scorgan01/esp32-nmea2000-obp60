#pragma once
#include "Pagedata.h"
#include "obp60task.h"
#include <unordered_map>

class Alarms {
public:
    enum AlarmState {
        ACTIVE,
        INACTIVE,
        SNOOZE
    };

    enum SignalType {
        MESSAGE,
        LED,
        BUZZER
    };

    struct tAlarm {
        GwApi::BoatValue* boatValue = nullptr; // the monitored boat value
        bool alarmSet = false; // Alarm user setting [OFF, ON]
        double highLimit = 0; // Upper threshold for alarm
        double lowLimit = 0; // Lower threshold for alarm
        AlarmState state = INACTIVE; // Alarm state
        ulong snzTimer = 0; // Snooze time setting for this alarm in millis
        ulong suspTime = 0; // Alarm suspend time in millis
        ulong snzTime = 0; // Current snooze time in millis
        SignalType signal = MESSAGE; // how to signal MESSAGE | LED | BUZZER (for future use)
        uint8_t alarmTime = 0; // seconds until alarm disappeares without user interaction
    };

private:
    static constexpr size_t MAX_ALARMS = 2; // max. number of alarm definitions
    static constexpr double HYSTERESIS = 0.04; // 4 percent hysteresis

    std::array<tAlarm, MAX_ALARMS> alarms; // array for list of boatValues with alarms specified
    BoatValueList& m_boatValueList;
    CommonData* m_common;
    GwConfigHandler* m_config;
    GwLog* logger;

    // User settings for various boat data formats
    static String lengthFormat; // [m|ft]
    static String distanceFormat; // [m|km|nm]
    static String speedFormat; // [m/s|km/h|kn]
    static String windspeedFormat; // [m/s|km/h|kn|bft]
    static String tempFormat; // [K|C|F]
    static uint buzzerPower; // [0..100]
    static ulong ALRM_SUSP_TIME; // time in millis for each alarm being suspended after confirmation

    void addAlarm(int8_t index, GwApi::BoatValue* value, bool alarmSet, double highLimit, double lowLimit, ulong snzTimer);
    bool setSnoozeTime(tAlarm& alarm); // set snooze time
    static double convertValueToSI(const double value, const String& valueFormat); // Convert boat values from user input format to internal standard SI format

public:
    Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log);
    ~Alarms() = default;
    void readConfig(GwConfigHandler* config);
    const tAlarm* getAlarm(int index) const; // Get alarm data for <index>
    int getNoOfAlarms(); // Get number of user defined alarms
    void checkAlarms(); // Test current boat values for alarm conditions and activate alarm flag
    int countAlarms(); // Count no. of active alarms
    tAlarm* isAlarm(); // Return 1st active alarm if any alarm is currently active
    bool setAlarmState(tAlarm& alarm, AlarmState state); // Set alarm state [ACTIVE, INACTIVE, SNOOZE]
    AlarmState getAlarmState(const tAlarm& alarm) const;
    bool snoozeTimeOver(const tAlarm& alarm);
    bool activateAlarm(tAlarm& alarm) { return setAlarmState(alarm, ACTIVE); }
    bool suspendAlarm(tAlarm& alarm) { return setAlarmState(alarm, INACTIVE); }
    bool snoozeAlarm(tAlarm& alarm);
};
