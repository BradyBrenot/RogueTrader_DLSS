#include <d3d11.h>
#include <nvsdk_ngx.h>
#include <nvsdk_ngx_helpers.h>

#include <Services.hpp>
#include <Services/DLSS.hpp>
#include <Services/Platform/D3D11.hpp>
#include <Util.hpp>

#include <cstdint>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "Interop.h"

#define DLSS_RUN_TESTS 0

namespace DLSS
{
    void Initialize(ID3D11Device* device, ID3D11DeviceContext* immediateContext);
    void Initialize_RunTests();

    int GetDefaultQualityModes(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes);
    int GetQualityModes_Impl(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes);
    void SetQualityMode_Impl(const QualityMode* mode, const EvaluationFlags flags);
    void Evaluate_Impl(RenderResource colorIn, RenderResource colorOut, float sharpness, const EvaluationParams* params);

    struct {
        bool Initialized = false;
        std::mutex Lock;
        NVSDK_NGX_Handle* Handle = nullptr;
        NVSDK_NGX_Parameter* Params = nullptr;
        QualityMode Mode;
        EvaluationFlags Flags; 
        std::unique_ptr<NVSDK_NGX_D3D11_DLSS_Eval_Params> PendingEvalParams;
        bool Dirty = false;
    } _dlssState;

    DLL_EXPORT bool DLSS_Initialize(ID3D11Resource* TexturePtr, const QualityMode* mode, const EvaluationFlags flags);
    DLL_EXPORT int DLSS_GetQualityModes(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes);
    DLL_EXPORT void DLSS_SetQualityMode(const QualityMode* mode, const EvaluationFlags flags);
    DLL_EXPORT void DLSS_Evaluate(RenderResource colorIn, RenderResource colorOut, float sharpness, const EvaluationParams* params);
}

bool DLSS::DLSS_Initialize(ID3D11Resource* TexturePtr, const QualityMode* mode, const EvaluationFlags flags)
{
    std::scoped_lock _(_dlssState.Lock);
    
    if (_dlssState.Initialized)
    {
        return false;
    }
    
    D3D11::Initialize(TexturePtr);
    Initialize(D3D11::GetDevice(), D3D11::GetImmediateContext());
    SetQualityMode_Impl(mode, flags);
    
    return true;
}

int DLSS::DLSS_GetQualityModes(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes)
{
    std::scoped_lock _(_dlssState.Lock);
    if (!_dlssState.Initialized) {
        return GetDefaultQualityModes(finalWidth, finalHeight, outQualityModes);
    }
    
    return GetQualityModes_Impl(finalWidth, finalHeight, outQualityModes);
}

void DLSS::DLSS_SetQualityMode(const QualityMode* mode, const EvaluationFlags flags)
{
    std::scoped_lock _(_dlssState.Lock);
    
    if (D3D11::GetDevice() == nullptr) {
        return;
    }
    
    if (mode->Preset == NVSDK_NGX_PerfQuality_Value_UltraQuality)
    {
        // Not valid
        const_cast<QualityMode*>(mode)->Preset = NVSDK_NGX_PerfQuality_Value_MaxQuality;
    }
    
    SetQualityMode_Impl(mode, flags);
}

void DLSS::DLSS_Evaluate(RenderResource colorIn, RenderResource colorOut, float sharpness, const EvaluationParams* params)
{
    Evaluate_Impl(colorIn, colorOut, sharpness, params);
}

int DLSS::GetDefaultQualityModes(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes)
{
    auto ScaleResolution = [](NVSDK_NGX_PerfQuality_Value v, uint32_t dim) {
        auto ScaleFactor = [v]() -> float
        {
            switch (v)
            {
            case NVSDK_NGX_PerfQuality_Value_MaxPerf: return 2.0f;
            case NVSDK_NGX_PerfQuality_Value_Balanced: return 1.0f / 0.58f;
            case NVSDK_NGX_PerfQuality_Value_MaxQuality: return 1.5;
            case NVSDK_NGX_PerfQuality_Value_UltraPerformance: return 3.0f;
            case NVSDK_NGX_PerfQuality_Value_UltraQuality: return 1.33f;
            case NVSDK_NGX_PerfQuality_Value_DLAA: return 1.0f;
            }
            
            return 1.f;
        };
        
        return static_cast<uint32_t>(static_cast<float>(dim) / ScaleFactor());
    };
    
    assert(!_dlssState.Initialized);
    
    int numModes = 0;
    for (int i = 0; i <= NVSDK_NGX_PerfQuality_Value_DLAA; ++i)
    {
        if (i == NVSDK_NGX_PerfQuality_Value_UltraQuality)
        {
            continue;
        }

        if (outQualityModes)
        {
            QualityMode& mode = outQualityModes[numModes];
            mode.Preset = static_cast<NVSDK_NGX_PerfQuality_Value>(i);
            mode.InputWidth = ScaleResolution(mode.Preset, finalWidth);
            mode.InputHeight = ScaleResolution(mode.Preset, finalHeight);
            mode.FinalWidth = finalWidth;
            mode.FinalHeight = finalHeight;
        }

        numModes++;
    }

    return numModes;
}

