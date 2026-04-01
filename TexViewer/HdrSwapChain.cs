using Microsoft.Graphics.Display;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml.Controls;
using SharpGen.Runtime;
using System;
using System.Diagnostics;
using System.Linq;
using System.Numerics;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using Vortice.Direct3D;
using Vortice.Direct3D11;
using Vortice.DXGI;
using Vortice.Mathematics;

using InputElementDescription = Vortice.Direct3D11.InputElementDescription;

namespace TexViewer;

public class HdrSwapChain : IDisposable
{
    public ColorSpaceType ColorSpace { get; private set; }
    public Format RenderFormat { get { return Format.R16G16B16A16_Float; } }
    public Format OutputFormat { get; private set; }
    public bool IsHDR { get { return (ColorSpace == ColorSpaceType.RgbFullG2084NoneP2020); }}
    public float MaxFullFrameLuminance { get; private set; }
    public float MaxLuminance { get; private set; }
    public float SDRWhiteLevel { get; private set; }
    public bool enableHDR { get; private set; }

    private ID3D11Device? d3dDevice;
    private ID3D11DeviceContext? d3dContext;
    private ID3D11Texture2D? bitmapTexture;
    private ID3D11VertexShader? vertexShader;
    private ID3D11InputLayout? inputLayout;
    private ID3D11PixelShader? allInOneDraw;
    private ID3D11ComputeShader? computeHistogram;
    private ID3D11SamplerState? samplerState;
    private ID3D11Buffer? vertexBuffer;
    private ID3D11Buffer? allInOneConstantBuffer;
    private ID3D11Buffer? histogramBuffer;
    private ID3D11Buffer? histogramReadback;
    private ID3D11UnorderedAccessView? histogramUAV;
    private ID3D11Buffer? histogramConstantBuffer;
    private ID3D11Buffer? gamutScaleBuffer;
    private ID3D11Buffer? stagingBuffer;
    private ID3D11UnorderedAccessView? gamutScaleUAV;

    private ID3D11RenderTargetView? renderTargetView;
    private ID3D11ShaderResourceView? shaderResView;

    private IDXGIFactory2 dxgiFactory2;
    //private IDXGIDevice? dxgiDevice;
    private IDXGISwapChain1? swapChain;
    private Vortice.WinUI.ISwapChainPanelNative? swapChainPanel;

    private byte[]? vertexShaderBytes;
    private byte[]? allInOneShaderBytes;
    private byte[]? computeShaderBytes;

    private Vortice.Luid adapterLuid;
    private bool prevHDR = false;
    private bool needReCreateSwapChain = false;
    const int HISTOGRAM_BINS = 1024;
    const float FallbackLuminance = 200.0f;
    const int readBackNum = 1;

    [StructLayout(LayoutKind.Sequential)]
    public struct VertexPos {
        public Vector3 pos;
        public Vector2 uv;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct AllInOneParams {
        public Vector4 srcRect;
        public Vector4 dstRect;
        public Vector4 selRect;
        public Vector4 canvasColor;
        public Vector4 param;
        public Vector4 toBT2020_0;
        public Vector4 toBT2020_1;
        public Vector4 toBT2020_2;

        public Vector2 selThickness;

        public float maxCLL;
        public float displayMaxNits;
        public float exposure;
        public float whiteLevel;
        public uint  flags;
        public float dummy;
    }

    [StructLayout(LayoutKind.Sequential)]
    struct HistogramParams {
        public uint Width;
        public uint Height;
        public uint ColorSpace;
        public uint TransferFunc;
    }

    //==============================================================================
    public HdrSwapChain()
    {
        bool debug = false;
#if DEBUG
        debug = true;
#endif
        dxgiFactory2 = DXGI.CreateDXGIFactory2<IDXGIFactory6>(debug);

        Task.Run(async () => await LoadShadersAsync()).Wait();
    }

    public void Dispose()
    {
        D3DCleanup();
        dxgiFactory2?.Dispose();
    }

