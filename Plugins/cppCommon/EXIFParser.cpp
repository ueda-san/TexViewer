#include "EXIFParser.h"

#include <format>
#include <algorithm>
#include <functional>

using namespace EXIFParser;

typedef struct {
    unsigned short byteOrder;
    unsigned short magic;
    unsigned int offset;
} TIFF_HEADER;

enum Type{
    BYTE      = 1,
    ASCII     = 2,
    SHORT     = 3,
    LONG      = 4,
    RATIONAL  = 5,
    SBYTE     = 6,   //TIFF only
    UNDEFINED = 7,
    SSHORT    = 8,   //TIFF only
    SLONG     = 9,
    SRATIONAL = 10,
    FLOAT     = 11,  //TIFF only
    DOUBLE    = 12,  //TIFF only
    UTF8      = 129, //Exif only
};

EXIFParser::TagOfInterest defaultList[] = {
    { IFD_Root,   256, "ImageWidth",                  PrintFormat::Auto},
    { IFD_Root,   257, "ImageHeight",                 PrintFormat::Auto},
    { IFD_Root,   258, "BitsPerSample",               PrintFormat::Auto},
    { IFD_Root,   259, "Compression",                 PrintFormat::Enum},
    { IFD_Root,   262, "PhotometricInterpretation",   PrintFormat::Enum},
    { IFD_Root,   270, "ImageDescription",            PrintFormat::Auto},
    { IFD_Root,   271, "Manufacturer",                PrintFormat::Auto},
    { IFD_Root,   272, "Model",                       PrintFormat::Auto},
    { IFD_Root,   273, "StripOffsets",                PrintFormat::Auto},
    { IFD_Root,   274, "Orientation",                 PrintFormat::Enum},
    { IFD_Root,   277, "SamplesPerPixel",             PrintFormat::Auto},
    { IFD_Root,   278, "RowsPerStrip",                PrintFormat::Auto},
    { IFD_Root,   279, "StripByteCounts",             PrintFormat::Auto},
    { IFD_Root,   282, "XResolution",                 PrintFormat::Auto},
    { IFD_Root,   283, "YResolution",                 PrintFormat::Auto},
    { IFD_Root,   284, "PlanarConfiguration",         PrintFormat::Auto},
    { IFD_Root,   296, "ResolutionUnit",              PrintFormat::Enum},
    { IFD_Root,   301, "TransferFunction",            PrintFormat::Auto},
    { IFD_Root,   305, "Software",                    PrintFormat::Auto},
    { IFD_Root,   306, "DateTime",                    PrintFormat::Auto},
    { IFD_Root,   315, "Artist",                      PrintFormat::Auto},
    { IFD_Root,   318, "WhitePoint",                  PrintFormat::Auto},
    { IFD_Root,   319, "PrimaryChromaticities",       PrintFormat::Auto},
    { IFD_Root,   513, "JPEGInterchangeFormat",       PrintFormat::Auto},
    { IFD_Root,   514, "JPEGInterchangeFormatLength", PrintFormat::Auto},
    { IFD_Root,   529, "YCbCrCoefficients",           PrintFormat::Auto},
    { IFD_Root,   530, "YCbCrSubSampling",            PrintFormat::Auto},
    { IFD_Root,   531, "YCbCrPositioning",            PrintFormat::Enum},
    { IFD_Root,   532, "ReferenceBlackWhite",         PrintFormat::Auto},
    { IFD_Root, 33432, "Copyright",                   PrintFormat::Auto},

    { IFD_Exif, 33434, "ExposureTime",                PrintFormat::Rational},
    { IFD_Exif, 33437, "FNumber",                     PrintFormat::Double_2f},
    { IFD_Exif, 34850, "ExposureProgram",             PrintFormat::Enum},
    { IFD_Exif, 34852, "SpectralSensitivity",         PrintFormat::Enum},
    { IFD_Exif, 34855, "ISOSpeedRatings",             PrintFormat::Auto}, // PhotographicSensitivity
    { IFD_Exif, 34856, "OECF",                        PrintFormat::Auto},
    { IFD_Exif, 34864, "SensitivityType",             PrintFormat::Enum},
    { IFD_Exif, 34865, "StandardOutputSensitivity",   PrintFormat::Auto},
    { IFD_Exif, 34866, "RecommendedExposureIndex",    PrintFormat::Auto},
    { IFD_Exif, 34867, "ISOSpeed",                    PrintFormat::Auto},
    { IFD_Exif, 34868, "ISOSpeedLatitudeyyy",         PrintFormat::Auto},
    { IFD_Exif, 34869, "ISOSpeedLatitudezzz",         PrintFormat::Auto},
    { IFD_Exif, 36864, "ExifVersion",                 PrintFormat::Version},
    { IFD_Exif, 36867, "DateTimeOriginal",            PrintFormat::Auto},
    { IFD_Exif, 36868, "DateTimeDigitized",           PrintFormat::Auto},
    { IFD_Exif, 36880, "OffsetTime",                  PrintFormat::Auto},
    { IFD_Exif, 36881, "OffsetTimeOriginal",          PrintFormat::Auto},
    { IFD_Exif, 36882, "OffsetTimeDigitized",         PrintFormat::Auto},
    { IFD_Exif, 37121, "ComponentsConfiguration",     PrintFormat::Enum},
    { IFD_Exif, 37122, "CompressedBitsPerPixel",      PrintFormat::Auto},
    { IFD_Exif, 37377, "ShutterSpeedValue",           PrintFormat::ShutterSpeed},
    { IFD_Exif, 37378, "ApertureValue",               PrintFormat::Aperture},
    { IFD_Exif, 37379, "BrightnessValue",             PrintFormat::Brightness},
    { IFD_Exif, 37380, "ExposureBiasValue",           PrintFormat::ExposureBias},
    { IFD_Exif, 37381, "MaxApertureValue",            PrintFormat::MaxAperture},
    { IFD_Exif, 37382, "SubjectDistance",             PrintFormat::Metre},
    { IFD_Exif, 37383, "MeteringMode",                PrintFormat::Enum},
    { IFD_Exif, 37384, "LightSource",                 PrintFormat::Enum},
    { IFD_Exif, 37385, "Flash",                       PrintFormat::Enum},
    { IFD_Exif, 37386, "FocalLength",                 PrintFormat::Millimetre},
    { IFD_Exif, 37396, "SubjectArea",                 PrintFormat::Double_2f},
    { IFD_Exif, 37500, "MakerNote",                   PrintFormat::Auto},
    { IFD_Exif, 37510, "UserComment",                 PrintFormat::Auto},
    { IFD_Exif, 37520, "SubSecTime",                  PrintFormat::Auto},
    { IFD_Exif, 37521, "SubSecTimeOriginal",          PrintFormat::Auto},
    { IFD_Exif, 37522, "SubSecTimeDigitized",         PrintFormat::Auto},
    { IFD_Exif, 37888, "Temperature",                 PrintFormat::Auto},
    { IFD_Exif, 37889, "Humidity",                    PrintFormat::Auto},
    { IFD_Exif, 37890, "Pressure",                    PrintFormat::Auto},
    { IFD_Exif, 37891, "WaterDepth",                  PrintFormat::Auto},
    { IFD_Exif, 37892, "Acceleration",                PrintFormat::Auto},
    { IFD_Exif, 37893, "CameraElevationAngle",        PrintFormat::Auto},
    { IFD_Exif, 40960, "FlashpixVersion",             PrintFormat::Version},
    { IFD_Exif, 40961, "ColorSpace",                  PrintFormat::Enum},
    { IFD_Exif, 40962, "PixelXDimension",             PrintFormat::Auto},
    { IFD_Exif, 40963, "PixelYDimension",             PrintFormat::Auto},
    { IFD_Exif, 40964, "RelatedSoundFile",            PrintFormat::Enum},
    { IFD_Exif, 41483, "FlashEnergy",                 PrintFormat::Auto},
    { IFD_Exif, 41484, "SpatialFrequencyResponse",    PrintFormat::Auto},
    { IFD_Exif, 41486, "FocalPlaneXResolution",       PrintFormat::Double_2f},
    { IFD_Exif, 41487, "FocalPlaneYResolution",       PrintFormat::Double_2f},
    { IFD_Exif, 41488, "FocalPlaneResolutionUnit",    PrintFormat::Enum},
    { IFD_Exif, 41492, "SubjectLocation",             PrintFormat::Auto},
    { IFD_Exif, 41493, "ExposureIndex",               PrintFormat::Auto},
    { IFD_Exif, 41495, "SensingMethod",               PrintFormat::Enum},
    { IFD_Exif, 41728, "FileSource",                  PrintFormat::Enum},
    { IFD_Exif, 41729, "SceneType",                   PrintFormat::Enum},
    { IFD_Exif, 41730, "CFAPattern",                  PrintFormat::Auto},
    { IFD_Exif, 41985, "CustomRendered",              PrintFormat::Enum},
    { IFD_Exif, 41986, "ExposureMode",                PrintFormat::Enum},
    { IFD_Exif, 41987, "WhiteBalance",                PrintFormat::Enum},
    { IFD_Exif, 41988, "DigitalZoomRatio",            PrintFormat::Auto},
    { IFD_Exif, 41989, "FocalLengthIn35mmFilm",       PrintFormat::Millimetre},
    { IFD_Exif, 41990, "SceneCaptureType",            PrintFormat::Enum},
    { IFD_Exif, 41991, "GainControl",                 PrintFormat::Enum},
    { IFD_Exif, 41992, "Contrast",                    PrintFormat::Enum},
    { IFD_Exif, 41993, "Saturation",                  PrintFormat::Enum},
    { IFD_Exif, 41994, "Sharpness",                   PrintFormat::Enum},
    { IFD_Exif, 41995, "DeviceSettingDescription",    PrintFormat::Auto},
    { IFD_Exif, 41996, "SubjectDistanceRange",        PrintFormat::Enum},
    { IFD_Exif, 42016, "ImageUniqueID",               PrintFormat::Auto},
    { IFD_Exif, 42032, "CameraOwnerName",             PrintFormat::Auto},
    { IFD_Exif, 42033, "BodySerialNumber",            PrintFormat::Auto},
    { IFD_Exif, 42034, "LensSpecification",           PrintFormat::Auto},
    { IFD_Exif, 42035, "Lens Maker",                  PrintFormat::Auto},
    { IFD_Exif, 42036, "Lens Model",                  PrintFormat::Auto},
    { IFD_Exif, 42037, "LensSerialNumber",            PrintFormat::Auto},
    { IFD_Exif, 42038, "ImageTitle",                  PrintFormat::Auto},
    { IFD_Exif, 42039, "Photographer",                PrintFormat::Auto},
    { IFD_Exif, 42040, "ImageEditor",                 PrintFormat::Auto},
    { IFD_Exif, 42041, "CameraFirmware",              PrintFormat::Auto},
    { IFD_Exif, 42042, "RAWDevelopingSoftware",       PrintFormat::Auto},
    { IFD_Exif, 42043, "ImageEditingSoftware",        PrintFormat::Auto},
    { IFD_Exif, 42044, "MetadataEditingSoftware",     PrintFormat::Auto},
    { IFD_Exif, 42080, "CompositeImage",              PrintFormat::Enum},
    { IFD_Exif, 42240, "Gamma",                       PrintFormat::Auto},

    { IFD_GPS,      0, "GPSVersion",                  PrintFormat::Version},
    { IFD_GPS,      1, "GPSLatitudeRef",              PrintFormat::Auto},
    { IFD_GPS,      2, "GPSLatitude",                 PrintFormat::LatLon},
    { IFD_GPS,      3, "GPSLongitudeRef",             PrintFormat::Auto},
    { IFD_GPS,      4, "GPSLongitude",                PrintFormat::LatLon},
    { IFD_GPS,      5, "GPSAltitudeRef",              PrintFormat::Enum},
    { IFD_GPS,      6, "GPSAltitude",                 PrintFormat::Metre},
    { IFD_GPS,      7, "GPSTimeStamp",                PrintFormat::Auto},
    { IFD_GPS,      8, "GPSSatellites",               PrintFormat::Auto},
    { IFD_GPS,      9, "GPSStatus",                   PrintFormat::Auto},
    { IFD_GPS,     10, "GPSMeasureMode",              PrintFormat::Auto},
    { IFD_GPS,     11, "GPSDOP",                      PrintFormat::Auto},
    { IFD_GPS,     12, "GPSSpeedRef",                 PrintFormat::Auto},
    { IFD_GPS,     13, "GPSSpeed",                    PrintFormat::Auto},
    { IFD_GPS,     14, "GPSTrackRef",                 PrintFormat::Auto},
    { IFD_GPS,     15, "GPSTrack",                    PrintFormat::Auto},
    { IFD_GPS,     16, "GPSImgDirectionRef",          PrintFormat::Auto},
    { IFD_GPS,     17, "GPSImgDirection",             PrintFormat::Auto},
    { IFD_GPS,     18, "GPSMapDatum",                 PrintFormat::Auto},
    { IFD_GPS,     19, "GPSDestLatitudeRef",          PrintFormat::Auto},
    { IFD_GPS,     20, "GPSDestLatitude",             PrintFormat::Auto},
    { IFD_GPS,     21, "GPSDestLongitudeRef",         PrintFormat::Auto},
    { IFD_GPS,     22, "GPSDestLongitude",            PrintFormat::Auto},
    { IFD_GPS,     23, "GPSDestBearingRef",           PrintFormat::Auto},
    { IFD_GPS,     24, "GPSDestBearing",              PrintFormat::Auto},
    { IFD_GPS,     25, "GPSDestDistanceRef",          PrintFormat::Auto},
    { IFD_GPS,     26, "GPSDestDistance",             PrintFormat::Auto},
    { IFD_GPS,     27, "GPSProcessingMethod",         PrintFormat::Auto},
    { IFD_GPS,     28, "GPSAreaInformation",          PrintFormat::Auto},
    { IFD_GPS,     29, "GPSDateStamp",                PrintFormat::Auto},
    { IFD_GPS,     30, "GPSDifferential",             PrintFormat::Auto},
    { IFD_GPS,     31, "GPSHPositioningError",        PrintFormat::Auto},

    { IFD_InterOP,  1, "InteroperabilityIndex",       PrintFormat::Auto},
    { IFD_InterOP,  2, "InteroperabilityVersion",     PrintFormat::Version},
    { IFD_InterOP,4097, "width?",     PrintFormat::Version},
    { IFD_InterOP,4098, "height?",     PrintFormat::Version},

    // TIFF only?
    { IFD_Root,   316, "HostComputer",                PrintFormat::Auto},
    { IFD_Root,   317, "Differencing Predictor",      PrintFormat::Auto},
    { IFD_Root,   322, "TileWidth",                   PrintFormat::Auto},
    { IFD_Root,   323, "TileLength",                  PrintFormat::Auto},
    { IFD_Root,   324, "TileOffsets",                 PrintFormat::Auto},
    { IFD_Root,   325, "TileByteCounts",              PrintFormat::Auto},
    { IFD_Root,   338, "Associated Alpha Handling",   PrintFormat::Auto}, //PMA?
    { IFD_Root,   339, "Data Sample Format",          PrintFormat::Auto},
    { IFD_Root,   700, "XMLPacket",                   PrintFormat::Auto},
    { IFD_Root, 34675, "InterColorProfile",           PrintFormat::Auto}, //ICC?
    { IFD_Root, 50341, "PrintImageMatching",          PrintFormat::Auto},

    { 0, 0, "", PrintFormat::Auto}, // terminator
};

