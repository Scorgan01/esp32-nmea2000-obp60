// Function lib for boat data calibration, history buffer handling, true wind calculation, and other operations on boat data
#pragma once
#include "OBPRingBuffer.h"
#include "Pagedata.h"
#include "obp60task.h"
#include "movingAvg.h"
#include <map>
#include <unordered_map>
#include "esp_heap_caps.h"

// Represents user settings of OBP data formats
enum class OBPUnits : uint8_t {
    M,      // metre
    FT,     // feet
    KM,     // kilometre
    NM,     // nautical miles
    MS,     // metre/sec
    KMH,    // kilometre/hour
    KN,     // knots
    BFT,    // Beaufort
    K,      // kelvin
    C,      // Celsius
    F,      // Fahrenheit
    DE,     // date format Germany
    GB,     // date GB
    US,     // date US
    ISO,    // date ISO
    UNSET   // unit not specified; should not happen
};

struct tOBPUnits {
    OBPUnits lengthFormat;
    OBPUnits distanceFormat;
    OBPUnits speedFormat;
    OBPUnits windspeedFormat;
    OBPUnits tempFormat;
    OBPUnits dateFormat;
};

// Represents boat data value formats
enum class BoatDataFmt : uint8_t {
    COURSE,
    DATE,
    DEPTH,
    DISTANCE,
    DOP,
    FIXED0,
    LAT,
    LON,
    NONE,
    OTHER,
    POSITION,
    ROTATION,
    SPEED,
    WIND,
    WINDSPEED,
    TEMP,
    TIME,
    XTE,
    XDR_ATTITUDE,
    XDR_CBM,
    XDR_CURRENT,
    XDR_HUMIDITY,
    XDR_LITRE,
    XDR_PRESSUREBAR,
    XDR_PRESSUREPA,
    XDR_RPM,
    XDR_VOLT
};

tOBPUnits readUserConfig(GwConfigHandler* config, GwLog* logger); // Read all user configuration for OBP unit settings


// Calibration of boat data values, when user setting available
// supported boat data types are: AWA, AWS, COG, DBS, DBT, HDM, HDT, PRPOS, RPOS, SOG, STW, TWA, TWS, TWD, WTemp
class CalibrationData {
public:
    explicit CalibrationData(GwLog* log);
    void readConfig(GwConfigHandler* config); // Load calibration data from user configuration
    void handleCalibration(BoatValueList* boatValues); // Calibrate all configured boat data values
    bool calibrateInstance(GwApi::BoatValue* boatDataValue); // Calibrate single boat data value
    bool smoothInstance(GwApi::BoatValue* boatDataValue); // Smooth single boat data value

private:
    struct tInstance {
        GwApi::BoatValue* bval = nullptr;
        String name; // boat data name
        double offset = 0.0; // calibration offset (SI units)
        double slope = 1.0; // calibration slope
        double smooth = 1.0; // weight in exponential smoothing; 1.0 = no smoothing
        BoatDataFmt format = BoatDataFmt::NONE; // short format of boat value
        double lastValue = 0.0; // last smoothed value
        bool hasLast = false; // lastValue is valid
        double value = 0.0; // calibrated data value (for future use)
        bool isCalibrated = false; // is data instance value calibrated? (for future use)
    };

    enum class UnitCategory : uint8_t { WindSpeed, Angle, Length, Speed, Temperature };

    struct tInstanceInfo {
        const char* name; // boat data name
        UnitCategory category;
    };

    static constexpr int8_t MAX_CALIBRATION_DATA = 4; // maximum number of calibration data instances
    std::array<tInstance, MAX_CALIBRATION_DATA> instances;
    uint8_t numInstances = 0;
    GwLog* logger;
//    tOBPUnits obpUnits; // store user settings for OBP data units