    public bool CheckDXGIFactory(bool force=false) {
        if (force || !dxgiFactory2.IsCurrent) {
            Debug.WriteLine("***** DXGIFactory Changed");
            D3DCleanup();
            dxgiFactory2?.Dispose();
            bool debug = false;
#if DEBUG
            debug = true;
#endif
            dxgiFactory2 = DXGI.CreateDXGIFactory2<IDXGIFactory6>(debug);
            adapterLuid = Vortice.Luid.FromInt64(0);
            return true;
        }
        return false;
    }

    public bool CheckHDR(AppWindow appWindow)
    {
        // 良いイベントが無い？ とりあえずポーリング。
        var displayInfo = DisplayInformation.CreateForWindowId(appWindow.Id);
        var colorInfo = displayInfo.GetAdvancedColorInfo();
        var enableHDR = (colorInfo.CurrentAdvancedColorKind == DisplayAdvancedColorKind.HighDynamicRange);
        bool ret = (prevHDR != enableHDR);
        prevHDR = enableHDR;
        return ret;
    }

    async Task LoadShadersAsync()
    {
        var shaders = new[] {
            "VertexShader.cso",
            "AllInOneDraw.cso",
            "ComputeHistogram.cso",
        };

        var tasks = shaders.Select(Util.LoadBytesAsync).ToList();
        var results = await Task.WhenAll(tasks);

        vertexShaderBytes   = results[0];
        allInOneShaderBytes = results[1];
        computeShaderBytes  = results[2];
    }


    bool GetAdapterStatus(nint hMonitor) // need re-create d3dDivice?
    {
        for (uint i = 0; ; i++) {
            if (dxgiFactory2.EnumAdapters1(i, out IDXGIAdapter1? adapter).Failure || adapter == null) break;
            try {
                for (uint j = 0; ; j++) {
                    if (adapter.EnumOutputs(j, out IDXGIOutput? output).Failure || output == null) break;
                    try {
                        if (output.Description.Monitor == hMonitor) {
                            MaxFullFrameLuminance = FallbackLuminance;
                            MaxLuminance = FallbackLuminance;
                            if (output.QueryInterface<IDXGIOutput6>() is IDXGIOutput6 output6) {
                                var desc = output6.Description1;
                                MaxFullFrameLuminance = desc.MaxFullFrameLuminance;
                                MaxLuminance = desc.MaxLuminance;
                                output6.Dispose();
                            }
                            if (adapterLuid != adapter.Description1.Luid){
                                Debug.WriteLine($"***** Adapter Changed {adapterLuid} {adapter.Description1.Luid}");

                                adapterLuid = adapter.Description1.Luid;
                                return true;
                            }
                        }
                    } finally {
                        output.Dispose();
                    }
                }
            } finally {
                adapter.Dispose();
            }
        }
        return false;
    }

    void D3DCleanup() // from CreateD3DDevice()
    {
        shaderResView?.Dispose();
        bitmapTexture?.Dispose();
        inputLayout?.Dispose();
        vertexShader?.Dispose();
        allInOneDraw?.Dispose();
        computeHistogram?.Dispose();
        samplerState?.Dispose();
        vertexBuffer?.Dispose();
        allInOneConstantBuffer?.Dispose();
        histogramBuffer?.Dispose();
        histogramReadback?.Dispose();
        histogramUAV?.Dispose();
        histogramConstantBuffer?.Dispose();
        gamutScaleBuffer?.Dispose();
        stagingBuffer?.Dispose();
        gamutScaleUAV?.Dispose();

        d3dContext?.Dispose();
        d3dDevice?.Dispose();
    }

    void SwapChainCleanup() // from CreateSwapChain()
    {
        renderTargetView?.Dispose();

        if (swapChainPanel != null && swapChainPanel.NativePointer != 0) swapChainPanel.SetSwapChain(null);
        swapChain?.Dispose();
        swapChainPanel?.Dispose();
    }

