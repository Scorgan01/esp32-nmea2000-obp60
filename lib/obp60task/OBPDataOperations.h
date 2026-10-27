// Function lib for boat data calibration, history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "Pagedata.h"
#include "obp60task.h"
#include "movingAvg.h"
#include <map>
#include <unordered_map>

// Calibration of boat data values, when user setting available
// supported boat data types are: AWA, AWS, COG, DBS, DBT, HDM, HDT, PRPOS, RPOS, SOG, STW, TWA, TWS, TWD, WTemp
class CalibrationData {
private:
    typedef struct {
        double offset; // calibration offset
        double slope; // calibration slope
        double smooth; // smoothing factor
        double value; // calibrated data value (for future use)
        bool isCalibrated; // is data instance value calibrated? (for future use)
    } tCalibrationData;

    std::unordered_map<std::string, tCalibrationData> calibrationMap; // list of calibration data instances
    std::unordered_map<std::string, double> lastValue; // array for last smoothed value of boat data values
    GwLog* logger;

    static constexpr int8_t MAX_CALIBRATION_DATA = 4; // maximum number of calibration data instances

public:
    CalibrationData(GwLog* log);
    void readConfig(GwConfigHandler* config);
    void handleCalibration(BoatValueList* boatValues); // Handle calibrationMap and calibrate all boat data values
    bool calibrateInstance(GwApi::BoatValue* boatDataValue); // Calibrate single boat data value
    bool smoothInstance(GwApi::BoatValue* boatDataValue); // Smooth single boat data value
};

// Class for a single history buffer of boat values
class HstryBuf {
private:
    RingBuffer<uint16_t> hstryBuf; // Circular buffer to store history values
    movingAvg<double> chrtAvg { 6 }; // Store average of the last 6 chart values if chart gradient shall be smoothed
    movingAvgAngle<double> chrtAvgAngle { 6 }; // Store average of the last 6 chart values (angle data) if chart gradient shall be smoothed
    String boatDataName;
    double hstryMin;
    double hstryMax;
    bool metaDataDefined = false;
    bool smoothing = false;
    bool isTypeAngle = false;
    unsigned long bufUpdateTime;
    GwApi::BoatValue* boatValue;
    GwLog* logger;

    friend class HstryBuffers;

public:
    HstryBuf(const String& name, const int size, BoatValueList* boatValues, const bool smooth, GwLog* log);
    bool hasMetaData() const { return metaDataDefined; };
    void init(const String& format, int updFreq, double mltplr, double minVal, double maxVal);
    void add(double value);
    void handle(bool useSimuData, CommonData& common);
};

// Manage list of history buffers for supported boat data; used by boat data charts
class HstryBuffers {
private:
    std::map<String, std::unique_ptr<HstryBuf>> hstryBuffers;
    int size; // size of all history buffers
    BoatValueList* boatValueList;
    GwLog* logger;

    struct HistoryParams {
        int hstryUpdFreq; // update frequency of history buffer (documentation only)
        double mltplr; // specifies actual value precision being storable:
                       // [10000: 0 - 6.5535 | 1000: 0 - 65.535 | 100: 0 - 655.35 | 10: 0 - 6553.5 | 1: 0 - 65535 | 0.1: 0 - 655350]
        double bufferMinVal; // minimum valid data value
        double bufferMaxVal; // maximum valid data value
    };