//------------------------------------------------------------------------------
EXIFParser::TagOfInterest simpleList[] = { // sample. define your own favorite list
    { IFD_Root,   256, "ImageWidth",                  PrintFormat::Auto},
    { IFD_Root,   257, "ImageHeight",                 PrintFormat::Auto},
    { IFD_Root,   282, "XResolution",                 PrintFormat::Auto},
    { IFD_Root,   283, "YResolution",                 PrintFormat::Auto},
    { IFD_Root,   296, "ResolutionUnit",              PrintFormat::Enum},
    { IFD_Root,   258, "BitsPerSample",               PrintFormat::Auto},
    { IFD_Root,   274, "Orientation",                 PrintFormat::Enum},
    { IFD_Root,   306, "DateTime",                    PrintFormat::Auto},

    { IFD_Exif, 37378, "ApertureValue",               PrintFormat::Aperture},
    { IFD_Exif, 33434, "ExposureTime",                PrintFormat::Rational},
    { IFD_Exif, 34855, "ISOSpeedRatings",             PrintFormat::Auto}, // PhotographicSensitivity
    { IFD_Exif, 37380, "ExposureBiasValue",           PrintFormat::ExposureBias},
    { IFD_Exif, 37386, "FocalLength",                 PrintFormat::Millimetre},
    { IFD_Exif, 41989, "FocalLengthIn35mmFilm",       PrintFormat::Millimetre},
    { IFD_Exif, 37385, "Flash",                       PrintFormat::Enum},
    { IFD_Exif, 41986, "ExposureMode",                PrintFormat::Enum},
    { IFD_Exif, 41987, "WhiteBalance",                PrintFormat::Enum},

    { IFD_Root,   270, "ImageDescription",            PrintFormat::Auto},
    { IFD_Root,   305, "Software",                    PrintFormat::Auto},
    { IFD_Root,   315, "Artist",                      PrintFormat::Auto},
    { IFD_Root, 33432, "Copyright",                   PrintFormat::Auto},

    { IFD_Root,   271, "Manufacturer",                PrintFormat::Auto},
    { IFD_Root,   272, "Model",                       PrintFormat::Auto},
    { IFD_Exif, 42035, "Lens Maker",                  PrintFormat::Auto},
    { IFD_Exif, 42036, "Lens Model",                  PrintFormat::Auto},

    { IFD_GPS,      0, "GPSVersion",                  PrintFormat::Version},
    { IFD_GPS,      1, "GPSLatitudeRef",              PrintFormat::Auto},
    { IFD_GPS,      2, "GPSLatitude",                 PrintFormat::LatLon},
    { IFD_GPS,      3, "GPSLongitudeRef",             PrintFormat::Auto},
    { IFD_GPS,      4, "GPSLongitude",                PrintFormat::LatLon},
    { IFD_GPS,      5, "GPSAltitudeRef",              PrintFormat::Enum},
    { IFD_GPS,      6, "GPSAltitude",                 PrintFormat::Metre},
};