    //------------------------------------------------------------------------------
    public void CreateD3DDevice(nint hMonitor, bool nearestNeighbor)
    {
        if (GetAdapterStatus(hMonitor)){
            D3DCleanup();

            IDXGIFactory4 factory4 = dxgiFactory2.QueryInterface<IDXGIFactory4>();
            factory4.EnumAdapterByLuid<IDXGIAdapter1>(adapterLuid, out var adapter);

            Vortice.Direct3D.FeatureLevel[] featureLevels = new Vortice.Direct3D.FeatureLevel[] {
                Vortice.Direct3D.FeatureLevel.Level_12_1,
                Vortice.Direct3D.FeatureLevel.Level_12_0,
                Vortice.Direct3D.FeatureLevel.Level_11_1,
                Vortice.Direct3D.FeatureLevel.Level_11_0,
            };

            ID3D11Device tempDevice;
            ID3D11DeviceContext tempContext;
            DeviceCreationFlags flags = DeviceCreationFlags.BgraSupport;
#if DEBUG
            flags |= DeviceCreationFlags.Debug;
#endif
            D3D11.D3D11CreateDevice(adapter, DriverType.Unknown, flags, featureLevels,
                                    out tempDevice, out tempContext).CheckError();
            d3dDevice = tempDevice;
            d3dContext = tempContext;

            vertexShader = d3dDevice.CreateVertexShader(vertexShaderBytes!);
            var inputElements = new[] {
                new InputElementDescription("POSITION", 0, Format.R32G32B32_Float, 0, 0),
                new InputElementDescription("TEXCOORD", 0, Format.R32G32_Float, 12, 0)
            };
            inputLayout = d3dDevice.CreateInputLayout(inputElements, vertexShaderBytes!);

            allInOneDraw = d3dDevice.CreatePixelShader(allInOneShaderBytes!);
            computeHistogram = d3dDevice.CreateComputeShader(computeShaderBytes!);

            SetSampler(nearestNeighbor);

            VertexPos[] vertices = new[] {
                /*
                new VertexPos { pos = new(-1.0f,  1.0f, 0.0f), uv = new(0.0f, 0.0f) }, // 0 0-1
                new VertexPos { pos = new( 1.0f,  1.0f, 0.0f), uv = new(1.0f, 0.0f) }, // 1 |/|
                new VertexPos { pos = new(-1.0f, -1.0f, 0.0f), uv = new(0.0f, 1.0f) }, // 2 2-3
                new VertexPos { pos = new(-1.0f, -1.0f, 0.0f), uv = new(0.0f, 1.0f) }, // 2
                new VertexPos { pos = new( 1.0f,  1.0f, 0.0f), uv = new(1.0f, 0.0f) }, // 1
                new VertexPos { pos = new( 1.0f, -1.0f, 0.0f), uv = new(1.0f, 1.0f) }, // 3
                */
                new VertexPos { pos = new(-1.0f,  1.0f, 0.0f), uv = new(0.0f, 0.0f) }, // 0 0-1
                new VertexPos { pos = new( 1.0f,  1.0f, 0.0f), uv = new(1.0f, 0.0f) }, // 1 |\|
                new VertexPos { pos = new( 1.0f, -1.0f, 0.0f), uv = new(1.0f, 1.0f) }, // 3 2-3
                new VertexPos { pos = new( 1.0f, -1.0f, 0.0f), uv = new(1.0f, 1.0f) }, // 3
                new VertexPos { pos = new(-1.0f, -1.0f, 0.0f), uv = new(0.0f, 1.0f) }, // 2
                new VertexPos { pos = new(-1.0f,  1.0f, 0.0f), uv = new(0.0f, 0.0f) }, // 0
            };
            vertexBuffer = d3dDevice.CreateBuffer(vertices, BindFlags.VertexBuffer);

            var allInOneConstantBufferDesc = new BufferDescription {
                ByteWidth = (uint)Marshal.SizeOf<AllInOneParams>(),
                Usage = ResourceUsage.Default,
                BindFlags = BindFlags.ConstantBuffer,
            };
            allInOneConstantBuffer = d3dDevice.CreateBuffer(allInOneConstantBufferDesc);

            var histBufferDesc = new BufferDescription(
                sizeof(uint) * HISTOGRAM_BINS,
                BindFlags.UnorderedAccess | BindFlags.ShaderResource,
                ResourceUsage.Default,
                CpuAccessFlags.None,
                ResourceOptionFlags.BufferStructured,
                structureByteStride: sizeof(uint));
            histogramBuffer = d3dDevice.CreateBuffer(histBufferDesc);

            var histUavDesc = new UnorderedAccessViewDescription
            {
                ViewDimension = UnorderedAccessViewDimension.Buffer,
                Format = Format.Unknown,
                Buffer = new BufferUnorderedAccessView {
                    FirstElement = 0,
                    NumElements = HISTOGRAM_BINS
                }
            };
            histogramUAV = d3dDevice.CreateUnorderedAccessView(histogramBuffer, histUavDesc);

            var readbackDesc = new BufferDescription(
                sizeof(uint) * HISTOGRAM_BINS,
                BindFlags.None,
                ResourceUsage.Staging,
                CpuAccessFlags.Read,
                ResourceOptionFlags.None,
                structureByteStride: 0);
            histogramReadback = d3dDevice.CreateBuffer(readbackDesc);

            var histogramConstantBufferDesc = new BufferDescription {
                ByteWidth = (uint)Marshal.SizeOf<HistogramParams>(),
                Usage = ResourceUsage.Default,
                BindFlags = BindFlags.ConstantBuffer,
            };
            histogramConstantBuffer = d3dDevice.CreateBuffer(histogramConstantBufferDesc);

            var gamutBufferDesc = new BufferDescription(
                sizeof(int) * readBackNum,
                BindFlags.UnorderedAccess | BindFlags.ShaderResource,
                ResourceUsage.Default,
                CpuAccessFlags.None,
                ResourceOptionFlags.None,
                0);
            gamutScaleBuffer = d3dDevice.CreateBuffer(gamutBufferDesc);

            var gamutUavDesc = new UnorderedAccessViewDescription
            {
                ViewDimension = UnorderedAccessViewDimension.Buffer,
                Format = Format.R32_SInt,
                Buffer = new BufferUnorderedAccessView {
                    FirstElement = 0,
                    NumElements = readBackNum
                }
            };
            gamutScaleUAV = d3dDevice.CreateUnorderedAccessView(gamutScaleBuffer, gamutUavDesc);

            var stagingDesc = new BufferDescription (
                sizeof(int) * readBackNum,
                BindFlags.None,
                ResourceUsage.Staging,
                CpuAccessFlags.Read,
                ResourceOptionFlags.None,
                structureByteStride: sizeof(int));
            stagingBuffer = d3dDevice.CreateBuffer(stagingDesc);

            //dxgiDevice = d3dDevice.QueryInterface<IDXGIDevice>();

            Debug.WriteLine("CreateD3DDevice: re-create");
            needReCreateSwapChain = true;
        }else{
            Debug.WriteLine("CreateD3DDevice: re-use");
        }
    }