    std::map<String, HistoryParams> bufferParams = {
        { "AWA", { 1000, 10000, 0.0, M_TWOPI } },
        { "AWD", { 1000, 10000, 0.0, M_TWOPI } },
        { "AWS", { 1000, 1000, 0.0, 65.0 } },
        { "BTW", { 1000, 10000, 0.0, M_TWOPI } },
        { "COG", { 1000, 10000, 0.0, M_TWOPI } },
        { "DBK", { 1000, 100, 0.0, 650.0 } },
        { "DBS", { 1000, 100, 0.0, 650.0 } },
        { "DBT", { 1000, 100, 0.0, 650.0 } },
        { "DPT", { 1000, 100, 0.0, 650.0 } },
        { "HDM", { 1000, 10000, 0.0, M_TWOPI } },
        { "HDT", { 1000, 10000, 0.0, M_TWOPI } },
        { "ROT", { 1000, 10000, -M_PI / 180.0 * 99.0, M_PI / 180.0 * 99.0 } }, // min/max is -/+ 99 degrees for "rate of turn"
        { "SOG", { 1000, 1000, 0.0, 65.0 } },
        { "STW", { 1000, 1000, 0.0, 65.0 } },
        { "TWA", { 1000, 10000, 0.0, M_TWOPI } },
        { "TWD", { 1000, 10000, 0.0, M_TWOPI } },
        { "TWS", { 1000, 1000, 0.0, 65.0 } },
        { "VMC", { 1000, 1000, 0.0, 65.0 } },
        { "VMG", { 1000, 1000, 0.0, 65.0 } },
        { "WTemp", { 1000, 100, 263.15, 403.15 } }, // water temp [-10..130] °C
        { "formatXdr:C:K", { 1000, 100, 223.15, 423.15 } }, // temperature [-50..150] deg celsius
        { "formatXdr:P:B", { 60000, 1000, 0, 65.0 } }, // pressure [0..65] bar
        { "formatXdr:P:P", { 60000, 0.1, 0, 650000 } }, // pressure [0..6500] hPa
        { "formatXdr:H:P", { 1000, 100, 0, 100 } }, // humidity [0..100] percent
        { "formatXdr:I:A", { 1000, 100, 0, 650.0 } }, // current [0..650] amperes
        { "formatXdr:U:V", { 1000, 1000, 0, 65.0 } }, // voltage [0..65] volts
        { "formatXdr:T:R", { 1000, 1, 0, 30000 } }, // tachometer [0..30000] rpm
        { "formatXdr:V:L", { 10000, 100, 0, 650 } }, // volume [0..650] litres
        { "formatXdr:V:M", { 10000, 10000, 0, 6.50 } }, // volume [0..6.5] m^3
    };

public:
    HstryBuffers(int size, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name, const bool smooth);
    void handleHstryBufs(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>* getBuffer(const String& name);
};

class WindUtils {
private:
    static constexpr double MIN_SOG = 0.1; // SOG below this value (m/s) is assumed to be data noise from GPS sensor

    GwApi::BoatValue *twaBVal, *twsBVal, *twdBVal, *maxtwsBVal;
    GwApi::BoatValue *awaBVal, *awsBVal, *awdBVal;
    GwApi::BoatValue *cogBVal, *stwBVal, *sogBVal, *hdtBVal, *hdmBVal, *varBVal;
    GwApi::BoatValue *vmgBVal, *vmcBVal, *btwBVal;
    double maxTws = 0.0; // own maxTWS value in obp user task; core gateway resets this MaxTws each second if true wind data is not available

    static void setValue(GwApi::BoatValue* bval, double value)
    {
        bval->value = value;
        bval->valid = true;
    }
    bool calcHDT(); // Derive HDT in place from HDT (if available), HDM + VAR, HDM only, or COG; false if not possible
    bool calcWD(const GwApi::BoatValue* windAng, GwApi::BoatValue* windDir); // Calc A/T wind direction; false if [A/T]WA or HDT not valid
    bool calcTrueWinds(); // Calculate true winds from apparent winds; fills only missing true wind values; false if HDT, AWS or AWD not valid

    //    void setMaxWs(const double* wsVal, GwApi::BoatValue* wsMaxValue);

public:
    WindUtils(BoatValueList* boatValues, GwLog* log);

    static double to2PI(double a);
    static double toPI(double a);
    static double to360(double a);
    static double to180(double a);

    static void toCart(double phi, double r, double& x, double& y);
    static void toPol(double x, double y, double& phi, double& r);
    static void addPolar(double phi1, double r1, double phi2, double r2, double& phi, double& r);

    static void calcTwdSA(double awd, double aws, double ctw, double stw, double hdt,
        double& twa, double& tws, double& twd); // True wind direction, speed, angle from apparent wind
    bool handleWinds(bool calcWinds);
    void calcOwnBoatVals()
    {
        handleMaxTWS();
        calcVMG();
        calcVMC();
    }
    void handleMaxTWS();
    bool calcVMG();
    bool calcVMC();
};