//------------------------------------------------------------------------------
// Enum

static const char *Compression[] = {
    "-", "uncompressed", "-", "-", "-", "-", "JPEG compression (thumbnails only)"
};

static const char *PhotometricInterpretation[] = {
    "-", "-", "RGB", "-", "-", "-", "YCbCr"
};

static const char *Orientation[] = {
    "-",
    "Horizontal (normal)",
    "Mirror horizontal",
    "Rotate 180",
    "Mirror vertical",
    "Mirror horizontal and rotate 270 CW",
    "Rotate 90 CW",
    "Mirror horizontal and rotate 90 CW",
    "Rotate 270 CW",
};

static const char *PlanarConfiguration[] = { "-", "chunky format", "planar format" };
static const char *ResolutionUnit[] = { "-", "-", "inch", "cm" };
static const char *YCbCrPositioning[] = { "-", "centered", "co-sited" };

static const char *ExposureProgram[] = {
    "-",
    "Manual",
    "Normal program",
    "Aperture priority",
    "Shutter priority",
    "Creative program",
    "Action program",
    "Portrait mode",
    "Landscape mode",
};

static const char *SensitivityType[] = {
    "-",
    "Standard output sensitivity(SOS)",
    "Recommended exposure index(REI)",
    "ISO speed",
    "Standard output sensitivity(SOS) and recommended exposure index(REI)",
    "Standard output sensitivity(SOS) and ISO speed",
    "Recommended exposure index (REI) and ISO speed",
    "Standard output sensitivity(SOS) and recommended exposure index(REI) and ISO speed",
};

