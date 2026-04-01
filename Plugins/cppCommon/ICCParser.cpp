#include "ICCParser.h"

#include <vector>
#include <string>
#include <format>

#include <wincodec.h>
#include <wrl/client.h>
#include <codecvt>

// TODO: 伝達関数がパラメトリック関数やテーブルになってるやつはどうしようかね？


using namespace ICCParser;
using Microsoft::WRL::ComPtr;

static const Primaries primariesList[] = {
//   rx     ry     gx     gy     bx     by     wx      wy
    {0.640, 0.330, 0.300, 0.600, 0.150, 0.060, 0.3127, 0.3290, CS_BT709},      // BT.709/sRGB
    {0.640, 0.330, 0.290, 0.600, 0.150, 0.060, 0.3100, 0.3160, CS_BT601},      // BT.601_PAL
    {0.630, 0.340, 0.310, 0.595, 0.155, 0.070, 0.3127, 0.3290, CS_BT601},      // BT.601_NTSC
    {0.708, 0.292, 0.170, 0.797, 0.131, 0.046, 0.3127, 0.3290, CS_BT2020},     // BT.2020/BT.2100
    {0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.3140, 0.3510, CS_DCI_P3},     // SMPTE RP 431-2
    {0.680, 0.320, 0.265, 0.690, 0.150, 0.060, 0.3127, 0.3290, CS_DISPLAY_P3}, // SMPTE RP 432-1
    {0.640, 0.330, 0.210, 0.710, 0.150, 0.060, 0.3127, 0.3290, CS_ADOBE98},    // AdobeRGB
};


//------------------------------------------------------------------------------
int getShort(unsigned char *buf){ return (buf[0]<<8)+buf[1]; }
int getInt(unsigned char *buf){ return (buf[0]<<24)+(buf[1]<<16)+(buf[2]<<8)+buf[3]; }
double getU8Fixed8Number(unsigned char *buf){ return ((buf[0]<<8)+buf[1])/256.0; }
double getS15Fixed16Number(unsigned char *buf){ return getInt(buf)/65536.0; }

int compareSignature(unsigned char *buf, const char *sig){
    if (buf[0] == sig[0] && buf[1] == sig[1] && buf[2] == sig[2] && buf[3] == sig[3]){
        return 1;
    }
    return 0;
}

int getChromaticAdaptation(unsigned char *buf, int ofs, int len, float *out){
    if (len == 44 && compareSignature(buf+ofs, "sf32")){
        for (int i=0; i<9; i++){
            *out++ = (float)getS15Fixed16Number(buf+ofs+8+i*4);
        }
        return 1;
    }
    return 0;
}

int getXYZ(unsigned char *buf, int ofs, int len, double *outX, double *outY, double *outZ){
    if (len == 20 && compareSignature(buf+ofs, "XYZ ")){
        *outX = getS15Fixed16Number(buf+ofs+8);
        *outY = getS15Fixed16Number(buf+ofs+12);
        *outZ = getS15Fixed16Number(buf+ofs+16);
        return 1;
    }
    return 0;
}

void XYZ_to_xy(double X, double Y, double Z, double& x, double& y) {
    double sum = X + Y + Z;
    if (sum > 0.0) {
        x = X / sum;
        y = Y / sum;
    } else {
        x = 0.31271; // D65
        y = 0.32902;
    }
}