    public void SetSampler(bool nearestNeighbor){
        Debug.Assert(d3dDevice != null);
        samplerState?.Dispose();
        var samplerDesc = new SamplerDescription() {
            Filter = nearestNeighbor?Vortice.Direct3D11.Filter.MinMagMipPoint:Vortice.Direct3D11.Filter.MinMagMipLinear,
            AddressU = TextureAddressMode.Clamp,
            AddressV = TextureAddressMode.Clamp,
            AddressW = TextureAddressMode.Clamp,
            MipLODBias = 0.0f,
            MaxAnisotropy = 1,
            ComparisonFunc = ComparisonFunction.Never,
            MinLOD = 0.0f,
            MaxLOD = float.MaxValue
        };
        samplerState = d3dDevice.CreateSamplerState(samplerDesc);
    }

    //------------------------------------------------------------------------------
    public bool CreateSwapChain(AppWindow appWindow, ref SwapChainPanel panel, bool SdrTexture, uint w, uint h, bool force)
    {
        Debug.Assert(d3dDevice != null);

        var displayInfo = DisplayInformation.CreateForWindowId(appWindow.Id);
        var colorInfo = displayInfo.GetAdvancedColorInfo();
        var _enableHDR = (colorInfo.CurrentAdvancedColorKind == DisplayAdvancedColorKind.HighDynamicRange);
        bool isSupported = colorInfo.IsHdrMetadataFormatCurrentlySupported(DisplayHdrMetadataFormat.Hdr10);
        SDRWhiteLevel = (float)colorInfo.SdrWhiteLevelInNits;
        if (SDRWhiteLevel <= 0.0f || SDRWhiteLevel >= 2000.0f) SDRWhiteLevel = 100.0f;
        Debug.WriteLine($"enableHDR:{_enableHDR}, isSupported:{isSupported}, MaxLuminance:{colorInfo.MaxLuminanceInNits}, SDRWhite:{colorInfo.SdrWhiteLevelInNits}");

        if (!force && !needReCreateSwapChain){
            if (IsHDR){
                if (!SdrTexture && enableHDR == _enableHDR) return false;
            }else{
                if (SdrTexture && enableHDR == _enableHDR) return false;
            }
        }

        enableHDR = _enableHDR;
        needReCreateSwapChain = false;
        Debug.WriteLine("CreateSwapChain");
        SwapChainCleanup();
        swapChainPanel = ComObject.As<Vortice.WinUI.ISwapChainPanelNative2>(panel);

        if (!SdrTexture && enableHDR && MakeSwapChain(w, h, Format.R10G10B10A2_UNorm)){
            //ret = true;
        }else{
            if (!MakeSwapChain(w, h, Format.B8G8R8A8_UNorm)){
                //FIXME
            }
            //ret = true;
        }

        Debug.Assert(swapChain != null);

        //renderTargetView?.Dispose();
        using (var backBuffer = swapChain.GetBuffer<ID3D11Texture2D>(0)) {
            renderTargetView = d3dDevice.CreateRenderTargetView(backBuffer);
        }

        swapChainPanel.SetSwapChain(swapChain);
        return true;
    }

