#if defined BOARD_OBP60S3 || defined BOARD_OBP40S3

#include "Pagedata.h"
#include "OBP60Extensions.h"
#include "OBPDataOperations.h"
#include "OBPcharts.h"

class PageOneValue : public Page {
private:
    GwLog* logger;

    enum PageMode {
        VALUE,
        BOTH,
        CHART
    };
    enum DisplayMode {
        FULL,
        HALF
    };

    int width; // Screen width
    int height; // Screen height

    bool keylock = false; // Keylock
    PageMode pageMode = VALUE; // Page display mode
    int8_t dataIntv = 1; // Update interval for wind history chart:
                         // (1)|(2)|(3)|(4)|(8) x 240 seconds for 4, 8, 12, 16, 32 min. history chart

    // String lengthformat;
    bool useSimuData;
    bool holdValues;
    String flashLED;
    bool smallDecimals;

    static constexpr int8_t LEFT = 0;
    static constexpr int8_t CENTER = 1;
    static constexpr int8_t RIGHT = 2;

    // Old values for hold function
    String sValue1Old = "";
    String unit1Old = "";

    // Data buffer pointer (owned by HstryBuffers)
    RingBuffer<uint16_t>* dataHstryBuf = nullptr;
    std::unique_ptr<Chart> dataChart = nullptr; // Chart object

    // display data value in display <mode> [FULL|HALF]
    void showData(GwApi::BoatValue* bValue1, DisplayMode mode)
    {
        int nameXoff, nameYoff, unitX, unitY, unitXoff, unitYoff, valueX, valueY, valueXoff, valueYoff;
        const GFXfont *nameFnt, *unitFnt;
        int valueFontSize1, valueFontSize2, valueFontSize3, DsegFontSize;

        if (mode == FULL) { // full size data display
            nameXoff = 0;
            nameYoff = 0;
            nameFnt = &Ubuntu_Bold32pt8b;
            unitX = 380;
            unitY = 100;
            unitXoff = 0;
            unitYoff = 0;
            unitFnt = &Ubuntu_Bold20pt8b;
            valueXoff = 0;
            valueYoff = 0;
            valueFontSize1 = 20;
            valueFontSize2 = 30;
            valueFontSize3 = 60;
        } else { // half size data and chart display
            nameXoff = -10;
            nameYoff = -34;
            nameFnt = &Ubuntu_Bold20pt8b;
            unitX = 10;
            unitY = 121;
            // unitXoff = -295;
            //unitXoff = -280;
            //unitYoff = 21;
            unitFnt = &Ubuntu_Bold12pt8b;
            // value1Xoff = 111;
            valueXoff = 0;
            valueYoff = -119;
            valueFontSize1 = 12;
            valueFontSize2 = 20;
            valueFontSize3 = 42;
        }

        String name1 = xdrDelete(bValue1->getName()); // Value name
        name1 = name1.substring(0, 6); // String length limit for value name
        double value1 = bValue1->value; // Value as double in SI unit
        bool valid1 = bValue1->valid; // Valid information
        String sValue1 = formatValue(bValue1, *commonData).svalue; // Formatted value as string including unit conversion and switching decimal places
        String unit1 = formatValue(bValue1, *commonData).unit; // Unit of value

        // Show name
        getdisplay().setTextColor(commonData->fgcolor);
        getdisplay().setFont(nameFnt);
        getdisplay().setCursor(20 + nameXoff, 100 + nameYoff);
        getdisplay().print(name1); // name

        // Show unit
        String unitName;
        getdisplay().setFont(unitFnt);
        if (holdValues) {
            unitName = unit1Old;
        } else {
            unitName = unit1;
        }
        if (mode == FULL) {
            drawTextRalign(unitX, unitY, unitName);
        } else {
            getdisplay().setCursor(unitX, unitY);
            getdisplay().print(unitName);
        }   

        // Switch font depending on value format and adjust position
        valueX = 380 + valueXoff;
        if (bValue1->getFormat() == "formatLatitude" || bValue1->getFormat() == "formatLongitude") {
            valueY = 180 + valueYoff;
            DsegFontSize = valueFontSize1;
        } else if (bValue1->getFormat() == "formatTime" || bValue1->getFormat() == "formatDate") {
            valueY = 200 + valueYoff;
            DsegFontSize = valueFontSize2;
        } else {
            valueY = 240 + valueYoff;
            DsegFontSize = valueFontSize3;
        }

        // Show bus data
        if (!holdValues || useSimuData) {
            printBoatValue(sValue1, valueX, valueY, RIGHT, DsegFontSize, smallDecimals);
        } else {
            printBoatValue(sValue1Old, valueX, valueY, RIGHT, DsegFontSize, smallDecimals);
        }

        if (valid1 == true) {
            sValue1Old = sValue1; // Save the old value
            unit1Old = unit1; // Save the old unit
        }
    }

public:
    PageOneValue(CommonData& common)
    {
        commonData = &common;
        logger = commonData->logger;
        LOG_DEBUG(GwLog::LOG, "Instantiate PageOneValue");

        width = getdisplay().width(); // Screen width
        height = getdisplay().height(); // Screen height
        getdisplay().setTextWrap(false);

        // Get config data
        useSimuData = commonData->config->getBool(commonData->config->useSimuData);
        holdValues = commonData->config->getBool(commonData->config->holdvalues);
        flashLED = commonData->config->getString(commonData->config->flashLED);
        smallDecimals = commonData->config->getBool(commonData->config->smallDecimals);
    }