int getTRC(unsigned char *buf, int ofs, int len, double *outG) {
    if (compareSignature(buf+ofs, "curv")){ // curveType
        if (len == 14){
            if (getInt(buf+ofs+8) == 1){
                *outG = 1.0 / getU8Fixed8Number(buf+ofs+12);
                return TF_GAMMA_N;
            }
        }else{
            //int num = getInt(buf+ofs+8);
            //for (int i=0; i<num; i++) getShort(buf+ofs+12+i*2);
            // TODO: TF_LUT を作ってテーブルを1Dtextureでシェーダーに渡す？
        }
        return 0;
    }else if (compareSignature(buf+ofs, "para")){ // parametricCurveType
        double g,a,b,c,d,e,f;
        int funcType = getShort(buf+ofs+8);
        switch(funcType){
          case 0: // Gamma
            *outG = 1.0 / getS15Fixed16Number(buf+ofs+12);
            return TF_GAMMA_N;
          case 3: // IEC 61966-2-1 (sRGB)
            g = getS15Fixed16Number(buf+ofs+12);
            a = getS15Fixed16Number(buf+ofs+16);
            b = getS15Fixed16Number(buf+ofs+20);
            c = getS15Fixed16Number(buf+ofs+24);
            d = getS15Fixed16Number(buf+ofs+28);
            return TF_SRGB;

          case 1: // CIE 122:1996 (Gain Offset Gamma)
            g = getS15Fixed16Number(buf+ofs+12);
            a = getS15Fixed16Number(buf+ofs+16);
            b = getS15Fixed16Number(buf+ofs+20);
            break;
          case 2: // IEC 61966-3 (Equipment using cathode ray tubes)
            g = getS15Fixed16Number(buf+ofs+12);
            a = getS15Fixed16Number(buf+ofs+16);
            b = getS15Fixed16Number(buf+ofs+20);
            c = getS15Fixed16Number(buf+ofs+24);
            break;
          case 4: // WICでも未実装らしい？
            g = getS15Fixed16Number(buf+ofs+12);
            a = getS15Fixed16Number(buf+ofs+16);
            b = getS15Fixed16Number(buf+ofs+20);
            c = getS15Fixed16Number(buf+ofs+24);
            d = getS15Fixed16Number(buf+ofs+28);
            e = getS15Fixed16Number(buf+ofs+32);
            f = getS15Fixed16Number(buf+ofs+36);
            break;
            // TODO: TF_PARAMETRIC を作ってパラメータをシェーダーに渡す？
        }
    }else{
        OutputDebugStringW(L"unknown Signature\n");
    }

    // unknown TRC format

    return 0;
}


std::string WideStringToStdString(const wchar_t* wideString, int len) {
    if (!wideString) {
        return "";
    }

    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wideString, len, nullptr, 0, nullptr, nullptr);
    if (bufferSize == 0) {
        return "";
    }

    std::vector<char> buffer(bufferSize);

    int result = WideCharToMultiByte(CP_UTF8, 0, wideString, len, buffer.data(), bufferSize, nullptr, nullptr);
    if (result == 0) {
         return "";
    }

    return std::string(buffer.data(), len);
}

std::string getMLU(unsigned char *buf, int ofs) {
    if (compareSignature(buf+ofs, "mluc")){ // ICC v4: multiLocalizedUnicodeType
        int num = getInt(buf+ofs+8);
        unsigned char *p = buf+ofs+16;
        std::string str = "";
        for (int i=0; i<num; i++){
            int lang = getShort(p);      // "en"
            int country = getShort(p+2); // "US"
            int str_len = getInt(p+4)/2;
            int str_ofs = getInt(p+8);
            if (i == 0 || lang == 0x656e){ // first or "en"
                std::vector<wchar_t> LEstr;
                unsigned char *p = buf+ofs+str_ofs;
                for (int j=0; j<str_len; j++, p+=2){
                    LEstr.push_back((p[0]<<8) | p[1]);
                }
                str = WideStringToStdString(LEstr.data(), str_len);
                if (lang == 0x656e && country == 0x5553) break; // "enUS"
            }
        }
        if (!str.empty()){
            str.erase(std::find(str.begin(), str.end(), '\0'), str.end());
            return str;
        }
    }
    return "";
}


double primariesDiff(Primaries a, Primaries b){
    return (fabs(a.rx-b.rx)+fabs(a.ry-b.ry)+
            fabs(a.gx-b.gx)+fabs(a.gy-b.gy)+
            fabs(a.bx-b.bx)+fabs(a.by-b.by)+
            fabs(a.wx-b.wx)+fabs(a.wy-b.wy));
}



//==============================================================================
// public method

IccParser::IccParser()
{
    cs = CS_UNKNOWN;
    tf = TF_DEFAULT;
    gamma = 0.0;
    foundPrimaries = false;
}

