#include "OBPDataOperations.h"

// --- general data helper ---------------

// Read all user configuration and convert into compact <OBPUnits> information
tOBPUnits readUserConfig(GwConfigHandler* config, GwLog* logger)
{
    enum class Setting : uint8_t { Length, Distance, Speed, WindSpeed, Temp, Date };

    struct Choice {
        Setting setting; // Unit setting type
        const char* text; // value from configuration
        OBPUnits unit;
    };

    // All allowed values of all settings
    static const Choice choices[] = {
        { Setting::Length, "m", OBPUnits::M }, // [m|ft]
        { Setting::Length, "ft", OBPUnits::FT },

        { Setting::Distance, "m", OBPUnits::M }, // [m|km|nm]
        { Setting::Distance, "km", OBPUnits::KM },
        { Setting::Distance, "nm", OBPUnits::NM },

        { Setting::Speed, "m/s", OBPUnits::MS }, // [m/s|km/h|kn]
        { Setting::Speed, "km/h", OBPUnits::KMH },
        { Setting::Speed, "kn", OBPUnits::KN },

        { Setting::WindSpeed, "m/s", OBPUnits::MS }, // [m/s|km/h|kn|bft]
        { Setting::WindSpeed, "km/h", OBPUnits::KMH },
        { Setting::WindSpeed, "kn", OBPUnits::KN },
        { Setting::WindSpeed, "bft", OBPUnits::BFT },

        { Setting::Temp, "K", OBPUnits::K }, // [K|C|F]
        { Setting::Temp, "C", OBPUnits::C },
        { Setting::Temp, "F", OBPUnits::F },

        { Setting::Date, "DE", OBPUnits::DE }, // [DE|GB|US|ISO]
        { Setting::Date, "GB", OBPUnits::GB },
        { Setting::Date, "US", OBPUnits::US },
        { Setting::Date, "ISO", OBPUnits::ISO },
    };

    // Lambda function to read the user's setting and translate it into a unit
    auto parse = [&](Setting setting, const char* name, const String& key) -> OBPUnits {
        const String value = config->getString(key);
        for (const Choice& choice : choices) {
            if (choice.setting == setting && value == choice.text) {
                return choice.unit;
            }
        }
        LOG_DEBUG(GwLog::ERROR, "ReadUserConfig: Failed to set format for %s, unsupported value: '%s'", name, value.c_str());
        return OBPUnits::UNSET;
    };

    tOBPUnits obpUnits;
    obpUnits.lengthFormat = parse(Setting::Length, "lengthFormat", config->lengthFormat);
    obpUnits.distanceFormat = parse(Setting::Distance, "distanceFormat", config->distanceFormat);
    obpUnits.speedFormat = parse(Setting::Speed, "speedFormat", config->speedFormat);
    obpUnits.windspeedFormat = parse(Setting::WindSpeed, "windspeedFormat", config->windspeedFormat);
    obpUnits.tempFormat = parse(Setting::Temp, "tempFormat", config->tempFormat);
    obpUnits.dateFormat = parse(Setting::Date, "dateFormat", config->dateFormat);

    return obpUnits;
}