int DLSS::GetQualityModes_Impl(uint32_t finalWidth, uint32_t finalHeight, QualityMode* outQualityModes)
{
    assert(_dlssState.Initialized);

    NVSDK_NGX_Parameter* params = nullptr;

    if (FAIL(NVSDK_NGX_D3D11_AllocateParameters(&params))) {
        return 0;
    }

    int count = 0;

    if (SUCCEEDED(NVSDK_NGX_D3D11_GetCapabilityParameters(&params))) {
        NVSDK_NGX_PerfQuality_Value preset = NVSDK_NGX_PerfQuality_Value_MaxPerf;

        while (preset <= NVSDK_NGX_PerfQuality_Value_DLAA) {
            uint32_t width = 0, dynWidthMin = 0, dynWidthMax = 0;
            uint32_t height = 0, dynHeightMin = 0, dynHeightMax = 0;
            float sharpness = 0.0f;

            NVSDK_NGX_PerfQuality_Value curPreset = preset;
            preset = (NVSDK_NGX_PerfQuality_Value)(curPreset + 1);

            if (FAIL(NGX_DLSS_GET_OPTIMAL_SETTINGS(
                    params,
                    finalWidth, finalHeight,
                    curPreset,
                    &width, &height,
                    &dynWidthMax, &dynHeightMax,
                    &dynWidthMin, &dynHeightMin,
                    &sharpness))) {
                continue;
            }

            if (width == 0 || height == 0) {
                continue;
            }

            ++count;

            if (outQualityModes) {
                outQualityModes->Preset = curPreset;
                outQualityModes->InputWidth = width;
                outQualityModes->InputHeight = height;
                outQualityModes->FinalWidth = finalWidth;
                outQualityModes->FinalHeight = finalHeight;
                ++outQualityModes;
            }
        }
    }

    DISCARD(NVSDK_NGX_D3D11_DestroyParameters(params));
    return count;
}

void DLSS::SetQualityMode_Impl(const QualityMode* mode, const EvaluationFlags flags)
{
    assert(_dlssState.Initialized);

    if (_dlssState.Handle) {
        if (_dlssState.Mode.InputWidth == mode->InputWidth &&
            _dlssState.Mode.InputHeight == mode->InputHeight &&
            _dlssState.Mode.FinalWidth == mode->FinalWidth &&
            _dlssState.Mode.FinalHeight == mode->FinalHeight &&
            _dlssState.Flags == flags
        ) {
            return; // Already selected, no need to recreate.
        }

        DISCARD(NVSDK_NGX_D3D11_ReleaseFeature(_dlssState.Handle));
        DISCARD(NVSDK_NGX_D3D11_DestroyParameters(_dlssState.Params));

        _dlssState.Handle = nullptr;
        _dlssState.Params = nullptr;
    }

    assert(!_dlssState.Handle);
    assert(!_dlssState.Params);

    NVSDK_NGX_Parameter* params = nullptr;

    if (FAIL(NVSDK_NGX_D3D11_AllocateParameters(&params))) {
        LOG_ERROR("Failed to AllocateParameters");
        return;
    }

    NVSDK_NGX_DLSS_Create_Params createParams = {
        .Feature = NVSDK_NGX_Feature_Create_Params {
            .InWidth = mode->InputWidth,
            .InHeight = mode->InputHeight,
            .InTargetWidth = mode->FinalWidth,
            .InTargetHeight = mode->FinalHeight,
            .InPerfQualityValue = mode->Preset,
        },
        .InFeatureCreateFlags = flags,
        .InEnableOutputSubrects = false
    };

    NVSDK_NGX_Handle* handle = nullptr;

    if (FAIL(NGX_D3D11_CREATE_DLSS_EXT(D3D11::GetImmediateContext(), &handle, params, &createParams))) {
        DISCARD(NVSDK_NGX_D3D11_DestroyParameters(params));
        return;
    }

    _dlssState.Handle = handle;
    _dlssState.Params = params;
    _dlssState.Mode = *mode;
    _dlssState.Flags = flags;
    _dlssState.Dirty = true;
}

