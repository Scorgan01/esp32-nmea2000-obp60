#include "OBPDataOperations.h"

// --- Class CalibrationData ---------------
CalibrationData::CalibrationData(GwLog* log)
{
    logger = log;
}

void CalibrationData::readConfig(GwConfigHandler* config)
// Initial load of calibration data into internal list
// This method is called once at init phase of <obp60task> to read the configuration values
{
    std::string instance;
    double offset;
    double slope;
    double smooth;

    String calInstance = "";
    String calOffset = "";
    String calSlope = "";
    String calSmooth = "";

    // Load user format configuration values
    String lengthFormat = config->getString(config->lengthFormat); // [m|ft]
    String distanceFormat = config->getString(config->distanceFormat); // [m|km|nm]
    String speedFormat = config->getString(config->speedFormat); // [m/s|km/h|kn]
    String windspeedFormat = config->getString(config->windspeedFormat); // [m/s|km/h|kn|bft]
    String tempFormat = config->getString(config->tempFormat); // [K|C|F]

    // Read calibration settings for data instances
    for (int i = 0; i < MAX_CALIBRATION_DATA; i++) {
        calInstance = "calInstance" + String(i + 1);
        calOffset = "calOffset" + String(i + 1);
        calSlope = "calSlope" + String(i + 1);
        calSmooth = "calSmooth" + String(i + 1);

        instance = std::string(config->getString(calInstance, "---").c_str());
        if (instance == "---") {
            LOG_DEBUG(GwLog::LOG, "No calibration data for instance no. %d", i + 1);
            continue;
        }

        calibrationMap[instance] = { 0.0f, 1.0f, 1.0f, 0.0f, false };
        offset = (config->getString(calOffset, "")).toDouble();
        slope = (config->getString(calSlope, "")).toDouble();
        smooth = (config->getString(calSmooth, "")).toInt(); // user input is int; further math is done with double

        if (slope == 0.0) {
            slope = 1.0; // eliminate adjustment if user selected "0" -> that would set the calibrated value to "0"
        }

        // Convert calibration values from user input format to internal standard SI format
        if (instance == "AWS" || instance == "TWS") {
            if (windspeedFormat == "m/s") {
                // No conversion needed
            } else if (windspeedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (windspeedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            } else if (windspeedFormat == "bft") {
                offset *= 2 + (offset / 2); // Convert Bft to m/s (approx) -> to be improved
            }

        } else if (instance == "AWA" || instance == "COG" || instance == "HDM" || instance == "HDT" || instance == "PRPOS" || instance == "RPOS" || instance == "TWA" || instance == "TWD") {
            offset *= DEG_TO_RAD; // Convert deg to rad

        } else if (instance == "DBS" || instance == "DBT") {
            if (lengthFormat == "m") {
                // No conversion needed
            } else if (lengthFormat == "ft") {
                offset /= 3.28084; // Convert ft to m
            }

        } else if (instance == "SOG" || instance == "STW") {
            if (speedFormat == "m/s") {
                // No conversion needed
            } else if (speedFormat == "km/h") {
                offset /= 3.6; // Convert km/h to m/s
            } else if (speedFormat == "kn") {
                offset /= 1.94384; // Convert kn to m/s
            }

        } else if (instance == "WTemp") {
            if (tempFormat == "K" || tempFormat == "C") {
                // No conversion needed
            } else if (tempFormat == "F") {
                offset *= 9.0 / 5.0; // Convert °F to K
                slope *= 9.0 / 5.0; // Convert °F to K
            }
        }

        // transform smoothing factor from [0.01..10] to [0.3..0.95] and invert for exponential smoothing formula
        if (smooth <= 0) {
            smooth = 0;
        } else {
            if (smooth > 10) {
                smooth = 10;
            }
            smooth = 0.3 + ((smooth - 0.01) * (0.95 - 0.3) / (10 - 0.01));
        }
        smooth = 1 - smooth;

        calibrationMap[instance].offset = offset;
        calibrationMap[instance].slope = slope;
        calibrationMap[instance].smooth = smooth;
        calibrationMap[instance].isCalibrated = false;
        LOG_DEBUG(GwLog::LOG, "Calibration data type added: %s, offset: %f, slope: %f, smoothing: %f", instance.c_str(),
            calibrationMap[instance].offset, calibrationMap[instance].slope, calibrationMap[instance].smooth);
    }
}

// Handle calibrationMap and calibrate all boat data values
void CalibrationData::handleCalibration(BoatValueList* boatValueList)
{
    GwApi::BoatValue* bValue;

    for (const auto& cMap : calibrationMap) {
        std::string instance = cMap.first.c_str();
        bValue = boatValueList->findValueOrCreate(String(instance.c_str()));

        calibrateInstance(bValue);
        smoothInstance(bValue);
    }
}

// Calibrate single boat data value
// Return calibrated boat value or DBL_MAX, if no calibration was performed
bool CalibrationData::calibrateInstance(GwApi::BoatValue* boatDataValue)
{
    std::string instance = boatDataValue->getName().c_str();
    double offset = 0;
    double slope = 1.0;
    double dataValue = 0;
    std::string format = "";

    // we test this earlier, but for safety reasons ...
    if (calibrationMap.find(instance) == calibrationMap.end()) {
        LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", instance.c_str());
        return false;
    }

    calibrationMap[instance].isCalibrated = false; // reset calibration flag until properly calibrated

    if (!boatDataValue->valid) { // no valid boat data value, so we don't want to apply calibration data
        return false;
    }

    offset = calibrationMap[instance].offset;
    slope = calibrationMap[instance].slope;
    dataValue = boatDataValue->value;
    format = boatDataValue->getFormat().c_str();
    // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: value: %f, format: %s", instance.c_str(), dataValue, format.c_str());

    if (format == "formatWind") { // instance is of type angle
        dataValue = (dataValue * slope) + offset;
        dataValue = WindUtils::to2PI(dataValue); // wind angle data is stored in [0..2PI] format

    } else if (format == "formatCourse") { // instance is of type direction
        dataValue = (dataValue * slope) + offset;
        dataValue = WindUtils::to2PI(dataValue);

    } else if (format == "kelvinToC") { // instance is of type temperature
        dataValue = ((dataValue - 273.15) * slope) + offset + 273.15;

    } else {
        dataValue = (dataValue * slope) + offset;
    }

    boatDataValue->value = dataValue; // update boat data value with calibrated value
    calibrationMap[instance].value = dataValue; // store the calibrated value in the list
    calibrationMap[instance].isCalibrated = true;

    // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s: Offset: %f, Slope: %f, Result: %f", instance.c_str(), offset, slope, calibrationMap[instance].value);
    return true;
}

// Smooth single boat data value
// Return smoothed boat value or DBL_MAX, if no smoothing was performed
bool CalibrationData::smoothInstance(GwApi::BoatValue* boatDataValue)
{
    std::string instance = boatDataValue->getName().c_str();
    double oldValue = 0;
    double dataValue = boatDataValue->value;
    double smoothFactor = 0;

    // we test this earlier, but for safety reason ...
    if (calibrationMap.find(instance) == calibrationMap.end()) {
        // LOG_DEBUG(GwLog::DEBUG, "BoatDataCalibration: %s not in calibration list", instance.c_str());
        return false;
    }

    calibrationMap[instance].isCalibrated = false; // reset calibration flag until properly calibrated

    if (!boatDataValue->valid) { // no valid boat data value, so we don't need to do anything
        return false;
    }

    smoothFactor = calibrationMap[instance].smooth;

    if (lastValue.find(instance) != lastValue.end()) {
        oldValue = lastValue[instance];
        dataValue = oldValue + (smoothFactor * (dataValue - oldValue)); // exponential smoothing algorithm
    }
    lastValue[instance] = dataValue; // store the new value for next cycle; first time, store only the current value and return

    boatDataValue->value = dataValue; // update boat data value with smoothed value
    calibrationMap[instance].value = dataValue; // store the smoothed value in the list
    calibrationMap[instance].isCalibrated = true;

    return true;
}
// --- End Class CalibrationData ---------------

// --- Class HstryBuf ---------------
HstryBuf::HstryBuf(const String& name, const int size, BoatValueList* boatValues, const bool smooth, GwLog* log)
    : logger(log)
    , smoothing(smooth)
    , boatDataName(name)
{
    hstryBuf.resize(size);
    boatValue = boatValues->findValueOrCreate(name);
}

void HstryBuf::init(const String& format, int updFreq, double mltplr, double minVal, double maxVal)
{
    hstryBuf.setMetaData(boatDataName, format, updFreq, mltplr, minVal, maxVal);
    hstryMin = minVal;
    hstryMax = maxVal;
    bufUpdateTime = 0;
    if (!boatValue->valid) {
        boatValue->value = std::numeric_limits<double>::max(); // mark current value invalid
    }

    isTypeAngle = format == "formatCourse" || format == "formatWind" || format == "formatRot";
    // initialize correct variant of buffer for averaging history data
    if (smoothing) {
        if (isTypeAngle) {
            chrtAvgAngle.begin();
        } else {
            chrtAvg.begin();
        }
    }
}

void HstryBuf::add(const double value)
{
    double bufVal = value;

    if (value >= hstryMin && value <= hstryMax) {

        if (smoothing) {
            if (isTypeAngle) {
                bufVal = chrtAvgAngle.reading(value);
            } else {
                bufVal = chrtAvg.reading(value);
            }
        }

        hstryBuf.add(bufVal);
        // LOG_DEBUG(GwLog::DEBUG, "HstryBuf::add:  name: %s, value: %.3f, value buffer: %.3f", hstryBuf.getName(), value, hstryBuf.getLast());
    }
}

void HstryBuf::handle(const bool useSimuData, CommonData& common)
{
    if ((millis() - bufUpdateTime) >= hstryBuf.getUpdFreq()) {

        bufUpdateTime = millis();

        if (boatValue->valid) {
            add(boatValue->value);

        } else if (useSimuData) { // add simulated value to history buffer
            std::unique_ptr<GwApi::BoatValue> tmpBVal; // Temp variable to get formatted and converted data value from OBP60Formatter
            tmpBVal = std::unique_ptr<GwApi::BoatValue>(new GwApi::BoatValue(boatDataName)); // create temporary boat value for retrieval of simulation value
            tmpBVal->setFormat(boatValue->getFormat());
            tmpBVal->value = boatValue->value;
            tmpBVal->valid = boatValue->valid;
            double simSIValue = formatValue(tmpBVal.get(), common).value; // simulated value is generated at <formatValue>; here: retreive SI value
            add(simSIValue);
        } else {
            // TODO: add invalid (DBL_MAX) value; this will mark periods of missing data in buffer together with a timestamp
        }
    }
}
// --- End Class HstryBuf ---------------

// --- Class HstryBuffers ---------------
HstryBuffers::HstryBuffers(const int size, BoatValueList* boatValues, GwLog* log)
    : size(size)
    , boatValueList(boatValues)
    , logger(log) { };

// Create history buffer for boat data type
void HstryBuffers::addBuffer(const String& name, const bool smooth)
{
    if (HstryBuffers::getBuffer(name) != nullptr) { // buffer for this data type already exists
        return;
    }

    auto it = bufferParams.find(name);
    if (it == bufferParams.end() && !name.startsWith("xdr")) { // requested boat data type is not supported in list of <bufferParams>
                                                               // we take any "XDR" type, though
        return;
    }

    // create buffer only; initialization with metadata can only be done later after boat data have been updated first time
    hstryBuffers[name] = std::unique_ptr<HstryBuf>(new HstryBuf(name, size, boatValueList, smooth, logger));

    LOG_DEBUG(GwLog::LOG, "HstryBuffers: new buffer added: name: %s", name);
}

// Handle all registered history buffers
void HstryBuffers::handleHstryBufs(bool useSimuData, CommonData& common)
{
    for (auto& bufMap : hstryBuffers) {
        auto& buf = bufMap.second;

        if (!buf->hasMetaData()) { // meta data initialization for buffer has not been done before

            String valueFormat = boatValueList->findValueOrCreate(buf->boatDataName)->getFormat().c_str();
            // LOG_DEBUG(GwLog::DEBUG, "HstryBuffers: value name: %s, format: %s", boatValueList->findValueOrCreate(buf->boatDataName)->getName().c_str(), valueFormat);

            if (!valueFormat.isEmpty()) {
                String lookupKey = buf->boatDataName;
                if (buf->boatDataName.startsWith("xdr")) { // current boat data is of type "XDR"
                    lookupKey = valueFormat;
                }
                auto it = bufferParams.find(lookupKey);
                if (it == bufferParams.end()) { // requested boat data type is not supported in list of <bufferParams>
                    return;
                }
                HistoryParams params = it->second; // this is the metadata for the new buffer

                int hstryUpdFreq = params.hstryUpdFreq; // Update frequency for history buffers in ms
                double mltplr = params.mltplr; // default multiplier which transforms original <double> value into buffer type format
                double bufferMinVal = params.bufferMinVal; // Min value for this history buffer
                double bufferMaxVal = params.bufferMaxVal; // Max value for this history buffer

                hstryBuffers[buf->boatDataName]->init(valueFormat, hstryUpdFreq, mltplr, bufferMinVal, bufferMaxVal);
                buf->metaDataDefined = true;
                LOG_DEBUG(GwLog::LOG, "HstryBuffers::handleBufs: metadata added: name: %s, format: %s, frequency: %d, multiplier: %f, min value: %.2f, max value: %.2f", buf->boatDataName, valueFormat, hstryUpdFreq,
                    mltplr, bufferMinVal, bufferMaxVal);
            }
        }

        buf->handle(useSimuData, common);
    }
}

RingBuffer<uint16_t>* HstryBuffers::getBuffer(const String& name)
{
    auto it = hstryBuffers.find(name);
    if (it != hstryBuffers.end()) {
        return &it->second->hstryBuf;
    } else {
        return nullptr;
    }
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
    windDir->valid = false;  // Cannot calculate wind direction WD without valid wind angle WA and HDT
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