// Initialize boat data format as a shorter version of boatValue->getFormat() standard format indicator
BoatDataFmt noteFormat(GwApi::BoatValue* bValue)
{
    const String& format = bValue->getFormat();
    const String& name = bValue->getName();

    if (format == "formatKnots" && (name == "SOG" || name == "STW" || name == "DFT")) {
        return BoatDataFmt::SPEED;
    } else if (format == "formatKnots" && (name == "AWS" || name == "TWS" || name == "MaxAws" || name == "MaxTws")) {
        return BoatDataFmt::WINDSPEED;
    } else if (format == "formatCourse") {
        return BoatDataFmt::COURSE;
    } else if (format == "formatWind") {
        return BoatDataFmt::WIND;
    } else if (format == "formatDepth") {
        return BoatDataFmt::DEPTH;
    } else if (format == "formatRot") {
        return BoatDataFmt::ROTATION;
    } else if (format == "formatLatitude") {
        return BoatDataFmt::LAT;
    } else if (format == "formatLongitude") {
        return BoatDataFmt::LON;
    } else if (format == "mtr2nm") {
        return BoatDataFmt::DISTANCE;
    } else if (format == "formatDop") {
        return BoatDataFmt::DOP;
    } else if (format == "formatXte") {
        return BoatDataFmt::XTE;
    } else if (format == "kelvinToC") {
        return BoatDataFmt::TEMP;
    } else if (format == "formatTime") {
        return BoatDataFmt::TIME;
    } else if (format == "formatDate") {
        return BoatDataFmt::DATE;
    } else if (format == "formatFixed0") {
        return BoatDataFmt::FIXED0;
    } else if (format.startsWith("formatXdr:A")) {
        return BoatDataFmt::XDR_ATTITUDE;
    } else if (format == "formatXdr:P:B") {
        return BoatDataFmt::XDR_PRESSUREBAR;
    } else if (format == "formatXdr:P:P") {
        return BoatDataFmt::XDR_PRESSUREPA;
    } else if (format == "formatXdr:H:P") {
        return BoatDataFmt::XDR_HUMIDITY;
    } else if (format == "formatXdr:U:V") {
        return BoatDataFmt::XDR_VOLT;
    } else if (format == "formatXdr:I:A") {
        return BoatDataFmt::XDR_CURRENT;
    } else if (format == "formatXdr:T:R") {
        return BoatDataFmt::XDR_RPM;
    } else if (format == "formatXdr:V:L") {
        return BoatDataFmt::XDR_LITRE;
    } else if (format == "formatXdr:V:M") {
        return BoatDataFmt::XDR_CBM;
    } else if (!format.isEmpty()) {
        return BoatDataFmt::OTHER;
    } else {
        return BoatDataFmt::NONE;
    }
}
// --- End general data helper ---------------

// --- Class CalibrationData ---------------
CalibrationData::CalibrationData(GwLog* log)
    : logger(log)
{
}

// Look up a supported boat data type; same list as in config.json
const CalibrationData::tInstanceInfo* CalibrationData::findInstanceInfo(const String& name)
{
    static const tInstanceInfo instanceType[] = {
        { "AWA", UnitCategory::Angle },
        { "AWS", UnitCategory::WindSpeed },
        { "COG", UnitCategory::Angle },
        { "DBS", UnitCategory::Length },
        { "DBT", UnitCategory::Length },
        { "HDM", UnitCategory::Angle },
        { "HDT", UnitCategory::Angle },
        { "PRPOS", UnitCategory::Angle },
        { "RPOS", UnitCategory::Angle },
        { "SOG", UnitCategory::Speed },
        { "STW", UnitCategory::Speed },
        { "TWA", UnitCategory::Angle },
        { "TWS", UnitCategory::WindSpeed },
        { "TWD", UnitCategory::Angle },
        { "WTemp", UnitCategory::Temperature },
    };

    for (const tInstanceInfo& info : instanceType) {
        if (name == info.name) {
            return &info;
        }
    }
    return nullptr;
}

