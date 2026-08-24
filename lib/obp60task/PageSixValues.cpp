#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"

class PageSixValues : public Page {
private:
    GwLog* logger;
    GwConfigHandler* config;

    bool holdValues;
    String flashLED;
    String backlightMode;

    static constexpr int8_t LEFT = 0;
    static constexpr int8_t CENTER = 1;
    static constexpr int8_t RIGHT = 2;
    bool smallDecimals;

    static constexpr int SixValues_x1 = 5;
    static constexpr int SixValues_DeltaX = 200;
    static constexpr int SixValues_y1 = 22;
    static constexpr int SixValues_DeltaY = 83;

    static constexpr int HowManyValues = 6;
    // Old values for hold function
    String OldDataText[HowManyValues] = { "", "", "", "", "", "" };
    String OldDataUnit[HowManyValues] = { "", "", "", "", "", "" };

public:
    PageSixValues(CommonData& common)
    {
        commonData = &common;
        config = commonData->config;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageSixValues");

        // Get config data
        holdValues = config->getBool(config->holdvalues);
        flashLED = config->getString(config->flashLED);
        backlightMode = config->getString(config->backlight);
        smallDecimals = config->getBool(config->smallDecimals);
    }

    virtual int handleKey(int key)
    {
        // Code for keylock
        if (key == 11) {
            commonData->keylock = !commonData->keylock;
            return 0; // Commit the key
        }
        return key;
    }

    int displayPage(PageData& pageData)
    {
        GwApi::BoatValue* bvalue;
        String DataName[HowManyValues];
        double DataValue[HowManyValues];
        bool DataValid[HowManyValues];
        String DataText[HowManyValues];
        String DataUnit[HowManyValues];
        String DataFormat[HowManyValues];
        int8_t DsegFontSize = 0;

        for (int i = 0; i < HowManyValues; i++) {
            bvalue = pageData.values[i];
            DataName[i] = xdrDelete(bvalue->getName());
            DataName[i] = DataName[i].substring(0, 6); // String length limit for value name
            DataValue[i] = bvalue->value; // Value as double in SI unit
            DataValid[i] = bvalue->valid;
            DataText[i] = formatValue(bvalue, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
            DataUnit[i] = formatValue(bvalue, *commonData).unit;
            DataFormat[i] = bvalue->getFormat(); // Unit of value
        }

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (bvalue == NULL)
            return PAGE_OK;

        // Draw page
        //***********************************************************

        // Set display in partial refresh mode
        displaySetPartialWindow(0, 0, getdisplay().width(), getdisplay().height()); // Set partial update
        getdisplay().setTextColor(commonData->fgcolor);

        for (int i = 0; i < (HowManyValues / 2); i++) {
            if (i < (HowManyValues / 2) - 1) { // Don't draw horizontal line after last line of values
                // Horizontal line 3 px
                getdisplay().fillRect(0, SixValues_y1 + (i + 1) * SixValues_DeltaY, 400, 3, commonData->fgcolor);
            }

            for (int j = 0; j < 2; j++) {
                int ValueIndex = i * 2 + j;
                int x0 = SixValues_x1 + j * SixValues_DeltaX;
                int y0 = SixValues_y1 + i * SixValues_DeltaY;
                LOG_DEBUG(GwLog::DEBUG, "Drawing at PageSixValue: %d %s %f %s %s", ValueIndex, DataName[ValueIndex], DataValue[ValueIndex], DataUnit[ValueIndex], DataFormat[ValueIndex]);

                // Show name
                getdisplay().setFont(&Ubuntu_Bold12pt8b);
                getdisplay().setCursor(x0, y0 + 25);
                getdisplay().print(DataName[ValueIndex]); // Page name

                // Show unit
                getdisplay().setFont(&Ubuntu_Bold8pt8b);
                if (holdValues == false) {
                    drawTextRalign(x0 + 187, y0 + 19, DataUnit[ValueIndex]); // Unit
                } else {
                    drawTextRalign(x0 + 187, y0 + 19, OldDataUnit[ValueIndex]);
                }

                // Set font size depending on data type
                if (DataFormat[ValueIndex] == "formatLatitude" || DataFormat[ValueIndex] == "formatLongitude") {
                    DsegFontSize = 12;
                } else if (DataFormat[ValueIndex] == "formatTime" || DataFormat[ValueIndex] == "formatDate") {
                    DsegFontSize = 16;
                }
                else if (DataFormat[ValueIndex] == "formatXdr:T:R") { // RPM
                    DsegFontSize = 20;
                }
                else {
                    DsegFontSize = 26;
                }

                // Show bus data value
                if (holdValues == false) {
                      printBoatValue(DataText[ValueIndex], x0 + 187, y0 + 79, RIGHT, DsegFontSize, smallDecimals);
                } else {
                      printBoatValue(OldDataText[ValueIndex], x0 + 187, y0 + 79, RIGHT, DsegFontSize, smallDecimals);
                }

                if (DataValid[ValueIndex] == true) {
                    OldDataText[ValueIndex] = DataText[ValueIndex]; // Save the old value
                    OldDataUnit[ValueIndex] = DataUnit[ValueIndex]; // Save the old unit
                }
            }
            // Vertical line 3 px
            getdisplay().fillRect(SixValues_x1 + SixValues_DeltaX - 8, SixValues_y1 + i * SixValues_DeltaY, 3, SixValues_DeltaY, commonData->fgcolor);
        }

        return PAGE_UPDATE;
    };
};
static Page* createPage(CommonData& common)
{
    return new PageSixValues(common);
} /**
   * with the code below we make this page known to the PageTask
   * we give it a type (name) that can be selected in the config
   * we define which function is to be called
   * and we provide the number of user parameters we expect
   * this will be number of BoatValue pointers in pageData.values
   */
PageDescription registerPageSixValues(
    "SixValues", // Page name
    createPage, // Action
    6, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
