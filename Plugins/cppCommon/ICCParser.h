#pragma once

#include <string>

#include "../LoadResult.h"

namespace ICCParser {

    typedef struct {
        double rx, ry;
        double gx, gy;
        double bx, by;
        double wx, wy;
        int cs;
    }Primaries;

    typedef struct {
        double x, y, z;
    } XYZ;


    //------------------------------------------------------------------------------
    class IccParser
    {
    public:
        IccParser();

        int parseIcc(unsigned char *iccData, int len);
        int parseIcc(const wchar_t* file);

        bool hasPrimaries() const { return foundPrimaries; }
        XYZ getWtpt() const { return wtpt; }
        XYZ getRxyz() const { return rXYZ; }
        XYZ getGxyz() const { return gXYZ; }
        XYZ getBxyz() const { return bXYZ; }
        int getColorSpace() const { return cs; }
        int getTransferFunction() const { return tf; }
        double getGamma() const { return gamma; }
        std::string dumpIccInfo();

        static int colorSpaceFromPrimaries(Primaries primaries);
        static std::string dumpPrimaries(Primaries _primaries, int _cs=CS_UNKNOWN, int _tf=TF_DEFAULT, double _gamma=1.0);
        static int colorSpaceFromCICP(unsigned int ColourPrimaries);
        static std::string csStringFromCICP(unsigned int ColourPrimaries);
        static int transferFunctionFromCICP(unsigned int TransferCharacteristics);
        static std::string tfStringFromCICP(unsigned int TransferCharacteristics);

    private:
        std::string getString(unsigned char *buf, int ofs, int len);
        int cs;
        int tf;
        double gamma;

        //double ParametricParam[7];
        //std::vector<unsigned short> LookUpTable;

        int majorRevision;
        int minorRevision;
        std::string cprt;
        std::string desc;
        std::string dmnd;
        std::string dmdd;

        bool foundPrimaries;
        XYZ wtpt;
        XYZ rXYZ;
        XYZ gXYZ;
        XYZ bXYZ;
    };
}