static const char *ComponentsConfiguration[] = { " ", "Y", "Cb", "Cr", "R", "G", "B" };

static const char *SensingMethod[] = {
    "-",
    "Not defined",
    "One-chip color area sensor",
    "Two-chip color area sensor",
    "Three-chip color area sensor",
    "Color sequential area sensor",
    "-",
    "Trilinear sensor",
    "Color sequential linear sensor",
};

static const char *MeteringMode[] = {
    "-",
    "Average",
    "CenterWeightedAverage",
    "Spot",
    "Multi-spot",
    "Pattern",
    "Partial",
};

static const char *LightSource[] = {
    "-",
    "Daylight",
    "Fluorescent",
    "Tungsten (incandescent light)",
    "Flash",
    "-","-","-","-", // 5...8
    "Fine weather",
    "Cloudy weather",
    "Shade",
    "Daylight fluorescent (D 5700 - 7100K)",
    "Day white fluorescent (N 4600 - 5500K)",
    "Cool white fluorescent (W 3800 - 4500K)",
    "White fluorescent (WW 3250 - 3800K)",
    "Warm white fluorescent (L 2600 - 3250K)",
    "Standard light A",
    "Standard light B",
    "Standard light C",
    "D55",
    "D65",
    "D75",
    "D50",
    "ISO studio tungsten",
};

static const char *FlashMode[] = {"-", "ON", "OFF", "Auto" };
static const char *FlashReturn[] = {"N/A", "-", "Not detected", "Detected" };

static const char *FileSource[] = {
    "others",
    "scanner of transparent type",
    "scanner of reflex type",
    "DSC",
};

static const char *SceneType[] = { "-", "A directly photographed image" };
static const char *CustomRendered[] = { "Normal process", "Custom process" };
static const char *ExposureMode[] = {"Auto", "Manual", "Auto bracket" };
static const char *WhiteBalance[] = {"Auto", "Manual" };

static const char *SceneCaptureType[] = {
    "Standard", "Landscape", "Portrait", "Night scene"
};

static const char *GainControl[] = {
    "None", "Low gain up", "High gain up", "Low gain down", "High gain down"
};

static const char *Contrast[] = { "Normal", "Soft", "Hard" };
static const char *Saturation[] = { "Normal", "Low saturation", "High saturation" };
static const char *Sharpness[] = { "Normal", "Soft", "Hard" };

static const char *SubjectDistanceRange[] = {
    "unknown", "Macro", "Close view", "Distant view"
};

static const char *CompositeImage[] = {
    "unknown",
    "non-composite image",
    "General composite image",
    "Composite image captured when shooting",
};

static const char *GPSAltitudeRef[] = {
    "Positive ellipsoidal height",
    "Negative ellipsoidal height",
    "Positive sea level value",
    "Negative sea level value",
};

static const char *GPSDifferential[] = {
    "Measurement without differential correction",
    "Differential correction applied",
};


typedef struct {
    unsigned short tag;
    unsigned short max;
    const char **list;
} TAG_ENUM;

TAG_ENUM enumList[] = {

    {   259,  6, Compression },
    {   262,  6, PhotometricInterpretation },
    {   274,  8, Orientation },
    {   284,  2, PlanarConfiguration },
    {   296,  3, ResolutionUnit },
    { 41488,  3, ResolutionUnit }, // FocalPlaneResolutionUnit (same as ResolutionUnit)
    {   531,  2, YCbCrPositioning },
    { 34850,  8, ExposureProgram },
    { 34864,  7, SensitivityType },
    //{ 37383, , MeteringMode},// 0...6, 255
    //{ 37384, , LightSource}, // 0...24, 255
    //{ 37385, , Flash },      // bitwise
    //{ 40961, , ColorSpace }, // 1 or 0xffff
    { 37121, 6, ComponentsConfiguration },
    { 41495, 8, SensingMethod },
    { 41728, 3, FileSource },
    { 41729, 1, SceneType },
    { 41985, 1, CustomRendered },
    { 41986, 2, ExposureMode },
    { 41987, 1, WhiteBalance },
    { 41990, 3, SceneCaptureType },
    { 41991, 4, GainControl },
    { 41992, 2, Contrast },
    { 41993, 2, Saturation },
    { 41994, 2, Sharpness },
    { 41996, 3, SubjectDistanceRange },
    { 42080, 3, CompositeImage },
    {     5, 3, GPSAltitudeRef },
    {    30, 1, GPSDifferential },
};

static std::string val2enum(unsigned short tag, int val)
{
    std::string ret = "-";
    if (val < 0) return ret;

    switch(tag){
      case 37383: // MeteringMode
        if (val <= 6) return MeteringMode[val];
        if (val == 255) return "other";
        break;
      case 37384: // LightSource
        if (val == 0) return "";
        else if (val <= 24) return LightSource[val];
        else if (val == 255) return "other light source";
        break;
      case 37385: // Flash
        if (val != 0) {
            ret = std::string(FlashMode[(val>>3)&0x03]);
            ret += (val & 0x01)?" (Fired)":" (Not fired)";
            if (val & 0x06) ret += ", return:"+std::string(FlashReturn[(val>>1)&0x03]);
            ret += (val & 0x20)?", No flash function":"";
            ret += (val & 0x40)?", Red-eye reduction":"";
            return ret;
        }else{
            return "";
        }
        break;
      case 40961: // ColorSpace
        if (val == 1) return "sRGB";
        if (val == 0xffff) return "Uncalibrated";
        break;
    }

    for (int i=0; i<sizeof(enumList)/sizeof(TAG_ENUM); i++){
        if (enumList[i].tag == tag){
            if (val <= enumList[i].max){
                return enumList[i].list[val];
            }
            break;
        }
    }

    return ret;
}


//------------------------------------------------------------------------------
static const unsigned char *boyer_moore_search(const unsigned char* haystack, size_t hay_len, const unsigned char* needle, size_t needle_len){
    auto it = std::search(haystack, haystack + hay_len, std::boyer_moore_searcher(needle, needle + needle_len));
    return (it == haystack + hay_len) ? nullptr : it;
}

static const unsigned int str2fourCC(const char *type){
    return (type[0]<<24)+(type[1]<<16)+(type[2]<<8)+type[3];
}