void DLSS::Evaluate_Impl(RenderResource colorIn, RenderResource colorOut, float sharpness, const EvaluationParams* params)
{
    if (_dlssState.PendingEvalParams)
    {
        return;
    }

    assert(_dlssState.Initialized);
    assert(_dlssState.Handle);
    assert(_dlssState.Params);

    assert(colorIn);
    assert(colorOut);

    assert(params);
    assert(params->DepthIn);
    assert(params->MvecIn);

    NVSDK_NGX_D3D11_DLSS_Eval_Params evalParams = {
        .Feature = {
            .pInColor = (ID3D11Resource*)colorIn,
            .pInOutput = (ID3D11Resource*)colorOut,
            .InSharpness = sharpness,
        },
        .pInDepth = (ID3D11Resource*)params->DepthIn,
        .pInMotionVectors = (ID3D11Resource*)params->MvecIn,
        .InJitterOffsetX = params->JitterX,
        .InJitterOffsetY = params->JitterY,
        .InRenderSubrectDimensions = { .Width = _dlssState.Mode.InputWidth, .Height = _dlssState.Mode.InputHeight },
        .InReset = _dlssState.Dirty,
        .InMVScaleX = params->MVecScaleX,
        .InMVScaleY = params->MVecScaleY,
        .InIndicatorInvertYAxis = true
    };
    
    _dlssState.Dirty = false;
    
    evalParams.Feature.pInColor->AddRef();
    evalParams.Feature.pInOutput->AddRef();
    evalParams.pInDepth->AddRef();
    evalParams.pInMotionVectors->AddRef();

    _dlssState.PendingEvalParams = std::make_unique<NVSDK_NGX_D3D11_DLSS_Eval_Params>(evalParams);
}