// Initial load of calibration data into internal list
// This method is called once at init phase of <obp60task> to read the configuration values
void CalibrationData::readConfig(GwConfigHandler* config)
{
    // Initialize all instance entries
    for (tInstance& instance : instances) {
        instance = tInstance();
    }
    numInstances = 0;

    // Load user format configuration values
    const tOBPUnits obpUnits = readUserConfig(config, logger);

    // Read calibration settings for data instances
    for (int i = 0; i < MAX_CALIBRATION_DATA; i++) {
        const String number(i + 1);
        const String instance = config->getString("calInstance" + number, "---");
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "No calibration data for instance no. %d", i + 1);
            continue;
        }
        const tInstanceInfo* info = findInstanceInfo(instance);
        if (info == nullptr) {
            LOG_DEBUG(GwLog::LOG, "Calibration data for instance no. %d ignored, unsupported type: %s", i + 1, instance.c_str());
            continue;
        }
        double offset = config->getString("calOffset" + number, "").toDouble();
        double slope = config->getString("calSlope" + number, "").toDouble();
        const long smoothSetting = config->getString("calSmooth" + number, "").toInt();

        offset = convertOffsetToSI(info->category, offset, obpUnits);

        if (slope == 0.0) {
            slope = 1.0; // empty or "0" would set every boat value to "0"
        }

        double alpha = 1.0; // no smoothing
        if (smoothSetting > 0) {
            const long s = std::min(smoothSetting, 10L);
            alpha = 1.0 - (0.3 + ((s - 0.01) * (0.95 - 0.3) / (10 - 0.01)));
        }

        // Same instance configured twice: the later entry replaces the earlier one
        tInstance* entry = findInstance(instance);
        if (entry == nullptr) {
            entry = &instances[numInstances++];
        }
        *entry = tInstance();
        entry->name = instance;
        entry->offset = offset;
        entry->slope = slope;
        entry->smooth = alpha;

        LOG_DEBUG(GwLog::LOG, "Calibration data type added: %s, offset: %f, slope: %f, smoothing: %f", instance.c_str(),
            entry->offset, entry->slope, entry->smooth);
    }
}

// Calibrate all configured boat data values
void CalibrationData::handleCalibration(BoatValueList* boatValueList)
{
    for (int i = 0; i < numInstances; i++) {
        tInstance& instance = instances[i];

        // read boat data entry once and remember
        if (instance.bval == nullptr) {
            instance.bval = boatValueList->findValueOrCreate(instance.name);
            if (instance.bval == nullptr) {
                continue;
            }
        }

        calibrate(instance, *instance.bval);
        smooth(instance, *instance.bval);
    }
}

// Calibrate single boat data value
// Return calibrated boat value or false, if no calibration was performed
bool CalibrationData::calibrateInstance(GwApi::BoatValue* boatDataValue)
{
    if (boatDataValue == nullptr) {
        return false;
    }

    tInstance* instance = findInstance(boatDataValue);
    if (instance == nullptr) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", boatDataValue->getName().c_str());
        return false;
    }
    return calibrate(*instance, *boatDataValue);
}

// Smooth single boat data value
// Return smoothed boat value or false, if no smoothing was performed
bool CalibrationData::smoothInstance(GwApi::BoatValue* boatDataValue)
{
    if (boatDataValue == nullptr) {
        return false;
    }

    tInstance* instance = findInstance(boatDataValue);
    if (instance == nullptr) {
        return false;
    }
    return smooth(*instance, *boatDataValue);
}

bool CalibrationData::calibrate(tInstance& instance, GwApi::BoatValue& bval)
{
    instance.isCalibrated = false; // reset calibration flag until properly calibrated

    if (!bval.valid) { // no valid boat data value, so we don't want to apply calibration data
        return false;
    }

    resolveFormat(instance, bval);

    double value = bval.value;
    switch (instance.format) {
    case BoatDataFmt::COURSE: // direction data
        value = WindUtils::to2PI((value * instance.slope) + instance.offset);
        break;
    case BoatDataFmt::WIND: // wind angle data is stored in [0..2PI] format
        value = WindUtils::to2PI((value * instance.slope) + instance.offset);
        break;
    case BoatDataFmt::TEMP:
        value = ((value - 273.15) * instance.slope) + instance.offset + 273.15;
        break;
    default: // also used as long as the format is not known yet (NONE)
        value = (value * instance.slope) + instance.offset;
        break;
    }

    bval.value = value; // update boat data value with calibrated value
    instance.value = value; // keep the calibrated value in the entry
    instance.isCalibrated = true;
    return true;
}