static std::string fourCC2str(int type){
    unsigned char s[5];
    s[0] = static_cast<char>((type >> 24) & 0xFF);
    s[1] = static_cast<char>((type >> 16) & 0xFF);
    s[2] = static_cast<char>((type >> 8)  & 0xFF);
    s[3] = static_cast<char>( type        & 0xFF);
    s[4] = '\0';
    return std::string(reinterpret_cast<char*>(s));
}

bool isFullBox(int type){
    static const std::vector<std::string> boxList = {
        "btrt", "ccid", "clap", "colr", "dinf", "edts", "extr", "fdpa", "fdsa", "feci",
        "frma", "ftyp", "idat", "istm", "mdat", "mdia", "meco", "mfra", "minf", "moof",
        "moov", "mvex", "paen", "pasp", "rcsr", "rinf", "rssr", "rtpx", "schi", "segr",
        "sinf", "snro", "sroc", "stbl", "strd", "strk", "tOD ", "tPAT", "tPMT", "tims",
        "traf", "trak", "tref", "tsro", "tssy", "tsti", "udta",
        //SampleEntry?
        //FreeSpaceBox?
    };
    static const std::vector<std::string> fullBoxList ={
        "bxml", "co64", "cprt", "cslg", "ctts", "dref", "elst", "hdlr", "hmhd", "iinf",
        "iloc", "infe", "ipro", "iref", "leva", "mdhd", "mehd", "meta", "mfhd", "mfro",
        "mvhd", "nmhd", "padb", "pdin", "pitm", "prft", "saio", "saiz", "sbgp", "sdtp",
        "sidx", "smhd", "srpp", "ssix", "stco", "stdp", "stri", "stsc", "stsg", "stsh",
        "stss", "stsz", "stts", "stvi", "stz2", "subs", "tfdt", "tfhd", "tfra", "tkhd",
        "trex", "trun", "tsel", "uri ", "uriI", "url ", "urn ", "vmhd", "xml ",
    };

    std::string typeStr = fourCC2str(type);
    return std::find(fullBoxList.begin(), fullBoxList.end(), typeStr) != fullBoxList.end();
}


//==============================================================================
// public method

ExifParser::ExifParser(TagOfInterest* tagOfInterest)
{
    bigEndian = false;
    result.clear();
    tagList = tagOfInterest;
    Exif_Item_ID = -1;
    iloc_items.clear();
}


//------------------------------------------------------------------------------
int ExifParser::parseExif(const unsigned char *data, size_t len, unsigned short root)
{
    // Exchangeable image file format for digital still cameras: Exif Version 3.0

    if (len < 8) return -1;
    TIFF_HEADER *header = (TIFF_HEADER *)data;
    if (header->byteOrder == 0x4949){ //"II"
        bigEndian = false;
    }else if (header->byteOrder == 0x4d4d){ //"MM"
        bigEndian = true;
    }else{
        return -1;
    }

    const unsigned char *p = (unsigned char *)&header->magic;
    unsigned short magic = getShort(&p);
    if (magic != 42) return -1;

    unsigned int offset = getInt(&p);

    parseIFD(root, data+offset, data, len);

    return result.empty()?0:1; //-1:error, 0:not found, 1:OK
}


//------------------------------------------------------------------------------
int ExifParser::parseJpeg(const wchar_t *file, unsigned short root)
{
    FILE *fp;
    if (_wfopen_s(&fp, file, L"rb") != 0) return -1;
    if (fp == 0) return -1;
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc(len);
    if (data == NULL) {
        fclose(fp);
        return -1;
    }
    size_t bytes_read = fread(data, 1, len, fp);
    fclose(fp);
    int ret = parseJpeg(data, len, root);
    free(data);
    return ret;
}
int ExifParser::parseJpeg(const unsigned char *data, size_t len, unsigned short root)
{
    // JPEG File Interchange Format Version 1.02

    // +0: SOI marker
    // +2: APP1 marker
    // +4: length of field
    // +6: "Exif\0\0"
    // +12: TIFF Image File Header

    bigEndian = true;  // for Jpeg header

    const unsigned char *p = data;
    int SOI = getShort(&p);
    if (SOI != 0xffd8) return -1;

    int found = 0; // 1:found  -1:error/not found
    while(found == 0 && p < data+len){
        int marker = getShort(&p);

        if ((marker & 0xff00) != 0xff00){ // bad marker
            found = -1;
        }else if (marker == 0xffe1){ // APP1
            int size = getShort(&p);
            if (memcmp(p, "Exif\0\0", 6) == 0){
                if (parseExif(p + 6, size, IFD_Root/*IFD_Exif*/) == 1){
                    found = 1;
                }
            }else{
                // TODO: xmp etc
                p += size-2; // size : including the byte count value (2 bytes)
            }
        }else if (marker == 0xffda){ // Start of Scan
            found = -1;
        }else{
            int size = getShort(&p);
            p += size-2; // size : including the byte count value (2 bytes)
        }
    }
    return found;
}


//------------------------------------------------------------------------------
int ExifParser::parseTiff(const wchar_t *file, unsigned short root)
{
    // TIFF (Tagged Image File Format) Revision 6.0

    // +0: TIFF Image File Header

    FILE *fp;
    if (_wfopen_s(&fp, file, L"rb") != 0) return -1;
    if (fp == 0) return -1;
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc(len);
    if (data == NULL) {
        fclose(fp);
        return -1;
    }
    size_t bytes_read = fread(data, 1, len, fp);
    fclose(fp);

    int ret = parseExif(data, len, root);
    free(data);
    return ret;
}


//------------------------------------------------------------------------------
int ExifParser::parseHeif(const wchar_t *file, unsigned short *cs, unsigned short *tf, unsigned short root)
{
    FILE *fp;
    if (_wfopen_s(&fp, file, L"rb") != 0) return -1;
    if (fp == 0) return -1;
    fseek(fp, 0, SEEK_END);
    size_t len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *data = (unsigned char *)malloc(len);
    if (data == NULL) {
        fclose(fp);
        return -1;
    }
    size_t bytes_read = fread(data, 1, len, fp);
    fclose(fp);
    int ret = parseHeif(data, len, cs, tf, root);
    free(data);
    return ret;
}