    static const tInstanceInfo* findInstanceInfo(const String& name); // supported boat data type
    static bool calibrate(tInstance& instance, GwApi::BoatValue& bval);
    static bool smooth(tInstance& instance, GwApi::BoatValue& bval);
    static double convertOffsetToSI(UnitCategory category, double offset, const tOBPUnits& OBPUnits); // user format input -> SI
    tInstance* findInstance(const String& name);
    tInstance* findInstance(const GwApi::BoatValue* bval);
    static void resolveFormat(tInstance& instance, GwApi::BoatValue& bval);
};

struct tHistoryParams {
    int hstryUpdFreq;    // update frequency of history buffer in ms
    double mltplr;       // specifies actual value precision being storable:
                         // [10000: 0 - 6.5535 | 1000: 0 - 65.535 | 100: 0 - 655.35 | 10: 0 - 6553.5 | 1: 0 - 65535 | 0.1: 0 - 655350]
    double bufferMinVal; // minimum valid data value
    double bufferMaxVal; // maximum valid data value
    bool isAngle;        // angle data: requires circular averaging when smoothing is enabled
};

// Class for a single history buffer of boat values
class HstryBuf {
public:
    HstryBuf(const String& name, int size, BoatValueList* boatValues, bool smooth, GwLog* log);
    HstryBuf(const HstryBuf&) = delete;
    HstryBuf& operator=(const HstryBuf&) = delete;

    void init(const String& format, const tHistoryParams& params); // Complete initialization once format of the boat value is known
    void add(double value);
    void handle(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>& ringBuffer() { return hstryBuf; }
    const RingBuffer<uint16_t>& ringBuffer() const { return hstryBuf; }

    const String& getName() const { return boatDataName; };
    String currentFormat() const { return String(boatValue->getFormat().c_str()); }; // format of the underlying boat value; empty until first data update
    bool hasMetaData() const { return state == State::Ready; }
    bool isPending() const { return state == State::Pending; }
    void markUnsupported() { state = State::Unsupported; }

private:
    enum class State : uint8_t {
        Pending,        // waiting for boat value to become available
        Ready,          // metadata set
        Unsupported     // format not in parameter table; never retried
    };

    RingBuffer<uint16_t> hstryBuf; // Circular buffer to store history values
    movingAvg<double> chrtAvg { 6 }; // Store average of the last 6 chart values if chart gradient shall be smoothed
    movingAvgAngle<double> chrtAvgAngle { 6 }; // Store average of the last 6 chart values (angle data) if chart gradient shall be smoothed

    const String boatDataName;
    bool smoothing;
    double hstryMin = 0.0;
    double hstryMax = 0.0;
    unsigned long updFreqMs = 0;
    unsigned long bufUpdateTime = 0;
    bool isTypeAngle = false;
    State state = State::Pending;

    GwApi::BoatValue* boatValue = nullptr;
    GwApi::BoatValue simValue;
    GwLog* logger = nullptr;
};

// Manage list of history buffers for supported boat data; used by boat data charts
class HstryBuffers {
public:
    HstryBuffers(int bufSize, BoatValueList* boatValues, GwLog* log);
    void addBuffer(const String& name, const bool smooth);
    void handleHstryBufs(bool useSimuData, CommonData& common);
    RingBuffer<uint16_t>* getBuffer(const String& name);
    const RingBuffer<uint16_t>* getBuffer(const String& name) const;

private:
    using ParamMap = std::map<String, tHistoryParams, std::less<String>,
        PSRAMAllocator<std::pair<const String, tHistoryParams>>>;
    static constexpr const char* XDR_PREFIX = "xdr";

    std::map<String, std::unique_ptr<HstryBuf>> hstryBuffers;
    int bufferSize; // size of each history buffer
    bool pendingInit = false; // true while at least one buffer is waiting for metadata
    BoatValueList* boatValueList;
    GwLog* logger = nullptr;

    static const ParamMap& bufferParams(); // Parameter table; built on first use
    static const tHistoryParams* findNgetParams(const String& key); // Look up buffer parameters by key
    void initBuffers(); // Try to initialize all buffers still waiting for metadata
};

class WindUtils {
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
};