int IccParser::parseIcc(unsigned char *data, int len)
{
    if (len < 128) return -1;
    int size = getInt(data);
    if (len < size) return -1;

    majorRevision = data[8];
    minorRevision = data[9];

    if (!compareSignature(data+12, "mntr")) return -1; // Profile/Device class
    if (!compareSignature(data+16, "RGB ")) return -1; // Color space of data
    if (!compareSignature(data+20, "XYZ ")) return -1; // Profile connection space

    int numTags = getInt(data+128);
    unsigned char *p = data+132;
    int found = 0;
    double G;
    int ret;
    for (int i=0; i<numTags; i++,p+=12){
        if (compareSignature(p, "meas")){
            //
        }else if (compareSignature(p, "cicp")){
            int ofs = getInt(p+4);
            int len = getInt(p+8);
            if (len == 12 && compareSignature(data+ofs, "cicp")){
                unsigned int ColourPrimaries = *(data+ofs+8);
                unsigned int TransferCharacteristics = *(data+ofs+9);
                //unsigned int MatrixCoefficients = *(data+ofs+10);
                //unsigned int VideoFullRangeFlag = *(data+ofs+11);
                cs = colorSpaceFromCICP(ColourPrimaries);
                tf = transferFunctionFromCICP(TransferCharacteristics);
                return 1;
            }
        }else if (compareSignature(p, "chad")){
            float arr[9];
            if (getChromaticAdaptation(data, getInt(p+4), getInt(p+8), arr)){
                // 1.04788208     0.0229187012  -0.0502014160
                // 0.0295867920   0.990478516   -0.0170593262
                //-0.00923156738  0.0150756836   0.751678467
            }
        }else if (compareSignature(p, "wtpt")){
            if (getXYZ(data, getInt(p+4), getInt(p+8), &wtpt.x, &wtpt.y, &wtpt.z)){
                found |= 0x0001;
            }
        }else if (compareSignature(p, "rXYZ")){
            if (getXYZ(data, getInt(p+4), getInt(p+8), &rXYZ.x, &rXYZ.y, &rXYZ.z)){
                found |= 0x0002;
            }
        }else if (compareSignature(p, "gXYZ")){
            if (getXYZ(data, getInt(p+4), getInt(p+8), &gXYZ.x, &gXYZ.y, &gXYZ.z)){
                found |= 0x0004;
            }
        }else if (compareSignature(p, "bXYZ")){
            if (getXYZ(data, getInt(p+4), getInt(p+8), &bXYZ.x, &bXYZ.y, &bXYZ.z)){
                found |= 0x0008;
            }
        }else if (compareSignature(p, "rTRC")){
            ret = getTRC(data, getInt(p+4), getInt(p+8), &G);
            if (ret == TF_SRGB){
                tf = TF_SRGB;
            }else if (ret == TF_GAMMA_N){
                gamma += G;
                found |= 0x0010;
            }
        }else if (compareSignature(p, "gTRC")){
            ret = getTRC(data, getInt(p+4), getInt(p+8), &G);
            if (ret == TF_SRGB){
                tf = TF_SRGB;
            }else if (ret == TF_GAMMA_N){
                gamma += G;
                found |= 0x0020;
            }
        }else if (compareSignature(p, "bTRC")){
            ret = getTRC(data, getInt(p+4), getInt(p+8), &G);
            if (ret == TF_SRGB){
                tf = TF_SRGB;
            }else if (ret == TF_GAMMA_N){
                gamma += G;
                found |= 0x0040;
            }
        }else if (compareSignature(p, "cprt")){
            cprt = getString(data, getInt(p+4), getInt(p+8));
        }else if (compareSignature(p, "desc")){
            desc = getString(data, getInt(p+4), getInt(p+8));
        }else if (compareSignature(p, "dmnd")){
            dmnd = getString(data, getInt(p+4), getInt(p+8));
        }else if (compareSignature(p, "dmdd")){
            dmdd = getString(data, getInt(p+4), getInt(p+8));
        }
    }

    if ((found & 0xf0) == 0x70){
        gamma /= 3.0;
        if (gamma == 1.0){
            tf = TF_LINEAR;
        }else{
            tf = TF_GAMMA_N;
        }
    }
    if ((found & 0x0f) == 0x0e){
        wtpt.x = 0.9642f; //D50
        wtpt.y = 1.0000f;
        wtpt.z = 0.8251f;
        found |= 1;
    }
    if ((found & 0x0f) == 0x0f){
        foundPrimaries = true;
        Primaries primaries;
        XYZ_to_xy(wtpt.x, wtpt.y, wtpt.z, primaries.wx, primaries.wy);
        XYZ_to_xy(rXYZ.x, rXYZ.y, rXYZ.z, primaries.rx, primaries.ry);
        XYZ_to_xy(gXYZ.x, gXYZ.y, gXYZ.z, primaries.gx, primaries.gy);
        XYZ_to_xy(bXYZ.x, bXYZ.y, bXYZ.z, primaries.bx, primaries.by);
        cs = colorSpaceFromPrimaries(primaries);
        return 1;
    }
    return 0;
}