int ExifParser::parseHeif(const unsigned char *data, size_t len, unsigned short *cs, unsigned short *tf, unsigned short root)
{
    // ISO/IEC 14496-12 ISO base media file format

    //[ftyp]       major_brand:heic
    //[meta]
    //  +[iinf]
    //     +[infe] item_type:Exif
    //  +[iloc]    offset,length
    //[mdat]

    if (len < 8) return -1;

    bigEndian = true;

    int type;
    const unsigned char *body;
    const unsigned char *cur = data;
    if (parseBox(data, len, &cur, cs, tf, &body) == str2fourCC("ftyp")){
        int major_brand = getInt(&body);
        if (major_brand != str2fourCC("heic") &&
            major_brand != str2fourCC("heix") && // 10bit images
            major_brand != str2fourCC("avif") &&
            major_brand != str2fourCC("hevc") &&
            major_brand != str2fourCC("hevx") &&
            major_brand != str2fourCC("heim") &&
            major_brand != str2fourCC("heis") &&
            major_brand != str2fourCC("hevm") &&
            major_brand != str2fourCC("hevs")){
            return -1;
        }
    }

    while(cur < data+len){
        type = parseBox(data, len , &cur, cs, tf, &body);
        _RPTFN(_CRT_WARN, "%s\n", fourCC2str(type).c_str());
    }
    if (Exif_Item_ID >= 0 && !iloc_items.empty()){
        for (ILOC_ITEM item : iloc_items){
            if (item.item_ID == Exif_Item_ID){
                int ofs = 0;
                const unsigned char *p = data + item.extent_offset;
                if ((p[10] == 0x49 && p[11] == 0x49)||(p[10] == 0x4d && p[11] == 0x4d)){
                    ofs = 10;  // TIFF offset(4) + Exif header(6)
                }else if ((p[4] == 0x49 && p[5] == 0x49)||(p[4] == 0x4d && p[5] == 0x4d)){
                    ofs = 4;  // without Exif header  (Nokia?)
                }else if ((p[0] == 0x49 && p[1] == 0x49)||(p[0] == 0x4d && p[1] == 0x4d)){
                    ofs = 0;  //
                    _RPTF0(_CRT_WARN, "no TIFF offset & Exif header ?\n");
                }
                parseExif(data + item.extent_offset + ofs, item.extent_length, IFD_Root/*IFD_Exif*/);
            }
        }
    }

    return (Exif_Item_ID<0)?0:1;
}


//------------------------------------------------------------------------------
std::string ExifParser::getExif(unsigned short ifd, unsigned short tag)
{
    unsigned int key = (ifd<<16) + tag;
    if (result.contains(key)) return result[key];
    return "";
}


//------------------------------------------------------------------------------
std::string ExifParser::dumpExif()
{
    if (result.empty()) return "";

    std::string ret = "";
    if (tagList){
        int j = 0;
        while(1){
            if (tagList[j].parentIFD == 0 && tagList[j].tag == 0) break;
            unsigned int key = (tagList[j].parentIFD<<16) + tagList[j].tag;
            if (result.contains(key)) ret += result[key];
            j++;
        }
    }else{
        for (std::pair<unsigned int, std::string> p : result){
            //unsigned short ptag = p.first >> 16;
            //unsigned short tag = p.first &0xffff;
            ret += p.second;
        }
    }
    return ret;
}


//------------------------------------------------------------------------------
TagOfInterest *ExifParser::getDefaultTagOfInterest()
{
    return defaultList;
}
TagOfInterest *ExifParser::getSimpleTagOfInterest()
{
    return simpleList;
}

std::string ExifParser::formatInt(PrintFormat fmt, unsigned int val, unsigned short tag)
{
    std::string ret;
    if (fmt == PrintFormat::Enum){
        ret = val2enum(tag, val);
    }else if (fmt == PrintFormat::Metre){
        ret = std::format("{}m", val);
    }else if (fmt == PrintFormat::Millimetre){
        ret = std::format("{}mm", val);
    }else{
        ret = std::format("{}", val);
    }
    return ret;
}

std::string ExifParser::formatSInt(PrintFormat fmt, int val, unsigned short tag)
{
    std::string ret;
    if (fmt == PrintFormat::Enum){
        ret = val2enum(tag, val);
    }else if (fmt == PrintFormat::Metre){
        ret = std::format("{}m", val);
    }else if (fmt == PrintFormat::Millimetre){
        ret = std::format("{}mm", val);
    }else{
        ret = std::format("{}", val);
    }
    return ret;
}

std::string ExifParser::formatDouble(PrintFormat fmt, double num, double denom)
{
    std::string ret;
    if (fmt == PrintFormat::Rational){
        ret = std::format("{}/{}", num, denom);
    }else{
        double val = (denom == 0)?0.0:(num/denom);

        if (fmt == PrintFormat::ShutterSpeed){
            ret = std::format("1/{:.2f}", 1.0/std::pow(2.0, -val));
        }else if (fmt == PrintFormat::Aperture){
            ret = std::format("f/{:.2f}", std::pow(2.0, val/2.0));
        }else if (fmt == PrintFormat::Brightness){
            ret = std::format("{:.2f}EV", val);
        }else if (fmt == PrintFormat::ExposureBias){
            ret = std::format("{:.2f}", val);
        }else if (fmt == PrintFormat::MaxAperture){
            ret = std::format("f/{:.2f}", std::pow(2.0, val/2.0));
        }else if (fmt == PrintFormat::Metre){
            ret = std::format("{:.2f}m", val);
        }else if (fmt == PrintFormat::Millimetre){
            ret = std::format("{:.2f}mm", val);
        }else if (fmt == PrintFormat::Double_2f){
            ret = std::format("{:.2f}", val);
        }else{
            ret = std::format("{}", val);
        }
    }
    return ret;
}


//==============================================================================
void ExifParser::parseIFD(unsigned short parentTag, const unsigned char *ifd, const unsigned char *top, size_t len)
{
    const unsigned char *p = ifd;
    short num = getShort(&p);

    for (int i=0; i<num; i++){
        if (p > (top+len-12)) break;
        const unsigned char *dirEnt = p;
        int tag = getShort(&p);
        int type = getShort(&p);
        int count = getInt(&p);
        unsigned int offset = getInt(&p);

        if (tag == IFD_Exif || tag == IFD_GPS || tag == IFD_InterOP){
            if (type == Type::LONG && count == 1){
                parseIFD(tag, top + offset, top, len);
            }else{
                // check me!
            }
        }else{
            //unsigned int key = (parentTag<<16) + tag;
            if (tagList){
                int j = 0;
                while(1){
                    if (tagList[j].parentIFD == 0 && tagList[j].tag == 0) break;
                    //if (parentTag == tagList[j].parentIFD && tag == tagList[j].tag){
                    bool parentMatch = (parentTag == tagList[j].parentIFD) || (parentTag == IFD_Exif && tagList[j].parentIFD == IFD_Root);
                    if (parentMatch && tag == tagList[j].tag){
                        std::string str = dumpDirEntry(dirEnt, top, tagList[j].fmt);
                        unsigned int key = (tagList[j].parentIFD<<16) + tag;
                        result[key] = std::format("{}: {}\n", tagList[j].name, str);
                        break;
                    }
                    j++;
                }
            }else{
                bool found = false;
                for (int i=0; i<sizeof(defaultList)/sizeof(TagOfInterest); i++){
                    //if (defaultList[i].parentIFD == parentTag && defaultList[i].tag == tag){
                    bool parentMatch = (parentTag == defaultList[i].parentIFD) || (parentTag == IFD_Exif && defaultList[i].parentIFD == IFD_Root);
                    if (parentMatch && defaultList[i].tag == tag){
                        std::string str = dumpDirEntry(dirEnt, top, defaultList[i].fmt);
                        unsigned int key = (defaultList[i].parentIFD<<16) + tag;
                        result[key] = std::format("{}: {}\n", defaultList[i].name, str);
                        found = true;
                        break;
                    }
                }
                if (!found){
                    std::string str = dumpDirEntry(dirEnt, top, PrintFormat::Auto);
                    unsigned int key = (parentTag<<16) + tag;
                    result[key] = std::format("{}: {}\n", std::format("TAG[{}, {}]", parentTag, tag), str);
                }
            }
        }
    }
}