    virtual void setupKeys()
    {
        Page::setupKeys();

#if defined BOARD_OBP60S3
        constexpr int ZOOM_KEY = 4;
#elif defined BOARD_OBP40S3
        constexpr int ZOOM_KEY = 1;
#endif

        if (dataHstryBuf) { // show "Mode" key only if chart-supported boat data type is available
            commonData->keydata[0].label = "MODE";
            if (pageMode != VALUE) { // show "ZOOM" key only if chart is visible
                commonData->keydata[ZOOM_KEY].label = "ZOOM";
            } else {
                commonData->keydata[ZOOM_KEY].label = "";
            }
        } else {
            commonData->keydata[0].label = "";
            commonData->keydata[ZOOM_KEY].label = "";
        }
    }

    // Key functions
    virtual int handleKey(int key)
    {
        if (dataHstryBuf) { // if boat data type supports charts

            // Set page mode: value | value/half chart | full chart
            if (key == 1) {
                switch (pageMode) {
                case VALUE:
                    pageMode = BOTH;
                    break;
                case BOTH:
                    pageMode = CHART;
                    break;
                case CHART:
                    pageMode = VALUE;
                    break;
                }
                setupKeys(); // Adjust key definition depending on <pageMode> and chart-supported boat data type
                return 0; // Commit the key
            }

            // Set time frame to show for chart
#if defined BOARD_OBP60S3
            if (key == 5  && pageMode != VALUE) {
#elif defined BOARD_OBP40S3
            if (key == 2  && pageMode != VALUE) {
#endif
                if (dataIntv == 1) {
                    dataIntv = 2;
                } else if (dataIntv == 2) {
                    dataIntv = 3;
                } else if (dataIntv == 3) {
                    dataIntv = 4;
                } else {
                    dataIntv = 1;
                }
                return 0; // Commit the key
            }
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
#ifdef BOARD_OBP60S3
        // Clear optical warning
        if (flashLED == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }
#endif
        // buffer initialization will fail, if page is default page, because <displayNew> is not executed at system start for default page
        if (!dataChart) { // Create chart objects if they don't exist

            GwApi::BoatValue* bValue1 = pageData.values[0]; // Page boat data element
            String bValName1 = bValue1->getName(); // Value name
            String bValFormat = bValue1->getFormat(); // Value format

            dataHstryBuf = pageData.hstryBuffers->getBuffer(bValName1);

            if (dataHstryBuf) {
                dataChart.reset(new Chart(*dataHstryBuf, *commonData, useSimuData));
                LOG_DEBUG(GwLog::DEBUG, "PageOneValue: Created chart object for %s", bValName1);
            } else {
                LOG_DEBUG(GwLog::DEBUG, "PageOneValue: No chart object available for %s", bValName1);
            }
        }

        setupKeys(); // Adjust key definition depending on <pageMode> and chart-supported boat data type
    }

    int displayPage(PageData& pageData)
    {
        LOG_DEBUG(GwLog::LOG, "Display PageOneValue");

        // Get boat value for page
        GwApi::BoatValue* bValue1 = pageData.values[0]; // Page boat data element

        // Optical warning by limit violation (unused)
        if (String(flashLED) == "Limit Violation") {
            setBlinkingLED(false);
            setFlashLED(false);
        }

        if (bValue1 == NULL)
            return PAGE_OK; // no data, no page to display

        LOG_DEBUG(GwLog::DEBUG, "PageOneValue: printing %s, %.3f", bValue1->getName().c_str(), bValue1->value);

        // Draw page
        //***********************************************************

        displaySetPartialWindow(0, 0, width, height); // Set partial update

        if (dataChart) { // Check only if dataChart object exist^s at all
            if (!dataChart->isValid()) {
                dataChart->init(); // try late initialization if chart object could not be properly initialized earlier due to missing boat data
            }
        }

        if (pageMode == VALUE || dataHstryBuf == nullptr) {
            // show only data value; ignore other pageMode options if no chart supported boat data history buffer is available
            showData(bValue1, FULL);

        } else if (pageMode == CHART) { // show only data chart
            if (dataChart) {
                dataChart->showChrt(Chart::HORIZONTAL, Chart::FULL_SIZE, dataIntv, Chart::PRNT_NAME, Chart::PRNT_VALUE, *bValue1);
            }

        } else if (pageMode == BOTH) { // show data value and chart
            showData(bValue1, HALF);
            if (dataChart) {
                dataChart->showChrt(Chart::HORIZONTAL, Chart::HALF_SIZE_RIGHT_BOTTOM, dataIntv, Chart::NO_PRNT_NAME, Chart::NO_PRNT_VALUE, *bValue1);
            }
        }

        return PAGE_UPDATE;
    };
};

static Page* createPage(CommonData& common)
{
    return new PageOneValue(common);
}

/**
 * with the code below we make this page known to the PageTask
 * we give it a type (name) that can be selected in the config
 * we define which function is to be called
 * and we provide the number of user parameters we expect
 * this will be number of BoatValue pointers in pageData.values
 */
PageDescription registerPageOneValue(
    "OneValue", // Page name
    createPage, // Action
    1, // Number of bus values depends on selection in Web configuration
    true // Show display header on/off
);

#endif
