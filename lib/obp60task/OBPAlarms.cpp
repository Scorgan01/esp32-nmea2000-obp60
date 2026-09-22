#include "OBPAlarms.h"

String Alarms::lengthFormat = "";
String Alarms::distanceFormat = "";
String Alarms::speedFormat = "";
String Alarms::windspeedFormat = "";
String Alarms::tempFormat = "";
uint Alarms::buzzerPower = 0;
ulong Alarms::ALRM_SUSP_TIME = 0;

Alarms::Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log)
    : m_boatValueList(boatValueList)
    , m_common(common)
    , logger(log)
{
}

void Alarms::addAlarm(int8_t index, GwApi::BoatValue* value, bool alarmSet, double highLimit, double lowLimit, ulong snzTimer)
{
    alarms[index].boatValue = value;
    alarms[index].alarmSet = alarmSet;
    alarms[index].highLimit = highLimit;
    alarms[index].lowLimit = lowLimit;
    alarms[index].state = INACTIVE;
    alarms[index].snzTimer = snzTimer;
    alarms[index].snzTime = 0;
    alarms[index].signal = MESSAGE;
    alarms[index].alarmTime = 0;
}

// Initial load of alarm definitions into internal list
void Alarms::readConfig(GwConfigHandler* config)
{
    {
        String noInstance = ""; // the instance variable to read from config
        String noAlarmSet = "";
        String noHighLimit = "";
        String noLowLimit = "";
        String noSnzTime = "";

        String instance;
        GwApi::BoatValue* alarmInstance = nullptr;
        double highLimit = 0;
        double lowLimit = 0;
        bool set = false;
        ulong snzTimer = 0;

        // Read user settings
        lengthFormat = config->getString(config->lengthFormat);
        distanceFormat = config->getString(config->distanceFormat);
        speedFormat = config->getString(config->speedFormat);
        windspeedFormat = config->getString(config->windspeedFormat);
        tempFormat = config->getString(config->tempFormat);
        buzzerPower = uint(config->getString(config->buzzerPower, "100").toInt());
        ALRM_SUSP_TIME = ulong(config->getString(config->almSuspTime, "60").toInt() * 1000 * 60); // user setting is in minutes

        // Read alarm definition for data instances
        for (size_t i = 0; i < MAX_ALARMS; i++) {
            noInstance = "almInstance" + String(i + 1);
            noHighLimit = "almHighLimit" + String(i + 1);
            noLowLimit = "almLowLimit" + String(i + 1);
            noAlarmSet = "almSet" + String(i + 1);
            noSnzTime = "snzTime" + String(i + 1);

            instance = config->getString(noInstance, "---");
            if (instance == "---") {
                LOG_DEBUG(GwLog::LOG, "No alarm definition for instance no. %d", i + 1);
                continue;
            }

            alarmInstance = m_boatValueList.findValueOrCreate(instance);
            set = config->getBool(noAlarmSet, false);
            highLimit = config->getString(noHighLimit, "").toDouble();
            lowLimit = config->getString(noLowLimit, "").toDouble();
            //highLimit = convertValueToSI(highLimit, alarmInstance->getFormat());
            //lowLimit = convertValueToSI(lowLimit, alarmInstance->getFormat());
            snzTimer = ulong(config->getString(noSnzTime, "").toInt() * 1000); // user setting is in seconds

            if (highLimit > 0 && lowLimit > highLimit) {
                lowLimit = highLimit; // adjust <lowLimit> if <highLimit> is set
            }

            addAlarm(i, alarmInstance, set, highLimit, lowLimit, snzTimer);
            LOG_DEBUG(GwLog::LOG, "Alarm definition added: boat value: %s, low limit: %f, high limit: %f, alarm set: %d",
                instance, lowLimit, highLimit, set);
            LOG_DEBUG(GwLog::LOG, "Alarm definition added: boat value: %s, low limit: %f, high limit: %f, alarm set: %d",
                alarms[i].boatValue->getName().c_str(), alarms[i].lowLimit, alarms[i].highLimit, alarms[i].alarmSet);
        }
        LOG_DEBUG(GwLog::LOG, "All alarm definition data read");
    }
}

// Get alarm data for <index>
const Alarms::tAlarm* Alarms::getAlarm(int index) const
{
    if (index >= 0 && index < MAX_ALARMS && alarms[index].boatValue != nullptr) {
        return &alarms[index];
    }
    
    return nullptr;
}

// Get number of user defined alarms
int Alarms::getNoOfAlarms()
{
    int count = 0;

    for (auto& alarm : alarms) {
        if (alarm.boatValue != nullptr)
            count++;
    }

    return count; 
}