    bool MakeSwapChain(uint w, uint h, Format format)
    {
        SwapChainDescription1 swapChainDesc = new SwapChainDescription1() {
            Stereo = false,
            Width = w,
            Height = h,
            BufferCount = 2,
            BufferUsage = Usage.RenderTargetOutput | Usage.ShaderInput,
            Format = format,
            SampleDescription = new SampleDescription(1, 0),
            Scaling = Scaling.Stretch,
            AlphaMode = Vortice.DXGI.AlphaMode.Ignore,
            Flags = SwapChainFlags.None,
            SwapEffect = SwapEffect.FlipSequential
        };
        swapChain = dxgiFactory2.CreateSwapChainForComposition(d3dDevice, swapChainDesc);
        return SetColorSpace(format);
    }

    bool SetColorSpace(Format format)
    {
        ColorSpaceType colorSpace;
        if (format == Format.R10G10B10A2_UNorm){
            colorSpace = ColorSpaceType.RgbFullG2084NoneP2020; // HDR
        }else if (format == Format.R16G16B16A16_Float){
            colorSpace = ColorSpaceType.RgbFullG10NoneP709; // not use
        }else if (format == Format.R8G8B8A8_UNorm || format == Format.B8G8R8A8_UNorm){
            colorSpace = ColorSpaceType.RgbFullG22NoneP709; // SDR
        }else{
            colorSpace = ColorSpaceType.RgbFullG22NoneP709; // fallback
        }

        if (swapChain != null){
            using (var swapChain3 = swapChain.QueryInterface<IDXGISwapChain3>()){
                var flags = swapChain3.CheckColorSpaceSupport(colorSpace);
                if ((flags & SwapChainColorSpaceSupportFlags.Present) != 0) {
                    swapChain3.SetColorSpace1(colorSpace);
                    OutputFormat = format;
                    ColorSpace = colorSpace;
                    Debug.WriteLine("SetColorSpace:"+colorSpace);
                    return true;
                }
            }
        }
        return false;
    }