std::string ExifParser::dumpDirEntry(const unsigned char *dirEnt, const unsigned char *top, PrintFormat fmt)
{
    const unsigned char *p = dirEnt;
    int tag = getShort(&p);
    int type = getShort(&p);
    int count = getInt(&p);
    //unsigned int offset = getInt(&p);
    //unsigned char *p = (unsigned char *)&ent->offset;
    std::string ret = "";

    switch(type){
      case Type::UNDEFINED:
      case Type::BYTE:
        if (count > 4) p = top + getInt(&p);
        if (fmt == PrintFormat::Enum){
            for (int i=0; i<count; i++){
                if (i != 0) ret +=", ";
                ret += val2enum(tag, *p++);
            }
        }else if (fmt == PrintFormat::Version){
            for (int i=0; i<count; i++){
                if (i != 0) ret +=".";
                char val = (char)*p++;
                if (val <= 9) val+='0';
                ret += std::format("{}", val);
            }
        }else{
            int head = (count > 8)?8:count;
            for (int i=0; i<head; i++, p++){
                if (i != 0) ret +=", ";
                ret += std::format("{}", *p);
            }
            if (count > 8){
                ret += std::format(" ... ({}bytes)", count);
            }
        }
        break;
      case Type::ASCII:
        if (count > 4) p = top + getInt(&p);
        ret += std::string(reinterpret_cast<const char*>(p), strnlen(reinterpret_cast<const char*>(p), count));
        break;

      case Type::SHORT:
        if (count > 2) p = top + getInt(&p);
        for (int i=0; i<count; i++){
            if (i != 0) ret +=", ";
            ret += formatInt(fmt, getShort(&p), tag);
        }
        break;
      case Type::LONG:
        if (count > 1) p = top + getInt(&p);
        for (int i=0; i<count; i++){
            if (i != 0) ret +=", ";
            ret += formatInt(fmt, getInt(&p), tag);
        }
        break;
      case Type::RATIONAL:
        p = top + getInt(&p);
        if (fmt == PrintFormat::LatLon && count == 3){
            double num = getInt(&p);
            double denom = getInt(&p);
            double dd = (denom == 0)?0.0:(num/denom);
            num = getInt(&p);
            denom = getInt(&p);
            double mm = (denom == 0)?0.0:(num/denom);
            num = getInt(&p);
            denom = getInt(&p);
            double ss = (denom == 0)?0.0:(num/denom);
            ret = std::format("{}deg {}' {:.2f}\"", dd, mm, ss);
        }else{
            for (int i=0; i<count; i++){
                if (i != 0) ret +=", ";
                double num = getInt(&p);
                double denom = getInt(&p);
                ret += formatDouble(fmt, num, denom);
            }
        }
        break;
      case Type::SBYTE:
        if (count > 4) p = top + getInt(&p);
        for (int i=0; i<count; i++){
            if (i != 0) ret +=", ";
            ret += std::format("{}", (signed char)*p++);
        }
        break;
      case Type::SSHORT:
        if (count > 2) p = top + getInt(&p);
        for (int i=0; i<count; i++){
            if (i != 0) ret +=", ";
            ret += formatSInt(fmt, getSShort(&p), tag);
        }
        break;
      case Type::SLONG:
        if (count > 1) p = top + getInt(&p);
        for (int i=0; i<count; i++){
            if (i != 0) ret +=", ";
            ret += formatSInt(fmt, getSInt(&p), tag);
        }
        break;
      case Type::SRATIONAL:
        p = top + getInt(&p);
        if (fmt == PrintFormat::LatLon && count == 3){
            double num = getSInt(&p);
            double denom = getSInt(&p);
            double dd = (denom == 0)?0.0:(num/denom);
            num = getSInt(&p);
            denom = getSInt(&p);
            double mm = (denom == 0)?0.0:(num/denom);
            num = getSInt(&p);
            denom = getSInt(&p);
            double ss = (denom == 0)?0.0:(num/denom);
            ret = std::format("{}deg {}' {:.2f}\"", dd, mm, ss);
        }else{
            for (int i=0; i<count; i++){
                if (i != 0) ret +=", ";
                double num = getSInt(&p);
                double denom = getSInt(&p);
                ret += formatDouble(fmt, num, denom);
            }
        }
        break;
      case Type::FLOAT:
        if (count > 1) p = top + getInt(&p);
        for (int i=0; i<count; i++, p+=4){
            if (i != 0) ret +=", ";
            float f;
            std::memcpy(&f, p, sizeof(float));
            ret += formatDouble(fmt, f);
        }
        break;
      case Type::DOUBLE:
        p = top + getInt(&p);
        for (int i=0; i<count; i++, p+=8){
            if (i != 0) ret +=", ";
            double d;
            std::memcpy(&d, p, sizeof(double));
            ret += formatDouble(fmt, d);
        }
        break;
    }

    return ret;
}

inline short ExifParser::getSShort(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    *buf += 2;
    if (bigEndian){
        return (short)((p[0]<<8)+p[1]);
    }else{
        return (short)((p[1]<<8)+p[0]);
    }
}

inline unsigned short ExifParser::getShort(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    *buf += 2;
    if (bigEndian){
        return (unsigned short)((p[0]<<8)+p[1]);
    }else{
        return (unsigned short)((p[1]<<8)+p[0]);
    }
}