int IccParser::parseIcc(const wchar_t* file)
{
    int ret = -1;
    HRESULT hr;
    IWICImagingFactory* factory = nullptr;
    hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));

    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromFilename(file, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (SUCCEEDED(hr)) {
        ComPtr<IWICBitmapFrameDecode> frame;
        hr = decoder->GetFrame(0, &frame);  // only 1st frame

        if (SUCCEEDED(hr)) {
            UINT count = 0;
            hr = frame->GetColorContexts(0, nullptr, &count);
            if (count > 0) {
                std::vector<ComPtr<IWICColorContext>> contexts(count);
                for (unsigned int i = 0; i < count; i++) {
                    factory->CreateColorContext(&contexts[i]);
                }
                frame->GetColorContexts(count, contexts[0].GetAddressOf(), &count);
                if (SUCCEEDED(hr) && count > 0) {
                    for (unsigned int i = 0; i < count; i++) {
                        UINT size = 0;
                        contexts[i]->GetProfileBytes(0, nullptr, &size);
                        if (size > 0) {
                            std::vector<BYTE> icc(size);
                            contexts[i]->GetProfileBytes(size, icc.data(), &size);
                            if (parseIcc(icc.data(), size) == 1) {
                                ret = 1;
                            }
                        }
                    }
                }
            }
        }
    }
    return ret;
}


//------------------------------------------------------------------------------
std::string IccParser::getString(unsigned char *buf, int ofs, int len) {
    if (compareSignature(buf+ofs, "text")){
        return std::string((const char *)(buf+ofs+8), len-8-1);
    }else if (compareSignature(buf+ofs, "desc")){
        if (majorRevision == 4){
            return getMLU(buf, ofs);
        }else{
            int str_len = getInt(buf+ofs+8);
            return std::string((const char*)(buf+ofs+12), str_len-1);
        }
    }else if (compareSignature(buf+ofs, "mluc")){ // ICC v4: multiLocalizedUnicodeType
        return getMLU(buf, ofs);
    }
    return "";
}

//------------------------------------------------------------------------------
int IccParser::colorSpaceFromPrimaries(Primaries primaries){
    double minDiff=999;
    int idx = 0;
    for (int i=0; i<sizeof(primariesList)/sizeof(Primaries); i++){
        double diff = primariesDiff(primariesList[i], primaries);
        if (diff < minDiff){
            minDiff = diff;
            idx = i;
        }
    }
    return primariesList[idx].cs;
}


std::string IccParser::dumpIccInfo(){
    std::string ret = std::format("ICC Revision {}.{}\n", majorRevision, minorRevision);
    if (!cprt.empty()) ret += std::format("Copyright: '{}'\n", cprt);
    if (!desc.empty()) ret += std::format("Description: '{}'\n", desc);
    if (!dmnd.empty()) ret += std::format("DeviceMfg: '{}'\n", dmnd); // deviceMfgDesc
    if (!dmdd.empty()) ret += std::format("DeviceModel: '{}'\n", dmdd); // deviceModelDesc

    Primaries primaries;
    XYZ_to_xy(wtpt.x, wtpt.y, wtpt.z, primaries.wx, primaries.wy);
    XYZ_to_xy(rXYZ.x, rXYZ.y, rXYZ.z, primaries.rx, primaries.ry);
    XYZ_to_xy(gXYZ.x, gXYZ.y, gXYZ.z, primaries.gx, primaries.gy);
    XYZ_to_xy(bXYZ.x, bXYZ.y, bXYZ.z, primaries.bx, primaries.by);
    ret += dumpPrimaries(primaries, cs, tf, gamma);
    return ret;
}

