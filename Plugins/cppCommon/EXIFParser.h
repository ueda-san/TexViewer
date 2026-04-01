#pragma once

#include <string>
#include <vector>
#include <map>

namespace EXIFParser {

    static const unsigned short IFD_Root = 0;
    static const unsigned short IFD_Exif = 34665;
    static const unsigned short IFD_GPS = 34853;
    static const unsigned short IFD_InterOP = 40965;

    enum class PrintFormat{
        Auto = 0,
        Enum,         // FlashMode:1 -> FlashMode:ON
        Double,       // 3.14159265
        Double_2f,    // 3.14
        Rational,     // "31415/100"
        LatLon,       // 00,11,22 -> 00deg 11' 22"
        Version,      // 48,49,50,51 -> "0.1.2.3"
        Metre,        // 12.3456 -> 12.34m
        Millimetre,   // 12.3456 -> 12.34mm
        ShutterSpeed, // APEX "7191/1000" -> "1/146" (1.0/pow(2.0, -val))
        Aperture,     // APEX "4971/1000" -> "f/5.6"
        Brightness,   // APEX "363/100"   -> "3.63EV"
        ExposureBias, // APEX "-1/1"      -> "-1EV"
        MaxAperture,  // APEX "3562/1000" -> "f/3.5"
    };

    typedef struct {
        unsigned short parentIFD;
        unsigned short tag;
        const char *name;
        PrintFormat fmt;
    } TagOfInterest;


    //------------------------------------------------------------------------------
    class ExifParser
    {
    public:
        ExifParser(TagOfInterest* tagOfInterest=0);

        int parseExif(const unsigned char *exif, size_t len, unsigned short root=IFD_Root);

        int parseTiff(const wchar_t *file, unsigned short root=IFD_Root);
        int parseJpeg(const wchar_t *file, unsigned short root=IFD_Root);
        int parseJpeg(const unsigned char *data, size_t len, unsigned short root=IFD_Root);
        int parseHeif(const wchar_t *file, unsigned short *cs, unsigned short *tf, unsigned short root=IFD_Root);
        int parseHeif(const unsigned char *data, size_t len, unsigned short *cs, unsigned short *tf, unsigned short root=IFD_Root);

        std::string getExif(unsigned short ifd, unsigned short tag);
        std::string dumpExif();

        static TagOfInterest *getDefaultTagOfInterest();
        static TagOfInterest *getSimpleTagOfInterest();
        static std::string formatInt(PrintFormat fmt, unsigned int val, unsigned short tag=0);
        static std::string formatSInt(PrintFormat fmt, int val, unsigned short tag=0);
        static std::string formatDouble(PrintFormat fmt, double num, double denom=1.0);

    private:
        bool bigEndian;
        std::map<unsigned int, std::string> result;
        TagOfInterest *tagList;

        void parseIFD(unsigned short parentTag, const unsigned char* ifd, const unsigned char* top, size_t len);
        std::string dumpDirEntry(const unsigned char *dirEnt, const unsigned char* top, PrintFormat fmt);

        short getSShort(const unsigned char **buf) const;
        unsigned short getShort(const unsigned char **buf) const;
        int getSInt(const unsigned char **buf) const;
        unsigned int getInt(const unsigned char **buf) const;
        float getFloat(const unsigned char **buf) const;
        double getDouble(const unsigned char **buf) const;
        unsigned __int64 getInt64(const unsigned char **buf) const;
        unsigned long long getNBytes(const unsigned char **buf, int n) const;


        // for Heif
        int parseBox(const unsigned char *data, size_t len, const unsigned char **cur, unsigned short *cs, unsigned short *tf, const unsigned char **outBody=0);
        typedef struct {
            unsigned int item_ID;
            size_t extent_offset;
            size_t extent_length;
        } ILOC_ITEM;

        int Exif_Item_ID;
        std::vector<ILOC_ITEM> iloc_items;
    };
}
