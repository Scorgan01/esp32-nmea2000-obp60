#include "OBPAlarms.h"

String Alarms::lengthFormat = "";
String Alarms::distanceFormat = "";
String Alarms::speedFormat = "";
String Alarms::windspeedFormat = "";
String Alarms::tempFormat = "";
uint Alarms::buzzerPower = 0;
ulong Alarms::alrmSuspTime = 0;

Alarms::Alarms(BoatValueList& boatValueList, CommonData* common, GwLog* log, GwApi* api)
    : m_boatValueList(boatValueList)
    , m_common(common)
    , logger(log)
    , m_api(api)
{
}

// Initial load of alarm definitions into internal list
void Alarms::readConfig(GwConfigHandler* config)
{
    String noInstance = ""; // the instance variable to read from config
    String noAlarmSet = "";
    String noHighLimit = "";
    String noLowLimit = "";
    String noAlmDelay = "";
    String noSnzTime = "";
    String instance = "";

    alarmList.clear();
    alarmList.reserve(MAX_ALARMS);

    // Read user settings
    lengthFormat = config->getString(config->lengthFormat);
    distanceFormat = config->getString(config->distanceFormat);
    speedFormat = config->getString(config->speedFormat);
    windspeedFormat = config->getString(config->windspeedFormat);
    tempFormat = config->getString(config->tempFormat);
    buzzerPower = uint(config->getString(config->buzzerPower, "100").toInt());
    alrmSuspTime = ulong(config->getString(config->almSuspTime, "60").toInt() * 1000 * 60); // user setting is in minutes

    // Read alarm definition for data instances
    for (size_t i = 0; i < MAX_ALARMS; i++) {
        noInstance = "almInstance" + String(i + 1);
        noHighLimit = "almHighLimit" + String(i + 1);
        noLowLimit = "almLowLimit" + String(i + 1);
        noAlarmSet = "almSet" + String(i + 1);
        noAlmDelay = "almDelay" + String(i + 1);
        noSnzTime = "snzTime" + String(i + 1);

        instance = config->getString(noInstance, "---");
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "No alarm definition for instance no. %d", i + 1);
            continue;
        }

        tAlarm a;
        a.boatValue = m_boatValueList.findValueOrCreate(instance);
        a.configSlot = i + 1;
        a.alarmSet = config->getBool(noAlarmSet, false);
        double highLimit = config->getString(noHighLimit, "").toDouble();
        double lowLimit = config->getString(noLowLimit, "").toDouble();
        highLimit = convertValueToSI(highLimit, a.boatValue->getName(), a.boatValue->getFormat());
        lowLimit = convertValueToSI(lowLimit, a.boatValue->getName(), a.boatValue->getFormat());
        if (highLimit > 0 && lowLimit > highLimit) {
            lowLimit = highLimit; // adjust <lowLimit> if <highLimit> is set
        }
        a.highLimit = highLimit;
        a.lowLimit = lowLimit;
        a.almDelay = ulong(config->getString(noAlmDelay, "").toInt() * 1000); // user setting is in seconds
        a.snzTimer = ulong(config->getString(noSnzTime, "").toInt() * 1000); // user setting is in seconds
        a.state = INACTIVE;
        a.snzTime = 0;
        a.suspTime = 0;
        a.alarmTime = 0;
        a.signal = MESSAGE;
        alarmList.push_back(a);

        LOG_DEBUG(GwLog::LOG, "Alarm definition added: boat value: %s, low limit: %f, high limit: %f, alarm set: %d, alarm delay: %d, snooze timer: %d",
            alarmList[i].boatValue->getName().c_str(), alarmList[i].lowLimit, alarmList[i].highLimit, alarmList[i].alarmSet, alarmList[i].almDelay, alarmList[i].snzTimer);
    }

    // Update boat value list with format parameters and adjust user defined limit values -> we need corresponding data for formatting
    m_api->getBoatDataValues(m_boatValueList.numValues, m_boatValueList.allBoatValues);
    for (auto& a : alarmList) {
        a.highLimit = convertValueToSI(a.highLimit, a.boatValue->getName(), a.boatValue->getFormat());
        a.lowLimit = convertValueToSI(a.lowLimit, a.boatValue->getName(), a.boatValue->getFormat());
    }

    LOG_DEBUG(GwLog::LOG, "All alarm definition data read");
}

// Get alarm definition for <index>
Alarms::tAlarm* Alarms::getAlarm(int index)
{
    if (index < 0 || index >= (int)alarmList.size()) {
        return nullptr;
    }

    return &alarmList[index];
}