    bool checkDeviceLost(Result hr) {
        // MainWindow.Window_Changed で !dxgiFactory2.IsCurrent になって作り直すからここには来ない？
        if (hr == Vortice.DXGI.ResultCode.DeviceRemoved ||
            hr == Vortice.DXGI.ResultCode.DeviceReset){
            Debug.WriteLine("Device Lost");
            return true;
        }else if (hr.Code != 0){
            Debug.WriteLine("HR:"+hr.Code);
        }
        return false;
    }

    //------------------------------------------------------------------------------
    public void Resize(uint w, uint h)
    {
        Debug.Assert(d3dDevice != null);
        Debug.Assert(swapChainPanel != null);
        if (swapChain == null) return;
        Debug.WriteLine("Resize");

        renderTargetView?.Dispose();
        swapChainPanel.SetSwapChain(null);

        var hr = swapChain.ResizeBuffers(2, w, h);
        checkDeviceLost(hr);

        using (var backBuffer = swapChain.GetBuffer<ID3D11Texture2D>(0)) {
            renderTargetView = d3dDevice.CreateRenderTargetView(backBuffer);
        }

        swapChainPanel.SetSwapChain(swapChain);
    }


    //------------------------------------------------------------------------------
    public void SetBitmap(uint w, uint h, byte[] data)
    {
        Debug.Assert(d3dDevice != null);
        Debug.Assert(d3dContext != null);
        shaderResView?.Dispose();
        bitmapTexture?.Dispose();

        Format format = (data.Length == w*h*4)?Format.R8G8B8A8_UNorm:Format.R16G16B16A16_Float;
        var texDesc = new Texture2DDescription {
            Width = w,
            Height = h,
            MipLevels = 1,
            ArraySize = 1,
            Format = format,
            SampleDescription = new SampleDescription(1, 0),
            Usage = ResourceUsage.Default,
            BindFlags = BindFlags.ShaderResource,
            CPUAccessFlags = CpuAccessFlags.None,
        };

        bitmapTexture = d3dDevice.CreateTexture2D(texDesc);
        uint rowPitch = format.GetBitsPerPixel()/8 * w;
        d3dContext.UpdateSubresource(data, bitmapTexture, 0, rowPitch);

        var shaderResourceViewDesc = new ShaderResourceViewDescription {
            Format = format,
            ViewDimension = ShaderResourceViewDimension.Texture2D,
            Texture2D = new Texture2DShaderResourceView {
                MipLevels = 1,
                MostDetailedMip = 0
            }
        };
        shaderResView = d3dDevice.CreateShaderResourceView(bitmapTexture, shaderResourceViewDesc);
    }



