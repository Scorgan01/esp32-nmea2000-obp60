#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageSolar : public Page
{
    static constexpr int8_t LEFT = 0;
    static constexpr int8_t CENTER = 1;
    static constexpr int8_t RIGHT = 2;

public:
    PageSolar(CommonData &common){
        commonData = &common;
        common.logger->logDebug(GwLog::LOG,"Instantiate PageSolar");
    }
    virtual int handleKey(int key){
        // Code for keylock
        if(key == 11){
            commonData->keylock = !commonData->keylock;
            return 0;                   // Commit the key
        }
        return key;
    }

    int displayPage(PageData &pageData){
        GwConfigHandler *config = commonData->config;
        GwLog *logger = commonData->logger;
        
        // Get config data
        bool simulation = config->getBool(config->useSimuData);
        bool holdvalues = config->getBool(config->holdvalues);
        String flashLED = config->getString(config->flashLED);
        String batVoltage = config->getString(config->batteryVoltage);
        int solPower = config->getInt(config->solarPower);
        String backlightMode = config->getString(config->backlight);
        String powerSensor = config->getString(config->usePowSensor2);
        bool smallDecimals = config->getBool(config->smallDecimals);

        double value1 = 0;  // Solar voltage
        double value2 = 0;  // Solar current
        double value3 = 0;  // Solar output power
        double valueTrend = 0;  // Average over 10 values
        int solPercentage = 0;  // Solar load
        
        // Get voltage value
        String name1 = "VSol";

        // Get raw value for trend indicator
        if(powerSensor != "off"){
            value1 = commonData->data.solarVoltage;  // Use voltage from external sensor
        }
        else{
            value1 = commonData->data.batteryVoltage; // Use internal voltage sensor
        }
        value2 = commonData->data.solarCurrent;
        value3 = commonData->data.solarPower;
        solPercentage = value3 * 100 / (double)solPower;    // Load value
        // Limits for battery level
        if(solPercentage < 0) solPercentage = 0;
        if(solPercentage > 99) solPercentage = 99;

        bool valid1 = true;

        // Optical warning by limit violation
        if(String(flashLED) == "Limit Violation"){
            // Over voltage
            if(value1 > 14.8 && batVoltage == "12V"){
                setBlinkingLED(true);
            }
            if(value1 <= 14.8 && batVoltage == "12V"){
                setBlinkingLED(false);
            }
            if(value1 > 29.6 && batVoltage == "24V"){
                setBlinkingLED(true);
            }
            if(value1 <= 29.6 && batVoltage == "24V"){
                setBlinkingLED(false);
            }     
        }
        
        // Logging voltage value
        LOG_DEBUG(GwLog::LOG,"Drawing at PageSolar, Type:%iW %s:=%f", solPower, name1.c_str(), value1);

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update

        getdisplay().setTextColor(commonData->fgcolor);

        // Show name
        getdisplay().setFont(&Ubuntu_Bold20pt8b);
        getdisplay().setCursor(10, 65);
        getdisplay().print("Solar");

        // Show voltage type
        int bvoltage = 0;
        if(String(batVoltage) == "12V") bvoltage = 12;
        else bvoltage = 24;
        printBoatValue(String(bvoltage), 10, 140, LEFT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        getdisplay().print("V");

        // Show solar power
        String svalueSolPower = (solPower <= 999) ? String(solPower) : String(float(solPower/1000.0), 1);
        printBoatValue(svalueSolPower, 10, 200, LEFT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        if(solPower <= 999) getdisplay().print("W");
        if(solPower > 999) getdisplay().print("kW");

        // Show info
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(10, 235);
        getdisplay().print("Installed");
        getdisplay().setCursor(10, 255);
        getdisplay().print("Solar Modul");

        // Show solar panel
        solarGraphic(150, 45, commonData->fgcolor, commonData->bgcolor);

        // Show load level in percent
        printBoatValue(String(solPercentage), 150, 200, LEFT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        getdisplay().print("%");
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(150, 235);
        getdisplay().print("Load");

        // Show sensor type info
        String i2cAddr = "";
        getdisplay().setFont(&Ubuntu_Bold8pt8b);
        getdisplay().setCursor(270, 60);
        if(powerSensor == "off") getdisplay().print("Internal");
        if(powerSensor == "INA219"){
            getdisplay().print("INA219");
            i2cAddr = " (0x" + String(INA219_I2C_ADDR2, HEX) + ")";
        }
        if(powerSensor == "INA226"){
            getdisplay().print("INA226");
            i2cAddr = " (0x" + String(INA226_I2C_ADDR2, HEX) + ")";
        }
        getdisplay().print(i2cAddr);
        getdisplay().setCursor(270, 80);
        getdisplay().print("Sensor Modul");

        // Reading bus data or using simulation data
        String svalue1;
        if(valid1 == true || holdvalues == true){
            svalue1 = formatValue(value1, String("formatXdr:U:V"), *commonData);
        } else {
            svalue1 = "---"; // Missing bus data
        }
        printBoatValue(svalue1, 355, 140, RIGHT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        getdisplay().print("V");

        // Show actual current in A
        String svalue2;
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            svalue2 = formatValue(value2, String("formatXdr:I:A"), *commonData);
        }
        else{
            svalue2 = "---";
        }
        printBoatValue(svalue2, 355, 200, RIGHT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        getdisplay().print("A");

        // Show actual consumption in W
        String svalue3;
        if((powerSensor == "INA219" || powerSensor == "INA226") && simulation == false){
            svalue3 = formatValue(value3, String("formatXdr:G:"), *commonData);
        }
        else{
            svalue3 = "---";
        }
        printBoatValue(svalue3, 355, 260, RIGHT, 20, smallDecimals);
        getdisplay().setFont(&Ubuntu_Bold16pt8b);
        getdisplay().print("W");

        return PAGE_UPDATE;
    };
};

static Page *createPage(CommonData &common){
    return new PageSolar(common);
}
/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect (0 here)
 * and will will provide the names of the fixed values we need
 */
PageDescription registerPageSolar(
    "Solar",        // Name of page
    createPage,     // Action
    0,              // Number of bus values depends on selection in Web configuration
    {},             // Names of bus values undepends on selection in Web configuration (refer GwBoatData.h)
    true            // Show display header on/off
);

#endif