void DLSS::Initialize(ID3D11Device* device, ID3D11DeviceContext* immediateContext)
{
    if (_dlssState.Initialized) {
        return;
    }

    IDXGIAdapter* adapter = D3D11::GetAdapter();

    const NVSDK_NGX_FeatureCommonInfo featureCommonInfo = {
        .LoggingInfo = NVSDK_NGX_LoggingInfo {
            .LoggingCallback = [](const char* msg, NVSDK_NGX_Logging_Level level, NVSDK_NGX_Feature source) {
                LOG(level == NVSDK_NGX_LOGGING_LEVEL_VERBOSE ? LogLevel::Debug : LogLevel::Info, "NGX: {}", msg);
            },
            .MinimumLoggingLevel = NVSDK_NGX_LOGGING_LEVEL_VERBOSE,
            .DisableOtherLoggingSinks = true }
    };

    static constexpr const char* _projectId = "33edfdb6-226b-463c-8c08-5f43e8aa6b82";
    static constexpr NVSDK_NGX_EngineType _engineType = NVSDK_NGX_ENGINE_TYPE_UNITY;
    static constexpr const char* _engineVersion = "1";

    const NVSDK_NGX_FeatureDiscoveryInfo discovery = {
        .SDKVersion = NVSDK_NGX_Version_API,
        .FeatureID = NVSDK_NGX_Feature_SuperSampling,
        .Identifier = NVSDK_NGX_Application_Identifier {
            .IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id,
            .v = NVSDK_NGX_ProjectIdDescription {
                .ProjectId = _projectId,
                .EngineType = _engineType,
                .EngineVersion = _engineVersion } },
        .ApplicationDataPath = TempPath(),
        .FeatureInfo = &featureCommonInfo
    };

    NVSDK_NGX_FeatureRequirement requirement = {};

    if (FAIL(NVSDK_NGX_D3D11_GetFeatureRequirements(adapter, &discovery, &requirement))) {
        return;
    }

    if (FAIL(requirement.FeatureSupported)) {
        std::string errorString;

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_Supported) {
            errorString += " NVSDK_NGX_FeatureSupportResult_Supported";
        }

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_CheckNotPresent) {
            errorString += " NVSDK_NGX_FeatureSupportResult_CheckNotPresent";
        }

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_DriverVersionUnsupported) {
            errorString += " NVSDK_NGX_FeatureSupportResult_DriverVersionUnsupported";
        }

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_AdapterUnsupported) {
            errorString += " NVSDK_NGX_FeatureSupportResult_AdapterUnsupported";
        }

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_OSVersionBelowMinimumSupported) {
            errorString += " NVSDK_NGX_FeatureSupportResult_OSVersionBelowMinimumSupported";
        }

        if (requirement.FeatureSupported & NVSDK_NGX_FeatureSupportResult_NotImplemented) {
            errorString += " NVSDK_NGX_FeatureSupportResult_NotImplemented";
        }

        LOG_ERROR("NVSDK_NGX_D3D11_GetFeatureRequirements failed: {}\n", errorString);
        return;
    }

    if (FAIL(NVSDK_NGX_D3D11_Init_with_ProjectID(_projectId, _engineType, _engineVersion, TempPath(), device, &featureCommonInfo))) {
        return;
    }

    static bool first = true;

    Interop::On_Frame([]() {
        first = true;
    });

    Interop::On_Before_Present([]() {
        std::lock_guard _(_dlssState.Lock);

        NVSDK_NGX_D3D11_DLSS_Eval_Params* params = _dlssState.PendingEvalParams.get();

        if (first && params) {
            // Validate that all the resources still match (this can fail if any resizing is happening).
            std::vector<std::tuple<ID3D11Resource*, uint32_t, uint32_t>> inputs = {
                { params->Feature.pInColor, _dlssState.Mode.InputWidth, _dlssState.Mode.InputHeight },
                { params->Feature.pInOutput, _dlssState.Mode.FinalWidth, _dlssState.Mode.FinalHeight },
                { params->pInDepth, _dlssState.Mode.InputWidth, _dlssState.Mode.InputHeight },
                { params->pInMotionVectors, _dlssState.Mode.InputWidth, _dlssState.Mode.InputHeight },
            };

            bool validationFailure = false;

            for (auto& [res, width, height] : inputs) {
                D3D11_TEXTURE2D_DESC desc = {};
                if (ID3D11Texture2D * tex; SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&tex))) {
                    tex->GetDesc(&desc);
                    tex->Release();
                }

                if (desc.Width != width || desc.Height != height) {
                    LOG_WARNING("DLSS input texture size mismatch: {}x{} vs {}x{} - {}\n", desc.Width, desc.Height, width, height, D3D11::GetDebugName(res));
                    validationFailure = true;
                }
            }

            if (!validationFailure) {
                DISCARD(NGX_D3D11_EVALUATE_DLSS_EXT(D3D11::GetImmediateContext(), _dlssState.Handle, _dlssState.Params, params));
            }

            params->Feature.pInColor->Release();
            params->Feature.pInOutput->Release();
            params->pInDepth->Release();
            params->pInMotionVectors->Release();

            _dlssState.PendingEvalParams.reset();
            first = false;
        }
    });
    
    #if 0
    DXGI::On_ResizeBuffers([]() {
        if (const NVSDK_NGX_D3D11_DLSS_Eval_Params* params = _dlssState.PendingEvalParams.get())
        {
            params->Feature.pInColor->Release();
            params->Feature.pInOutput->Release();
            params->pInDepth->Release();
            params->pInMotionVectors->Release();

            _dlssState.PendingEvalParams.reset();
        }
    });
    #endif

    _dlssState.Initialized = true;

    Initialize_RunTests();
}

void DLSS::Initialize_RunTests()
{
#if DLSS_RUN_TESTS == 0
    return;
#endif

    assert(_dlssState.Initialized);

    int numModes = DLSS_GetQualityModes(1920, 1080, nullptr);
    assert(numModes > 0);

    std::vector<QualityMode> modes(numModes);
    int numModesWithData = DLSS_GetQualityModes(1920, 1080, modes.data());
    assert(numModesWithData > 0);

    DLSS_SetQualityMode(&modes[0], {});
    assert(_dlssState.Handle);
    assert(_dlssState.Params);

    NVSDK_NGX_Handle* handle = _dlssState.Handle;
    NVSDK_NGX_Parameter* params = _dlssState.Params;

    DLSS_SetQualityMode(&modes[0], {});
    assert(_dlssState.Handle == handle);
    assert(_dlssState.Params == params);

    DLSS_SetQualityMode(&modes[1], {});
    assert(_dlssState.Handle && _dlssState.Handle != handle);
    assert(_dlssState.Params && _dlssState.Params != params);
}
