#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "GwApi.h"

class PageDST810 : public Page
{
private:
    GwLog* logger;

    enum LogMode {
        LOG,
        TRIP
    };

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    bool useSimuData;
    bool holdValues;
    String flashLED;
    String backlightMode;
    String lengthformat;
    uint8_t dst810Address = 36; // N2k device address of DST810 triducer (255 = broadcast)

    LogMode logMode = TRIP; // Mode for log data
    bool rstTripLog = false; // Indicator for reset command of trip log data

    // Old values for hold function
    String svalue1old = "";
    String unit1old = "";
    String svalue2old = "";
    String unit2old = "";
    String svalue3old = "";
    String unit3old = "";
    String svalue4old = "";
    String unit4old = "";

    // send NMEA2000 message 126208 for reset of trip log data on Airmar DST 810 triducer
    void resetDSTTripLog(PageData &pageData, uint8_t n2kTarget) {

        /* Maretron DST110 documentation https://www.maretron.com/support/manuals/DST110UM_1.0.html

        PGN 126208 – NMEA Command Group Function – Distance Log Reset
        This command will reset the “Distance Since Last Reset” field of the Distance Log PGN (128275).

        Field  1:  Complex Command Group Function Code (8 bits) – set this field’s value to 0x01, which denotes a command PGN
                2:  Commanded PGN (24 bits) – set this field’s value to 128275, which denotes the Distance Log PGN
                3:  Priority Setting (4 bits) – set this field’s value to 0x8, which indicates to leave priority settings unchanged
                4:  Reserved (4 bits) – set this field’s value to 0xF, which is the value for a reserved field of this size
                5:  Number of Pairs of Commanded Parameters to Follow (8 bits) – set this field’s value to 0x1, indicating that one parameter will follow
                6:  Number of First Commanded Parameter (8 bits) – set this field’s value to 0x4, which indicates the Distance Since Last Reset field
                7:  Distance Since Last Reset (16 bits) – set this field’s value 0 to reset the Distance Since Last Reset counter to zero. */

        tN2kMsg n2kMsg;

        n2kMsg.Clear();                          // initialise message / clear
        n2kMsg.SetPGN(126208L);                  // PGN 126208 (NMEA group function)
        n2kMsg.Priority = 3;                     // message specific priority
        n2kMsg.Destination = n2kTarget;          // should specify real destination, but cannot query it out of user task

        // Payload for PGN 126208 (Command)
        n2kMsg.AddByte(0x01);                    // Byte 1: function code = command (1)
        n2kMsg.Add3ByteInt(128275);              // Bytes 2–4: target PGN 128275 (distance log)
        n2kMsg.AddByte(0x8F);                    // Byte 5: priority = 8, reserved = F
        n2kMsg.AddByte(0x01);                    // Byte 6: no. of parameter = 1
        n2kMsg.AddByte(0x04);                    // Byte 7: parameter index = 4 (distance since last reset)
        n2kMsg.Add4ByteUInt(0);                  // Bytes 8-11: value = 0 (reset)

        pageData.api->sendN2kMessage(n2kMsg, true);
        LOG_DEBUG(GwLog::DEBUG, "N2k PGN 126208 sent to DST810. Target address: %u (destination in PGN: %u)", n2kTarget, (unsigned)n2kMsg.Destination);
        LOG_DEBUG(GwLog::DEBUG, "N2k PGN 126208 sent. dst810Address: %u", dst810Address);
    }

public:
    PageDST810(CommonData &common){
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageDST810");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height

        // Get config data
        useSimuData = commonData->config->getBool(commonData->config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        backlightMode = commonData->config->getString(commonData->config->backlight);
        lengthformat = commonData->config->getString(commonData->config->lengthFormat);
#if defined BOARD_OBP60S3
        dst810Address = uint(commonData->config->getInt(commonData->config->dst810Target));
#endif
    }

    virtual void setupKeys()
    {
        Page::setupKeys();

        commonData->keydata[0].label = "MODE";
#if defined BOARD_OBP60S3
        constexpr int RST_KEY = 4;
//        constexpr int RST_KEY = 1;
        if (logMode == TRIP) { // show "RESET" key only if trip log data is selected
            commonData->keydata[RST_KEY].label = "RESET";
        } else {
            commonData->keydata[RST_KEY].label = "";
        }
#endif
    }

    virtual int handleKey(int key){

        if (key == 1) {
            switch (logMode) {
            case TRIP:
                logMode = LOG;
                break;
            case LOG:
                logMode = TRIP;
                break;
            }
            setupKeys(); // Adjust key definition depending on <logMode>
            return 0; // Commit the key
        }

    #if defined BOARD_OBP60S3
        // OBP40 cannot send N2k messages
        // Reset trip log if <reset> button has been pressed
        if (key == 5  && logMode == TRIP) {
//        if (key == 2  && logMode == TRIP) {
            rstTripLog = true;
            return 0; // Commit the key
        }
    #endif

        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    virtual void displayNew(PageData& pageData)
    {
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
    }
        
    int displayPage(PageData &pageData){

        // Get boat values #1 - DBT
        GwApi::BoatValue *bvalue1 = pageData.values[0]; // First element in list
        String name1 = xdrDelete(bvalue1->getName());   // Value name
        double value1 = bvalue1->value;                 // Value as double in SI unit
        bool valid1 = bvalue1->valid;                   // Valid information 
        String svalue1 = formatValue(bvalue1, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bvalue1, *commonData).unit;        // Unit of value

        // Get boat values #2 - STW
        GwApi::BoatValue *bvalue2 = pageData.values[1]; // Second element in list
        String name2 = xdrDelete(bvalue2->getName());   // Value name
        double value2 = bvalue2->value;                 // Value as double in SI unit
        bool valid2 = bvalue2->valid;                   // Valid information 
        String svalue2 = formatValue(bvalue2, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit2 = formatValue(bvalue2, *commonData).unit;        // Unit of value

        // Get boat values #3 - TripLog or Log
        GwApi::BoatValue *bvalue3;
        if (logMode == TRIP) {
            bvalue3 = pageData.values[2]; // Second element in list -> TripLog
        } else {
            bvalue3 = pageData.values[3]; // Third element in list -> Log
        }
        String name3 = xdrDelete(bvalue3->getName());   // Value name
        double value3 = bvalue3->value;                 // Value as double in SI unit
        bool valid3 = bvalue3->valid;                   // Valid information 
        String svalue3 = formatValue(bvalue3, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit3 = formatValue(bvalue3, *commonData).unit;        // Unit of value

        // Get boat values #4 - WTemp
        GwApi::BoatValue *bvalue4 = pageData.values[4]; // Second element in list
        String name4 = xdrDelete(bvalue4->getName());   // Value name
        double value4 = bvalue4->value;                 // Value as double in SI unit
        bool valid4 = bvalue4->valid;                   // Valid information 
        String svalue4 = formatValue(bvalue4, *commonData).svalue;    // Formatted value as string including unit conversion and switching decimal places
        String unit4 = formatValue(bvalue4, *commonData).unit;        // Unit of value

        if (bvalue1 == NULL) return PAGE_OK;

        if (rstTripLog) { // user pressed reset buttion for trip log data
            resetDSTTripLog(pageData, dst810Address);
            buzzer(TONE3, 150);
            LOG_DEBUG(GwLog::LOG,"PageDST810: Trip log data reset performed. N2k target address: %u", dst810Address);

            rstTripLog = false;
        }

        // Optical warning by limit violation (unused)
        /* if(String(flashLED) == "Limit Violation"){
            setBlinkingLED(false);
            setFlashLED(false); 
        } */

        LOG_DEBUG(GwLog::LOG,"Drawing at PageDST810, %s: %f, %s: %f, %s: %f, %s: %f", name1.c_str(), value1, name2.c_str(), value2, name3.c_str(), value3, name4.c_str(), value4);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, width, height); // Set partial update

        getdisplay().setTextColor(commonData->fgcolor);

        // ############### Value 1 ################

        // Show name
        getdisplay().setFont(&Ubuntu_Bold20pt8b);
        getdisplay().setCursor(20, 55);
        getdisplay().print("Depth");                         // Page name

        // Show unit
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(20, 90);
        if(holdValues == false){
            getdisplay().print(unit1);                       // Unit
        }
        else{
            getdisplay().print(unit1old);
        }

        // Set font
        getdisplay().setFont(&DSEG7Classic_BoldItalic30pt7b);
        getdisplay().setCursor(180, 90);

        // Show bus data
        if(holdValues == false){
            getdisplay().print(svalue1);                                     // Real value as formated string
        }
        else{
            getdisplay().print(svalue1old);                                  // Old value as formated string
        }
        if(valid1 == true){
            svalue1old = svalue1;                                       // Save the old value
            unit1old = unit1;                                           // Save the old unit
        }

        // ############### Horizontal Line ################

        // Horizontal line 3 pix
        getdisplay().fillRect(0, 105, 400, 3, commonData->fgcolor);

        // ############### Value 2 ################

        // Show name
        getdisplay().setFont(&Ubuntu_Bold20pt8b);
        getdisplay().setCursor(20, 145);
        getdisplay().print("Speed");

        // Show unit
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(20, 180);
        if(holdValues == false){
            getdisplay().print(unit2);                       // Unit
        }
        else{
            getdisplay().print(unit2old);
        }

        // Setfont
        getdisplay().setFont(&DSEG7Classic_BoldItalic30pt7b);
        getdisplay().setCursor(180, 180);

        // Show bus data
        if(holdValues == false){
            getdisplay().print(svalue2);                                     // Real value as formated string
        }
        else{
            getdisplay().print(svalue2old);                                  // Old value as formated string
        }
        if(valid2 == true){
            svalue2old = svalue2;                                       // Save the old value
            unit2old = unit2;                                           // Save the old unit
        }

        // ############### Horizontal Line ################

        // Horizontal line 3 pix
        getdisplay().fillRect(0, 195, 400, 3, commonData->fgcolor);

        // ############### Value 3 ################ - TripLog / Log

        // Show name
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(20, 220);
        getdisplay().print(name3);                          // [TripLog | Log]

        // Show unit
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(20, 240);
        if(holdValues == false){
            getdisplay().print(unit3);                       // Unit
        }
        else{
            getdisplay().print(unit3old);
        }

        // Set font
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(80, 270);

        // Show bus data
        if(holdValues == false){
            getdisplay().print(svalue3);                                     // Real value as formated string
        }
        else{
            getdisplay().print(svalue3old);                                  // Old value as formated string
        }
        if(valid3 == true){
            svalue3old = svalue3;                                       // Save the old value
            unit3old = unit3;                                           // Save the old unit
        }

        // ############### Vertical Line ################

        // Vertical line 3 pix
        getdisplay().fillRect(200, 195, 3, 75, commonData->fgcolor);

        // ############### Value 4 ################ - WTemp

        // Show name
        getdisplay().setFont(&Ubuntu_Bold12pt8b);
        getdisplay().setCursor(220, 220);
        getdisplay().print(name4);                           // WTemp

        // Show unit
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(220, 240);
        if(holdValues == false){
            getdisplay().print(unit4);                       // Unit
        }
        else{
            getdisplay().print(unit4old);
        }

        // Set font
        getdisplay().setFont(&DSEG7Classic_BoldItalic20pt7b);
        getdisplay().setCursor(280, 270);

        // Show bus data
        if(holdValues == false){
            getdisplay().print(svalue4);                                     // Real value as formated string
        }
        else{
            getdisplay().print(svalue4old);                                  // Old value as formated string
        }
        if(valid4 == true){
            svalue4old = svalue4;                                       // Save the old value
            unit4old = unit4;                                           // Save the old unit
        }

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageDST810(common);
}/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageDST810(
    "DST810",           // Page name
    createPage,         // Action
    0,                  // Number of bus values depends on selection in Web configuration
    {"DBT","STW","TripLog","Log","WTemp"},      // Bus values we need in the page
    true                // Show display header on/off
);

#endif