bool CalibrationData::smooth(tInstance& instance, GwApi::BoatValue& bval)
{
    if (!bval.valid) { // no valid boat data value; forget the old one, so that the next valid value starts fresh
        instance.hasLast = false;
        return false;
    }

    if (instance.smooth >= 1.0) { // smoothing is switched off
        instance.value = bval.value;
        return true;
    }

    resolveFormat(instance, bval);

    double value = bval.value;
    if (instance.hasLast) { // first time, only store the current value
        if (instance.format == BoatDataFmt::COURSE || instance.format == BoatDataFmt::WIND) {
            // exponential smoothing along the shortest arc, so that 359 deg -> 1 deg does not pass 180 deg
            value = WindUtils::to2PI(instance.lastValue + (instance.smooth * WindUtils::toPI(value - instance.lastValue)));
        } else {
            value = instance.lastValue + (instance.smooth * (value - instance.lastValue)); // exponential smoothing algorithm
        }
    }
    instance.lastValue = value; // store the new value for next cycle
    instance.hasLast = true;

    bval.value = value; // update boat data value with smoothed value
    instance.value = value;
    return true;
}

// Convert calibration offset from the user format to SI
double CalibrationData::convertOffsetToSI(UnitCategory category, double offset, const tOBPUnits& obpUnits)
{
    switch (category) {
    case UnitCategory::WindSpeed:
        if (obpUnits.windspeedFormat == OBPUnits::KMH) {
            return offset / 3.6; // km/h to m/s
        }
        if (obpUnits.windspeedFormat == OBPUnits::KN) {
            return offset / 1.94384; // kn to m/s
        }
        if (obpUnits.windspeedFormat == OBPUnits::BFT) {
            return std::copysign(0.836 * std::pow(std::fabs(offset), 1.5), offset); // WMO standard empirical formula: v = 0.836 * B^1.5
        }
        return offset; // m/s

    case UnitCategory::Angle:
        return offset * DEG_TO_RAD; // deg to rad

    case UnitCategory::Length:
        if (obpUnits.lengthFormat == OBPUnits::FT) {
            return offset / 3.28084; // ft to m
        }
        return offset; // m

    case UnitCategory::Speed:
        if (obpUnits.speedFormat == OBPUnits::KMH) {
            return offset / 3.6; // km/h to m/s
        }
        if (obpUnits.speedFormat == OBPUnits::KN) {
            return offset / 1.94384; // kn to m/s
        }
        return offset; // m/s

    case UnitCategory::Temperature:
        if (obpUnits.tempFormat == OBPUnits::F) {
            return offset * (5.0 / 9.0); // temperature difference: deg F to K
        }
        return offset; // K or deg C (same difference)
    }
    return offset;
}

CalibrationData::tInstance* CalibrationData::findInstance(const String& name)
{
    for (uint8_t i = 0; i < numInstances; i++) {
        if (instances[i].name == name) {
            return &instances[i];
        }
    }
    return nullptr;
}

CalibrationData::tInstance* CalibrationData::findInstance(const GwApi::BoatValue* bval)
{
    for (uint8_t i = 0; i < numInstances; i++) {
        if (instances[i].bval == bval || instances[i].name == bval->getName()) {
            return &instances[i];
        }
    }
    return nullptr;
}

void CalibrationData::resolveFormat(tInstance& instance, GwApi::BoatValue& bval)
{
    if (instance.format == BoatDataFmt::NONE) { // Instance format stays NONE and is looked up again on the next call.
        instance.format = noteFormat(&bval);
    }
}
// --- End Class CalibrationData ---------------

// --- Class HstryBuf ---------------
HstryBuf::HstryBuf(const String& name, const int size, BoatValueList* boatValues, const bool smooth, GwLog* log)
    : boatDataName(name)
    , smoothing(smooth)
    , simValue(name)
    , logger(log)
{
    hstryBuf.resize(size);
    boatValue = boatValues->findValueOrCreate(boatDataName);
}