inline int ExifParser::getSInt(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    *buf += 4;
    if (bigEndian){
        return (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
    }else{
        return (p[3]<<24)+(p[2]<<16)+(p[1]<<8)+p[0];
    }
}

inline unsigned int ExifParser::getInt(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    *buf += 4;
    if (bigEndian){
        return (unsigned int)((p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3]);
    }else{
        return (unsigned int)((p[3]<<24)+(p[2]<<16)+(p[1]<<8)+p[0]);
    }
}
inline unsigned __int64 ExifParser::getInt64(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    *buf += 8;
    if (bigEndian){
        unsigned __int64 high = (p[0]<<24)+(p[1]<<16)+(p[2]<<8)+p[3];
        unsigned __int64 low  = (p[4]<<24)+(p[5]<<16)+(p[6]<<8)+p[7];
        return (high<<32) | low;
    }else{
        unsigned __int64 high = (p[7]<<24)+(p[6]<<16)+(p[5]<<8)+p[4];
        unsigned __int64 low  = (p[3]<<24)+(p[2]<<16)+(p[1]<<8)+p[0];
        return (high<<32) | low;
    }
}

inline unsigned long long ExifParser::getNBytes(const unsigned char **buf, int n) const{
    const unsigned char *p = *buf;
    *buf += n;
    unsigned long long val = 0;

    if (bigEndian){
        for (int i=0; i<n; i++){
            val = (val << 8) | *p++;
        }
    }else{
        for (int i=0; i<n; i++){
            val |=  ((long long)*p++) << i*8;
        }
    }
    return val;
}

inline float ExifParser::getFloat(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    unsigned int val = getInt(&p);
    *buf += 4;
    float f;
    std::memcpy(&f, &val, sizeof(f)); // avoid strict aliasing violation
    return f;
}

inline double ExifParser::getDouble(const unsigned char **buf) const{
    const unsigned char *p = *buf;
    unsigned char data[8]{};
    if (bigEndian){
        data[0] = p[7]; data[1] = p[6]; data[2] = p[5]; data[3] = p[4];
        data[4] = p[3]; data[5] = p[2]; data[6] = p[1]; data[7] = p[0];
    }else{
        data[0] = p[0]; data[1] = p[1]; data[2] = p[2]; data[3] = p[3];
        data[4] = p[4]; data[5] = p[5]; data[6] = p[6]; data[7] = p[7];
    }
    *buf += 8;
    double d;
    std::memcpy(&d, data, sizeof(d)); // avoid strict aliasing violation
    return d;
}

//------------------------------------------------------------------------------
// for Heif (ISOBMFF)
int ExifParser::parseBox(const unsigned char *data, size_t len, const unsigned char **cur, unsigned short *cs, unsigned short *tf, const unsigned char **outBody)
{
    const unsigned char *p = *cur;
    size_t size = getInt(&p);
    int type = getInt(&p);
    if (size == 1) size = getInt64(&p);
    if (type == str2fourCC("uuid")){
        p+=16;
    }
    int ver_flag = 0;
    if (isFullBox(type)){
        ver_flag = getInt(&p);
    }

    if (outBody) *outBody = p;

    _RPTFN(_CRT_WARN, "%s %x\n", fourCC2str(type).c_str(), *cur - data);

    if (type == str2fourCC("meta")){
        const unsigned char *_cur = p;
        while(_cur < data+len){
            parseBox(data, len, &_cur, cs, tf);
        }
    }else if (type == str2fourCC("iprp") || type == str2fourCC("ipco")){
        const unsigned char *_cur = p;
        while(_cur < data+len){
            parseBox(data, len, &_cur, cs, tf);
        }
    }else if (type == str2fourCC("iinf")){
        unsigned short entry_count = getShort(&p);
        const unsigned char *_cur = p;
        for (int i=0; i<entry_count; i++){
            parseBox(data, len, &_cur, cs, tf);
        }
    }

    if (type == str2fourCC("infe")){  // meta - iinf - infe
        if ((ver_flag >> 24) == 2){
            unsigned short item_ID = getShort(&p);
            unsigned short item_protection_index = getShort(&p);
            unsigned int item_type = getInt(&p);
            if (item_type == str2fourCC("Exif")){
                Exif_Item_ID = item_ID;
                _RPTFN(_CRT_WARN, "infe %s (item_ID:%d pidx:%d) Exif!\n", fourCC2str(item_type).c_str(), item_ID, item_protection_index);
            }else{
                _RPTFN(_CRT_WARN, "infe %s (item_ID:%d pidx:%d)\n", fourCC2str(item_type).c_str(), item_ID, item_protection_index);
            }
        }
    }else if (type == str2fourCC("iloc")){
        unsigned char tmp = *p++;
        unsigned int offset_size = tmp>>4;
        unsigned int length_size = tmp&0x0f;
        tmp = *p++;
        unsigned int base_offset_size = tmp>>4;
        unsigned int index_size = tmp&0x0f;
        unsigned short item_count = getShort(&p);

        for (int i=0; i<item_count; i++){
            unsigned short item_ID = getShort(&p);
            if ((ver_flag >> 24) == 1){
                p++; // reserved 12bit
                unsigned int construction_method = *p++ & 0x0f;
            }
            unsigned short data_reference_index = getShort(&p);
            size_t base_offset = getNBytes(&p, base_offset_size);
            unsigned short extent_count = getShort(&p);
            _RPTFN(_CRT_WARN, "iloc item %d\n", item_ID);
            for (int j=0; j<extent_count; j++){
                size_t extent_index = 0;
                if ((ver_flag >> 24) == 1 && index_size > 0){
                    extent_index = getNBytes(&p, index_size);
                }
                size_t extent_offset = getNBytes(&p, offset_size);
                size_t extent_length = getNBytes(&p, length_size);
                _RPTFN(_CRT_WARN, " extent %d (idx: %d ofs:%x len:%x)\n", j, extent_index, extent_offset, extent_length);
                /*
                if (item_ID == Exif_Item_ID){
                    int ofs = 4+6; // TIFF offset(4) + TIFF header(6)
                    parseExif(data + extent_offset + ofs, extent_length, IFD_Exif);
                }
                */
                ILOC_ITEM item;
                item.item_ID = item_ID;
                item.extent_offset = extent_offset;
                item.extent_length = extent_length;
                iloc_items.push_back(item);
            }
        }
    }else if (type == str2fourCC("colr")){ // meta - iprp - ipco - colr
        int colour_type = getInt(&p);
        if (colour_type == str2fourCC("nclx")){
            unsigned short colour_primaries = getShort(&p);
            unsigned short transfer_characteristics = getShort(&p);
            //unsigned short matrix_coefficients = getShort(&p);
            //unsigned char full_range_flag = *p;
            _RPTFN(_CRT_WARN, " nclx: %d %d !!\n", colour_primaries, transfer_characteristics);
            *cs = colour_primaries;
            *tf = transfer_characteristics;
        }
    }


    if (size == 0){
        *cur = data+len;
    }else{
        *cur += size;
    }

    return type;
}