    //------------------------------------------------------------------------------
    //public float Draw(Vector4 srcRect, Vector4 dstRect, Vector4 selRect, Vector4 col, Vector4 param, Vector2 selThickness, float maxCLL, float maxNits, float exposure, float whiteLevel, uint flags, float outW, float outH)
    public float Draw(AllInOneParams allInOneParams, float outW, float outH)
    {
        Debug.Assert(vertexShader != null);
        Debug.Assert(vertexBuffer != null);
        Debug.Assert(inputLayout != null);
        Debug.Assert(allInOneDraw != null);
        Debug.Assert(allInOneConstantBuffer != null);
        Debug.Assert(renderTargetView != null);
        Debug.Assert(gamutScaleUAV != null);
        Debug.Assert(stagingBuffer != null);

        float ret = 1.0f;
        if (d3dContext == null || swapChain == null) return ret;

        if (!dxgiFactory2.IsCurrent){
            Debug.WriteLine("*** dxgiFactory2.IsCurrent: false");
        }
        bool checkGamutScale = (allInOneParams.flags&0x10000000) != 0;

        var stride = Marshal.SizeOf<VertexPos>();
        if (checkGamutScale){
            d3dContext.ClearUnorderedAccessView(gamutScaleUAV, Int4.Zero);
            var uavs = new ID3D11UnorderedAccessView[1];
            uavs[0] = gamutScaleUAV;  // register(u1)
            d3dContext.OMSetRenderTargetsAndUnorderedAccessViews(new ID3D11RenderTargetView[] {renderTargetView}, null!, 1, uavs, new uint[] { 0xFFFFFFFFu });
        }else{
            d3dContext.OMSetRenderTargets(renderTargetView);
        }
        var viewport = new Viewport(0, 0, outW, outH);
        d3dContext.RSSetViewport(viewport);
        d3dContext.RSSetScissorRect((int)outW, (int)outH);
        d3dContext.OMSetBlendState(null);
        d3dContext.ClearRenderTargetView(renderTargetView, new Color4(0.0f, 0.0f, 0.0f, 1.0f));

        d3dContext.IASetInputLayout(inputLayout);
        d3dContext.VSSetShader(vertexShader);
        d3dContext.PSSetShader(allInOneDraw);
        d3dContext.PSSetSampler(0, samplerState);
        if (shaderResView != null) d3dContext.PSSetShaderResource(0, shaderResView);
        d3dContext.UpdateSubresource(in allInOneParams, allInOneConstantBuffer);
        d3dContext.PSSetConstantBuffer(0, allInOneConstantBuffer);

        d3dContext.IASetVertexBuffer(0, vertexBuffer, (uint)stride, 0);
        d3dContext.IASetPrimitiveTopology(PrimitiveTopology.TriangleList);
        d3dContext.Draw(6, 0);

        if (checkGamutScale){
            d3dContext.CopyResource(stagingBuffer, gamutScaleBuffer);
            unsafe{
                var mappedSubresource = d3dContext.Map(stagingBuffer, MapMode.Read);
                var data = new Span<int>(mappedSubresource.DataPointer.ToPointer(), readBackNum);
                ret = data[0]/65535.0f;
                d3dContext.Unmap(stagingBuffer, 0);
                //Debug.WriteLine($"{data[1]/65535.0f} {data[2]/65535.0f} {data[3]/65535.0f} ");
            }
        }
        d3dContext.PSSetShaderResource(0, null!);
        var hr = swapChain.Present(1, PresentFlags.None);
        checkDeviceLost(hr);

        return ret;
    }




    //------------------------------------------------------------------------------

    public float ComputeMaxCLL(uint width, uint height, uint cs, uint tf, float maxNits=10000.0f){
        if (d3dContext == null || histogramBuffer == null || histogramConstantBuffer == null || histogramReadback == null) return maxNits;
        uint[] zeros = new uint[HISTOGRAM_BINS];
        d3dContext.UpdateSubresource(zeros, histogramBuffer);

        var histogramParams = new HistogramParams {
            Width = width,
            Height = height,
            ColorSpace = cs,
            TransferFunc = tf
        };

        d3dContext.CSSetShader(computeHistogram);
        d3dContext.CSSetShaderResource(0, shaderResView);
        d3dContext.CSSetUnorderedAccessView(0, histogramUAV);
        d3dContext.UpdateSubresource(in histogramParams, histogramConstantBuffer);
        d3dContext.CSSetConstantBuffer(0, histogramConstantBuffer);

        uint groupX = (width + 7) / 8;
        uint groupY = (height + 7) / 8;
        d3dContext.Dispatch(groupX, groupY, 1);

        d3dContext.CSSetUnorderedAccessView(0, null);
        d3dContext.CopyResource(histogramReadback, histogramBuffer);

        var dataBox = d3dContext.Map(histogramReadback, 0, MapMode.Read, Vortice.Direct3D11.MapFlags.None);
        uint[] histogram = new uint[HISTOGRAM_BINS];
        Marshal.Copy(dataBox.DataPointer, (int[])(object)histogram, 0, HISTOGRAM_BINS);
        d3dContext.Unmap(histogramReadback, 0);

        uint totalPixels = (uint)(width * height);
        uint threshold = (uint)(totalPixels * 0.999f);  // 上位1%で MaxCLL 推定
        ulong accum = 0;
        for (int i = 0; i < HISTOGRAM_BINS; i++) {
            accum += histogram[i];
            if (accum >= threshold) {
                float maxCLL = (float)(i + 1) / HISTOGRAM_BINS * maxNits;
                Debug.WriteLine($"ComputeMaxCLL:{maxCLL}");
                return maxCLL;
            }
        }
        return maxNits;
    }

}