void HstryBuf::init(const String& format, const tHistoryParams& params)
{
    if (state == State::Ready) {
        return; // Buffer initialization already complete
    }

    hstryBuf.setMetaData(boatDataName, format, params.hstryUpdFreq, params.mltplr, params.bufferMinVal,
        params.bufferMaxVal);
    hstryMin = params.bufferMinVal;
    hstryMax = params.bufferMaxVal;
    updFreqMs = static_cast<ulong>(params.hstryUpdFreq);
    isTypeAngle = params.isAngle;

    bufUpdateTime = millis() - updFreqMs;

    if (!boatValue->valid) {
        boatValue->value = std::numeric_limits<double>::max(); // mark current value invalid
    }

    // Format is known now; set it once for the reusable simulation value
    simValue.setFormat(boatValue->getFormat());

    // initialize correct variant of buffer for averaging history data
    if (smoothing) {
        if (isTypeAngle) {
            chrtAvgAngle.begin();
        } else {
            chrtAvg.begin();
        }
    }

    state = State::Ready;

    LOG_DEBUG(GwLog::LOG, "HstryBuf::init: metadata added: name: %s, format: %s, frequency: %d, multiplier: %f, min value: %.2f, max value: %.2f",
        boatDataName.c_str(), format.c_str(), params.hstryUpdFreq, params.mltplr, params.bufferMinVal, params.bufferMaxVal);
}

void HstryBuf::add(const double value)
{
    if (!(value >= hstryMin && value <= hstryMax)) {
        return;
    }

    double bufVal = value;
    if (smoothing) {
        bufVal = isTypeAngle ? chrtAvgAngle.reading(value) : chrtAvg.reading(value);
    }

    hstryBuf.add(bufVal);
    // LOG_DEBUG(GwLog::DEBUG, "HstryBuf::add:  name: %s, value: %.3f, value buffer: %.3f", boatDataName.c_str(), value, hstryBuf.getLast());
}

void HstryBuf::handle(const bool useSimuData, CommonData& common)
{
    const unsigned long now = millis();

    if ((now - bufUpdateTime) < updFreqMs) {
        return;
    }
    bufUpdateTime = now;

    if (boatValue->valid) {
        add(boatValue->value);
    } else if (useSimuData) { // add simulated value to history buffer
        simValue.value = boatValue->value;
        simValue.valid = boatValue->valid;
        double simSIValue = formatValue(&simValue, common).value; // simulated value is generated at <formatValue>; here: retreive SI value
        add(simSIValue);
    } else {
        // TODO: add invalid value; this will mark periods of missing data in buffer together with a timestamp
    }
}
// --- End Class HstryBuf ---------------

// --- Class HstryBuffers ---------------
HstryBuffers::HstryBuffers(const int size, BoatValueList* boatValues, GwLog* log)
    : bufferSize(size)
    , boatValueList(boatValues)
    , logger(log)
{
}

// Function-local static; built once on first use, after heap and PSRAM are initialized;shared by all HstryBuffers instances.
const HstryBuffers::ParamMap& HstryBuffers::bufferParams()
{
    static const ParamMap params = {
        //  name { upd [ms], mltplr, min, max, isAngle }
        { "AWA", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "AWD", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "AWS", { 1000, 1000, 0.0, 65.0, false } },
        { "BTW", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "COG", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "DBK", { 1000, 100, 0.0, 650.0, false } },
        { "DBS", { 1000, 100, 0.0, 650.0, false } },
        { "DBT", { 1000, 100, 0.0, 650.0, false } },
        { "DPT", { 1000, 100, 0.0, 650.0, false } },
        { "HDM", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "HDT", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "ROT", { 1000, 10000, -M_PI / 180.0 * 99.0, M_PI / 180.0 * 99.0, true } }, // min/max is -/+ 99 degrees for "rate of turn"
        { "SOG", { 1000, 1000, 0.0, 65.0, false } },
        { "STW", { 1000, 1000, 0.0, 65.0, false } },
        { "TWA", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "TWD", { 1000, 10000, 0.0, M_TWOPI, true } },
        { "TWS", { 1000, 1000, 0.0, 65.0, false } },
        { "VMC", { 1000, 1000, 0.0, 65.0, false } },
        { "VMG", { 1000, 1000, 0.0, 65.0, false } },
        { "WTemp", { 1000, 100, 263.15, 403.15, false } }, // water temp [-10..130] °C
        { "formatXdr:C:K", { 1000, 100, 223.15, 423.15, false } }, // temperature [-50..150] deg celsius
        { "formatXdr:P:B", { 60000, 1000, 0, 65.0, false } }, // pressure [0..65] bar
        { "formatXdr:P:P", { 60000, 0.1, 0, 650000, false } }, // pressure [0..6500] hPa
        { "formatXdr:H:P", { 1000, 100, 0, 100, false } }, // humidity [0..100] percent
        { "formatXdr:I:A", { 1000, 100, 0, 650.0, false } }, // current [0..650] amperes
        { "formatXdr:U:V", { 1000, 1000, 0, 65.0, false } }, // voltage [0..65] volts
        { "formatXdr:T:R", { 1000, 1, 0, 30000, false } }, // tachometer [0..30000] rpm
        { "formatXdr:V:L", { 10000, 100, 0, 650, false } }, // volume [0..650] litres
        { "formatXdr:V:M", { 10000, 10000, 0, 6.50, false } }, // volume [0..6.5] m^3
    };
    return params;
}