// Test current boat values for alarm conditions and activate alarm flag
void Alarms::checkAlarms()
{
    for (auto& alarm : alarms) {
        LOG_DEBUG(GwLog::LOG, "checkAlarms: boat value: %s, low limit: %.2f, high limit: %.2f, value: %.2f",
                alarm.boatValue->getName().c_str(), alarm.lowLimit, alarm.highLimit, alarm.boatValue->value);

        if (!alarm.alarmSet || alarm.boatValue == nullptr || !alarm.boatValue->valid)
            continue;

        double value = alarm.boatValue->value;
        AlarmState currentState = getAlarmState(alarm);
        LOG_DEBUG(GwLog::LOG, "checkAlarms: boat value: %s, low limit: %.2f, high limit: %.2f, value: %.2f",
                alarm.boatValue->getName().c_str(), alarm.lowLimit, alarm.highLimit, value);

        if (currentState == INACTIVE || (currentState == SNOOZE && snoozeTimeOver(alarm))) {
            if ((alarm.highLimit > 0 && value > alarm.highLimit) || 
                (alarm.lowLimit > 0 && value < alarm.lowLimit)) {
                setAlarmState(alarm, ACTIVE);
            }
        } else if (currentState == ACTIVE) {
            bool highCleared = (alarm.highLimit <= 0) || (value < alarm.highLimit * (1.0 - HYSTERESIS));
            bool lowCleared  = (alarm.lowLimit <= 0)  || (value > alarm.lowLimit * (1.0 + HYSTERESIS));

            if (highCleared && lowCleared) {
                setAlarmState(alarm, INACTIVE);
            }
        }
    }
}

// Count no. of active alarms
int Alarms::countAlarms()
{
    int count = 0;

    for (auto& alarm : alarms) {
        if (alarm.boatValue != nullptr && getAlarmState(alarm) == ACTIVE)
            count++;
    }

    return count;
};

// Return 1st active alarm if any alarm is currently active
Alarms::tAlarm* Alarms::isAlarm()
{
    for (auto& alarm : alarms) {
        if (alarm.boatValue != nullptr && getAlarmState(alarm) == ACTIVE)
            return &alarm;
    }

    return nullptr;
}

// Set alarm state [ACTIVE, INACTIVE, SNOOZE]
bool Alarms::setAlarmState(tAlarm& alarm, AlarmState state)
{
    if (alarm.boatValue == nullptr)
        return false;

    alarm.state = state;
    return true;
}

// Get alarm state [ACTIVE, INACTIVE, SNOOZE]
Alarms::AlarmState Alarms::getAlarmState(const tAlarm& alarm) const
{
    if (alarm.boatValue == nullptr)
        return INACTIVE;

    return alarm.state;
}

bool Alarms::snoozeAlarm(tAlarm& alarm)
{
    if (alarm.boatValue == nullptr)
        return false;

    setSnoozeTime(alarm);
    return setAlarmState(alarm, SNOOZE);
}

// Set snooze time
bool Alarms::setSnoozeTime(tAlarm& alarm)
{
    if (alarm.boatValue == nullptr)
        return false;

    alarm.snzTime = millis();
    return false;
}

// Check snooze time
bool Alarms::snoozeTimeOver(const tAlarm& alarm)
{
    if (alarm.boatValue == nullptr)
        return false;

    if ((millis() - alarm.snzTime) > alarm.snzTimer)
        return true;
    else
        return false;
}

// Convert boat values from user input format to internal standard SI format
double Alarms::convertValueToSI(const double value, const String& valueFormat)
{
    double result;

    // TODO
    if (valueFormat == "formatSpeed") {
        if (windspeedFormat == "km/h") {
            result /= 3.6; // Convert km/h to m/s
        } else if (windspeedFormat == "kn") {
            result /= 1.94384; // Convert kn to m/s
        } else if (windspeedFormat == "bft") {
            result *= 2 + (value / 2); // Convert Bft to m/s (approx) -> to be improved

            //        } else if (instance == "AWA" || instance == "COG" || instance == "HDM" || instance == "HDT" || instance == "PRPOS" || instance == "RPOS" || instance == "TWA" || instance == "TWD") {
            //            result *= DEG_TO_RAD; // Convert deg to rad
        } else { // value is given in SI m/s
            // No conversion needed
        }
    }

    else if (valueFormat == "formatDepth") {
        if (lengthFormat == "ft") {
            result /= 3.28084; // Convert ft to m
        } else { // value is given in SI metres
            // No conversion needed
        }
    }

    else if (valueFormat == "formatXte") {
        if (String(distanceFormat) == "km") {
            result *= 1000;
        } else if (String(distanceFormat) == "nm") {
            result *= 1852;
        } else { // value is given in SI metres
            // No conversion needed
        }
    }

    else if (valueFormat == "kelvinToC") {
        if (tempFormat == "C") {
            result += 273.15;
        } else if (tempFormat == "F") {
            result = (result - 32.0) * (5.0 / 9.0) + 273.15; // Convert °F to K
        } else { // value is given in SI Kelvin
            // No conversion needed
        }
    }

    /* else if (instance == "SOG" || instance == "STW")
    {
        if (speedFormat == "m/s") {
            // No conversion needed
        } else if (speedFormat == "km/h") {
            result /= 3.6; // Convert km/h to m/s
        } else if (speedFormat == "kn") {
            result /= 1.94384; // Convert kn to m/s
        } */

    return result;
}