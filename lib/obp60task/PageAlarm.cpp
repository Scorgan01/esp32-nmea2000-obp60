#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPAlarms.h"
#include <vector>

class PageAlarm : public Page {
private:

    enum BuzzerMode {
        OFF,
        SHORT,
        LONG,
        TIME,
        ACK
    };

    GwLog* logger;

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock

    // String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    bool smallDecimals;
    String sBuzzerMode;
    BuzzerMode buzzerMode;
    ulong buzzerDuration;
    bool buzzerOn;

    Alarms* boatAlarms = nullptr;
    ulong buzzerTime = 0;
    bool flashHdr = true;

    // Old values for hold function
    String sValue1Old = "";
    String unit1Old = "";

public:
    PageAlarm(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        GwConfigHandler* config = commonData->config;

        LOG_DEBUG(GwLog::LOG, "Instantiate PageAlarm");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height
        getdisplay().setTextWrap(false);

        // Get config data
        useSimuData = config->getBool(config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        smallDecimals = commonData->config->getBool(commonData->config->smallDecimals);
        buzzerDuration = ulong(config->getString(commonData->config->buzzerTime, "").toInt() * 1000); // user setting is in seconds
        sBuzzerMode = commonData->config->getString(commonData->config->buzzerMode);
        if (sBuzzerMode == "Off") {
            buzzerMode = OFF;
        } else if (sBuzzerMode == "Short Beep") {
            buzzerMode = SHORT;
        } else if (sBuzzerMode == "Long Beep") {
            buzzerMode = LONG;
        } else if (sBuzzerMode == "Beep Duration") {
            buzzerMode = TIME;
        } else if (sBuzzerMode == "Beep until Confirmation") {
            buzzerMode = ACK;
        } else {
            buzzerMode = SHORT;
        };

        boatAlarms = commonData->alarmList;
    }

    virtual void setupKeys()
    {
        Page::setupKeys();

#if defined BOARD_OBP60S3
        constexpr int SNZ_KEY = 4;
#elif defined BOARD_OBP40S3
        constexpr int SNZ_KEY = 1;
#endif
        commonData->keydata[0].label = "CNFRM";
        commonData->keydata[SNZ_KEY].label = "SNOOZE";
    }

    // Key functions
    virtual int handleKey(int key)
    {
        Alarms::tAlarm* m_alarm = boatAlarms->getActiveAlarm(); // get 1st active alarm if any exist

        if (key == 1) {
            if (m_alarm != nullptr) {
                boatAlarms->suspendAlarm(*m_alarm); // handle always first alarm in list
            }
            return 0; // Commit the key
        }

#if defined BOARD_OBP60S3
        if (key == 5) {
#elif defined BOARD_OBP40S3
        if (key == 2) {
#endif
            if (m_alarm != nullptr) {
                boatAlarms->snoozeAlarm(*m_alarm); // handle always first alarm in list
            }
            return 0; // Commit the key
        }

        // Keylock function
        if (key == 11) { // Code for keylock
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData& pageData)
    {
        setupKeys();

#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        buzzerOn = true; // start new alarm window with buzzer sound on
        buzzerTime = millis();  // start new alarm windows with activated buzzer sound
        flashHdr = true; // start new alarm window with activated "Alarm" headline
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageAlarm");

#ifdef BOARD_OBP60S3
        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        if (!boatAlarms->isAlarm())
            return PAGE_OK; // no active alarm, no page to display; should never happen

        std::vector<Alarms::tAlarm>* alarmList;
        alarmList = boatAlarms->getAlarmList();

        LOG_DEBUG(GwLog::DEBUG, "PageAlarm: # of alarms: Defined - %d; active - %d", boatAlarms->getNoOfAlarms(), boatAlarms->countAlarms());

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, width, height); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        int x = 20;
        int y = 50;
        int xTab1 = 45;
        int xTab2 = 125;
        int xTab3 = 245;
        int yTabHdr = 60;
        int yTabLn = 74;
        int lineHgt = 28;

        getdisplay().drawRoundRect(x, y, 360, 222, 5, commonData->fgcolor); // Draw box

        if (flashHdr) { // flash header once per second
            // exclamation icon in left top corner
            getdisplay().drawXBitmap(x + 16, y + 8, exclamation_bits, exclamation_width, exclamation_height, commonData->fgcolor);
            getdisplay().setFont(&Ubuntu_Bold12pt8b);
            drawTextCenter(x + 170, y + 20, "ALARM!");
        }
        flashHdr = !flashHdr;

        // print table header
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(x + xTab1, y + yTabHdr);
        getdisplay().print("Type");
        getdisplay().setCursor(x + xTab2, y + yTabHdr);
        getdisplay().print("Value");
        getdisplay().setCursor(x + xTab3, y + yTabHdr);
        getdisplay().print("Threshold");
        getdisplay().drawLine(x + 10, y + yTabHdr + 5, x + 350, y + yTabHdr + 5, commonData->fgcolor);

        int i = 0;
        const GFXfont* textFnt = nullptr;
        const GFXfont* numFnt = nullptr;
        for (auto& alarm : *alarmList) {
            if (boatAlarms->getAlarmState(alarm) == Alarms::ACTIVE) {
                i++;

                if (i == 1) { // increase font size for 1st alarm because this is the alarm to be acknoledged
                    textFnt = &Ubuntu_Bold12pt8b;
                    numFnt = &DSEG7Classic_BoldItalic16pt7b;
                } else {
                    textFnt = &Ubuntu_Bold10pt8b;
                    numFnt = &DSEG7Classic_BoldItalic12pt7b;
                }

                GwApi::BoatValue* bvalue = alarm.boatValue;
                String name = xdrDelete(bvalue->getName()); // Value name
                name = name.substring(0, 6); // String length limit for value name
                double value = bvalue->value; // Value as double in SI unit
                String svalue = formatValue(bvalue, *commonData).svalue;
                String unit = formatValue(bvalue, *commonData).unit; // Unit of value

                getdisplay().setCursor(x + 10, y + yTabLn + lineHgt * i);
                getdisplay().setFont(textFnt);
                getdisplay().print("#" + String(i));
                getdisplay().setCursor(x + xTab1, y + yTabLn + lineHgt * i);
                getdisplay().print(name);
                getdisplay().setFont(numFnt);
                drawTextRalign(x + xTab3 - 49, y + yTabLn + lineHgt * i, svalue, true);
                getdisplay().setFont(&Ubuntu_Bold8pt8b);
                getdisplay().print(unit);

                double threshold;
                if (boatAlarms->hitHighLimit(alarm))
                    threshold = alarm.highLimit;
                if (boatAlarms->hitLowLimit(alarm))
                    threshold = alarm.lowLimit;
                GwApi::BoatValue tmpBVal(bvalue->getName()); // temporary boat value for string formatter
                tmpBVal.setFormat(bvalue->getFormat());
                tmpBVal.valid = true;
                tmpBVal.value = threshold;
                String sThreshold = formatValue(&tmpBVal, *commonData).svalue;
                getdisplay().setFont(numFnt);
                drawTextRalign(x + 316, y + yTabLn + lineHgt * i, sThreshold, true);
                getdisplay().setFont(&Ubuntu_Bold8pt8b);
                getdisplay().print(unit);
            }
        }

#if defined BOARD_OBP60S3
        if (buzzerMode == OFF) {
            // don't want any buzzer noise
        } else if (buzzerMode == SHORT && buzzerOn) {
            buzzer(TONE4, 250);
            buzzerOn = false; // single alarm tone only
        } else if (buzzerMode == LONG && buzzerOn) {
            buzzer(TONE4, 500);
            buzzerOn = false; // single alarm tone only
        } else if (buzzerMode == TIME && (millis() - buzzerTime < buzzerDuration)) {
                // buzzer only as long as buzzerDuration is not exceeded; <buzzerTime> will be reset at next displayNew()
                buzzer(TONE4, 250);
        } else if (buzzerMode == ACK) {
            // ToDo
        }
#endif        
        // Update display
        displayNextPage(); // Partial update (fast)
        return PAGE_OK;
    }
};

static Page* createPage(CommonData& common)
{
    return new PageAlarm(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageAlarm(
    "Alarm", // Page name
    createPage, // Action
    0, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