// Get 1st active alarm in list
Alarms::tAlarm* Alarms::getActiveAlarm()
{
    for (auto& alarm : alarmList) {
        if (isAlarm(alarm))
            return &alarm;
    }

    return nullptr;
}

// Test current boat values for alarm conditions and activate alarm flag
void Alarms::checkAlarms()
{
    for (auto& alarm : alarmList) {
        LOG_DEBUG(GwLog::DEBUG, "checkAlarms: #%d, boat value: %s, low limit: %.2f, high limit: %.2f, value: %.2f, valid: %d, set: %d",
            alarm.configSlot, alarm.boatValue->getName().c_str(), alarm.lowLimit, alarm.highLimit, alarm.boatValue->value, alarm.boatValue->valid, alarm.alarmSet);

        if (!alarm.alarmSet || !alarm.boatValue->valid) {
            setAlarmState(alarm, INACTIVE); // ToDo: implement holdValues and 4 sec delay after invalid data
            continue;
        }

        double value = alarm.boatValue->value;
        AlarmState currentState = getAlarmState(alarm);

        if ((currentState == INACTIVE && suspTimeOver(alarm)) || (currentState == SNOOZE && snoozeTimeOver(alarm))) {
            if (alarm.highLimit > 0 && value > alarm.highLimit) {
                activateAlarm(alarm);
                alarm.hitHighLimit = true;
            }
            if (alarm.lowLimit > 0 && value < alarm.lowLimit) {
                activateAlarm(alarm);
                alarm.hitLowLimit = true;
            }

        } else if (currentState == ACTIVE) {
            bool highCleared = (alarm.highLimit <= 0) || (value < alarm.highLimit * (1.0 - HYSTERESIS));
            bool lowCleared = (alarm.lowLimit <= 0) || (value > alarm.lowLimit * (1.0 + HYSTERESIS));

            if (highCleared && lowCleared) {
                suspendAlarm(alarm);
                if (highCleared)
                    alarm.hitHighLimit = false;
                if (lowCleared)
                    alarm.hitLowLimit = false;
            }
        }
        LOG_DEBUG(GwLog::DEBUG, "checkAlarms: alarm state for alarm: %s - %d", alarm.boatValue->getName().c_str(), getAlarmState(alarm));
    }
}

// Count no. of active alarms
int Alarms::countAlarms()
{
    int count = 0;
    for (auto& alarm : alarmList) {
        if (getAlarmState(alarm) == ACTIVE) {
            count++;
        }
    }

    return count;
}

// Test for any currently active alarm
bool Alarms::Alarms::isAlarm()
{
    for (auto& alarm : alarmList) {
        if (getAlarmState(alarm) == ACTIVE) {
            return true;
        }
    }

    return false;
}

void Alarms::suspendAlarm(tAlarm& alarm)
{
    alarm.suspTime = millis();
    setAlarmState(alarm, INACTIVE);
}

void Alarms::snoozeAlarm(tAlarm& alarm)
{
    alarm.snzTime = millis();
    setAlarmState(alarm, SNOOZE);
}

// Check alarm inactive time
bool Alarms::suspTimeOver(const tAlarm& alarm) const
{
    if (alarm.suspTime == 0) { // no suspend happened yet -> initial phase
        return true;
    }

    if ((millis() - alarm.suspTime) > alrmSuspTime) {
        return true;
    } else {
        return false;
    }
}

// Check snooze time
bool Alarms::snoozeTimeOver(const tAlarm& alarm) const
{
    if (alarm.snzTime == 0) { // no snooze happened yet -> initial phase
        return true;
    }

    if ((millis() - alarm.snzTime) > alarm.snzTimer) {
        return true;
    } else {
        return false;
    }
}

// Convert boat values from user input format to internal standard SI format
double Alarms::convertValueToSI(const double value, const String& valueName, const String& valueFormat)
{
    double result = value;

    if (valueFormat == "formatKnots") {
        if (valueName == "AWS" || valueName == "TWS" || valueName == "MaxAws" || valueName == "MaxTws") {
            if (windspeedFormat == "km/h") {
                result /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                result /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                result *= 2 + (value / 2); // Convert Bft to m/s (approx) -> to be improved
            } else { // value is given in SI m/s
                // No conversion needed
            }
        }
        if (valueName == "SOG" || valueName == "STW" || valueName == "DFT") {
            if (speedFormat == "km/h") {
                result /= 3.6; // Convert km/h to m/s
            } else if (speedFormat == "kn") {
                result /= 1.94384; // Convert kn to m/s
            } else { // value is given in SI m/s
                // No conversion needed
            }
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

    return result;
}