const tHistoryParams* HstryBuffers::findNgetParams(const String& key)
{
    const ParamMap& params = bufferParams();
    auto it = params.find(key);
    return (it != params.end()) ? &it->second : nullptr;
}

// Create history buffer for boat data type
void HstryBuffers::addBuffer(const String& name, const bool smooth)
{
    // boat data type not in params list -> not supported for history buffer
    if (!name.startsWith(XDR_PREFIX) && findNgetParams(name) == nullptr) {
        return;
    }
    if (hstryBuffers.find(name) != hstryBuffers.end()) { // buffer for this data type already exists
        return;
    }

    // create buffer only; initialization with metadata can only be done later after boat data have been updated first time
    hstryBuffers[name] = std::unique_ptr<HstryBuf>(new HstryBuf(name, bufferSize, boatValueList, smooth, logger));
    pendingInit = true;

    LOG_DEBUG(GwLog::LOG, "HstryBuffers: new buffer added: name: %s", name.c_str());
}

void HstryBuffers::initBuffers()
{
    bool stillPending = false;

    for (auto& entry : hstryBuffers) {
        HstryBuf& buf = *entry.second;
        if (!buf.isPending()) {
            continue; // buffer metadata is already initialized
        }

        const String format = buf.currentFormat();
        if (format.isEmpty()) { // no boat data available yet; retry on next call
            stillPending = true;
            continue;
        }

        // XDR values are looked up by their format, all others by their name
        const String& key = buf.getName().startsWith(XDR_PREFIX) ? format : buf.getName();
        const tHistoryParams* params = findNgetParams(key);
        if (params == nullptr) {
            buf.markUnsupported(); // format is not supported; skip this buffer permanently
            LOG_DEBUG(GwLog::LOG, "HstryBuffers: unsupported format, buffer disabled: name: %s, format: %s", buf.getName().c_str(), format.c_str());
            continue;
        }

        buf.init(format, *params);
    }
    pendingInit = stillPending;
}

// Handle all registered history buffers
void HstryBuffers::handleHstryBufs(bool useSimuData, CommonData& common)
{
    if (pendingInit) {
        initBuffers(); // not all buffers have been initialized yet
    }

    for (auto& entry : hstryBuffers) {
        HstryBuf& buf = *entry.second;
        if (buf.hasMetaData()) {
            buf.handle(useSimuData, common);
        }
    }
}

RingBuffer<uint16_t>* HstryBuffers::getBuffer(const String& name)
{
    auto it = hstryBuffers.find(name);
    return (it != hstryBuffers.end()) ? &it->second->ringBuffer() : nullptr;
}

const RingBuffer<uint16_t>* HstryBuffers::getBuffer(const String& name) const
{
    auto it = hstryBuffers.find(name);
    return (it != hstryBuffers.end()) ? &it->second->ringBuffer() : nullptr;
}
// --- End Class HstryBuffers ---------------