std::string IccParser::dumpPrimaries(Primaries _primaries, int _cs, int _tf, double _gamma){
    std::string ret = "Color Primaries\n";
    ret += std::format(" R: {:.5f},{:.5f}\n G: {:.5f},{:.5f}\n B: {:.5f},{:.5f}\n W: {:.5f},{:.5f}\n",
                       _primaries.rx, _primaries.ry,
                       _primaries.gx, _primaries.gy,
                       _primaries.bx, _primaries.by,
                       _primaries.wx, _primaries.wy);
    if (_cs != CS_UNKNOWN){
        std::string csStr="";
        if ((_cs & 0x00ff) <= CS_DISPLAY_P3){
            csStr = std::string(csList[_cs & 0xff]);
        }
        ret += std::format(" Estimated color space: {}\n", csStr);
    }
    if (_tf != TF_DEFAULT){
        std::string tfStr="";
        if (_tf == TF_PQ){
            tfStr = "PQ";
        }else if (_tf == TF_HLG){
            tfStr = "HLG";
        }else if (_tf == TF_GAMMA_N){
            tfStr = std::format("Gamma {:.5f} (1/{:.2f})", _gamma, 1.0/_gamma);
        }else if (_tf <= TF_DISPLAY_P3){
            tfStr = std::string(tfList[_tf >> 8]);
        }
        ret += std::format(" Estimated transfer function: {}\n", tfStr);
    }
    return ret;
}

int IccParser::colorSpaceFromCICP(unsigned int ColourPrimaries) {
    switch(ColourPrimaries){
      case 1:  return CS_BT709;      // BT.709(sRGB)
      case 5:  return CS_BT601;      // BT.601_PAL
      case 6:  return CS_BT601;      // BT.601_NTSC
      case 7:  return CS_BT601;      // SMPTE ST 240
      case 9:  return CS_BT2020;     // BT.2020/BT.2100
      case 11: return CS_DCI_P3;     // SMPTE RP 431-2
      case 12: return CS_DISPLAY_P3; // SMPTE EG 432-1
      default:
        //case 0: Reserved
        //case 2: Unspecified
        //case 3: Reserved
        //case 4: ITU-R BT.470-6 System M
        //case 8: Generic film
        //case 10: SMPTE ST 428-1
        //case 13...21: Reserved
        //case 22: No corresponding industry specification identified
        break;
    }
    return CS_UNKNOWN;
}

std::string IccParser::csStringFromCICP(unsigned int ColourPrimaries) {
    switch(ColourPrimaries){
      case 1:  return "BT.709(sRGB)";
      case 5:  return "BT.601_PAL";
      case 6:  return "BT.601_NTSC";
      case 7:  return "SMPTE ST 240";
      case 9:  return "BT.2020/BT.2100";
      case 11: return "SMPTE RP 431-2(DCI_P3)";
      case 12: return "SMPTE EG 432-1(Display_P3)";
      default:
        break;
    }
    return std::format("Unknown({})", ColourPrimaries);
}


int IccParser::transferFunctionFromCICP(unsigned int TransferCharacteristics) {
    switch(TransferCharacteristics){
      case 1:  return TF_BT709;
      case 6:  return TF_BT601;
      case 8:  return TF_LINEAR;
      case 13: return TF_SRGB;
      case 14: return TF_BT2020; // BT.2020(10bit)
      case 15: return TF_BT2020; // BT.2020(12bit)
      case 16: return TF_PQ;
      case 18: return TF_HLG;
      default:
        //case 0: Reserved
        //case 2: Unspecified
        //case 3: Reserved
        //case 4: ITU-R BT.1700-0 625 PAL and 625 SECAM
        //case 5: ITU-R BT.470-6 System B, G
        //case 7: SMPTE ST 240
        //case 9: Logarithmic transfer characteristic (100:1 range)
        //case 10: Logarithmic transfer characteristic (100 * Sqrt( 10 ) : 1 range)
        //case 11: IEC 61966-2-4
        //case 12: ITU-R BT.1361-0 extended colour gamut system
        //case 17: SMPTE ST 428-1
        //case 19...255: Reserved
        break;
    }
    return TF_DEFAULT;
}

std::string IccParser::tfStringFromCICP(unsigned int TransferCharacteristics) {
    switch(TransferCharacteristics){
      case 1:  return "BT.709";
      case 6:  return "BT.601";
      case 13: return "sRGB";
      case 14: return "BT.2020(10bit)";
      case 15: return "BT.2020(12bit)";
      case 16: return "BT.2100(PQ)";
      case 18: return "BT.2100(HLG)";
      default:
        break;
    }
    return std::format("Unknown({})", TransferCharacteristics);
}