// --- Class WindUtils --------------
WindUtils::WindUtils(BoatValueList* boatValues, GwLog* log)
{
    twaBVal = boatValues->findValueOrCreate("TWA");
    twsBVal = boatValues->findValueOrCreate("TWS");
    twdBVal = boatValues->findValueOrCreate("TWD");
    awaBVal = boatValues->findValueOrCreate("AWA");
    awsBVal = boatValues->findValueOrCreate("AWS");
    cogBVal = boatValues->findValueOrCreate("COG");
    stwBVal = boatValues->findValueOrCreate("STW");
    sogBVal = boatValues->findValueOrCreate("SOG");
    hdtBVal = boatValues->findValueOrCreate("HDT");
    hdmBVal = boatValues->findValueOrCreate("HDM");
    varBVal = boatValues->findValueOrCreate("VAR");
    btwBVal = boatValues->findValueOrCreate("BTW");

    // boat values which will be calculated here
    awdBVal = boatValues->findValueOrCreate("AWD");
    maxtwsBVal = boatValues->findValueOrCreate("MaxTws");
    vmgBVal = boatValues->findValueOrCreate("VMG");
    vmcBVal = boatValues->findValueOrCreate("VMC");

    awdBVal->setFormat("formatCourse");
    awdBVal->valid = false;
    maxtwsBVal->setFormat("formatKnots");
    maxtwsBVal->valid = false;
    vmgBVal->setFormat("formatKnots");
    vmgBVal->valid = false;
    vmcBVal->setFormat("formatKnots");
    vmcBVal->valid = false;
};

double WindUtils::to2PI(double a)
{
    if (a >= 0.0 && a < M_TWOPI) {
        return a;
    }
    a = fmod(a, M_TWOPI);
    if (a < 0.0) {
        a += M_TWOPI;
    }
    if (a >= M_TWOPI) { // very small values + 2PI rounds to exactly 2PI
        a = 0.0;
    }
    return a;
}

double WindUtils::toPI(double a)
{
    if (a >= -M_PI && a < M_PI) {
        return a;
    }
    a += M_PI;
    a = to2PI(a);
    a -= M_PI;

    return a;
}

double WindUtils::to360(double a)
{
    if (a >= 0.0 && a < 360.0) {
        return a;
    }
    a = fmod(a, 360.0);
    if (a < 0.0) {
        a += 360.0;
    }
    if (a >= 360.0) {
        a = 0.0;
    }
    return a;
}

double WindUtils::to180(double a)
{
    if (a >= -180.0 && a < 180.0) {
        return a;
    }
    a += 180.0;
    a = to360(a);
    a -= 180.0;

    return a;
}

void WindUtils::toCart(double phi, double r, double& x, double& y)
{
    x = r * sin(phi);
    y = r * cos(phi);
}

void WindUtils::toPol(double x, double y, double& phi, double& r)
{
    phi = (M_PI / 2) - atan2(y, x);
    phi = to2PI(phi);
    r = sqrt(x * x + y * y);
}

void WindUtils::addPolar(double phi1, double r1, double phi2, double r2, double& phi, double& r)
{
    double x1, y1, x2, y2;
    toCart(phi1, r1, x1, y1);
    toCart(phi2, r2, x2, y2);
    toPol(x1 + x2, y1 + y2, phi, r);
}

void WindUtils::calcTwdSA(double awd, double aws, double ctw, double stw, double hdt,
    double& twa, double& tws, double& twd)
{
    addPolar(awd, aws, ctw, -stw, twd, tws);
    twa = to2PI(twd - hdt); // for internal storage we normalize wind angle to [0..2PI], as well
}

// calculate HDT from either HDM + VAR, HDM only, or COG
bool WindUtils::calcHDT()
{
    if (hdtBVal->valid) { // HDT already available -> nothing to do
        return true;
    }
    if (hdmBVal->valid) {
        double hdg;
        hdg = hdmBVal->value + (varBVal->valid ? varBVal->value : 0.0); // corrected HDM, or just HDM if VAR is not available
        hdg = to2PI(hdg);
        setValue(hdtBVal, hdg);
        return true;
    }
    if (cogBVal->valid && sogBVal->valid && sogBVal->value >= MIN_SOG) { // COG as fallback, if SOG is not data noise
        setValue(hdtBVal, cogBVal->value);
        return true;
    }
    hdtBVal->valid = false;
    return false;
}

// calculate wind direction (either TWD or AWD) from wind angle (TWA or AWA) and HDT
bool WindUtils::calcWD(const GwApi::BoatValue* windAng, GwApi::BoatValue* windDir)
{
    if (windAng->valid && hdtBVal->valid) {
        double dir;
        dir = windAng->value + hdtBVal->value;
        dir = to2PI(dir);
        setValue(windDir, dir);
        return true;
    }
    windDir->valid = false; // Cannot calculate wind direction WD without valid wind angle WA and HDT
    return false;
}

// calculate full set of true winds (TWA, TWS, TWD) from apparent winds and more data
bool WindUtils::calcTrueWinds()
{
    // cannot calculate true winds without HDT, apparent wind speed and apparent wind direction
    if (!hdtBVal->valid || !awsBVal->valid || !awdBVal->valid) {
        return false;
    }

    const double hdt = hdtBVal->value;
    const bool cogOk = cogBVal->valid && sogBVal->valid && sogBVal->value >= MIN_SOG; // if SOG is data noise, we don't trust COG
    const double ctw = cogOk ? cogBVal->value : hdt;

    double boatSpeed;
    if (stwBVal->valid) {
        boatSpeed = stwBVal->value;
    } else if (sogBVal->valid) {
        boatSpeed = sogBVal->value;
    } else {
        return false; // neither STW nor SOG available
    }

    double twa, tws, twd;
    calcTwdSA(awdBVal->value, awsBVal->value, ctw, boatSpeed, hdt, twa, tws, twd);

    // replace only missing values
    if (!twaBVal->valid) {
        setValue(twaBVal, twa);
    }
    if (!twsBVal->valid) {
        setValue(twsBVal, tws);
    }
    if (!twdBVal->valid) {
        setValue(twdBVal, twd);
    }

    return true;
}

// Set max wind speed
void WindUtils::handleMaxTWS()
{
    if (twsBVal->valid && twsBVal->value > maxTws) {
        maxTws = twsBVal->value;
    }

    // overwrite core gateway value each cycle with own value if that value is larger
    if (maxTws > 0.0 && (!maxtwsBVal->valid || maxTws >= maxtwsBVal->value)) {
        setValue(maxtwsBVal, maxTws);
    }
}

bool WindUtils::calcVMG()
{
    if (stwBVal->valid && twaBVal->valid) {
        double twa = toPI(twaBVal->value); // we store TWA in [0..2PI]
        vmgBVal->value = stwBVal->value * cos(twa);
        vmgBVal->valid = true;
        return true;
    }
    vmgBVal->valid = false;
    return false;
}

bool WindUtils::calcVMC()
{
    if (sogBVal->valid && cogBVal->valid && btwBVal->valid) {
        double diff = cogBVal->value - btwBVal->value;
        diff = toPI(diff);
        vmcBVal->value = sogBVal->value * cos(diff);
        vmcBVal->valid = true;
        return true;
    }
    vmcBVal->valid = false;
    return false;
}

// Calculate missing wind data and add to obp60task boat data list
bool WindUtils::handleWinds(bool calcWinds)
{
    // HDT is prerequisite for everything
    if (!calcHDT()) {
        hdtBVal->valid = false;
        return false;
    }

    // AWD from AWA, if it does not exist yet
    if (!awdBVal->valid) {
        calcWD(awaBVal, awdBVal);
    }

    // TWD from TWA, if it does not exist yet
    if (!twdBVal->valid) {
        calcWD(twaBVal, twdBVal);
    }

    // don't calculate true winds if not set in configuration, or if nothing is missing
    if (!calcWinds || (twaBVal->valid && twsBVal->valid && twdBVal->valid)) {
        return false;
    }

    return calcTrueWinds();
}

// --- End Class WindUtils --------------