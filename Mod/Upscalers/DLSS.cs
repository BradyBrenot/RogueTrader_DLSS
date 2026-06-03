using System;
using System.Linq;
using System.Runtime.InteropServices;
using UnityEngine;

namespace EnhancedGraphics.Upscalers;

public class DlssUpscaler : IUpscaler {
    private bool _settingsDirty = false;
    private bool _regeneratePresets = false;

    private QualityMode _mode;
    private EvaluationFlags _evalFlags;
    
    private QualityMode Mode {
        get => _mode;
        set {
            if (_mode.InputWidth == value.InputWidth && _mode.InputHeight == value.InputHeight && _mode.Preset == value.Preset) return;
            
            _mode = value;
            _settingsDirty = true;
        }
    }
    private EvaluationFlags EvalFlags {
        get => _evalFlags;
        set {
            _evalFlags = value;
            _settingsDirty = true;
        }
    }
    
    public UpscalePreset[] AvailablePresets {
        get {
            Vector2 displayResolution = new(Screen.width, Screen.height);
            if (_availablePresets == null || displayResolution != _availablePresetsResolution || _regeneratePresets) {
                _availablePresets = [.. GetAvailablePresets(displayResolution).OrderByDescending(x => x.Ratio)];
                _availablePresetsResolution = displayResolution;
                _regeneratePresets = false;
            }
            return _availablePresets;
        }
    }

    public unsafe bool SetPreset(UpscalePreset preset, UpscaleFlags flags) {
        Mode = new() {
            Preset = (DlssPreset)preset.Preset,
            InputWidth = (uint)preset.RenderResolution.x,
            InputHeight = (uint)preset.RenderResolution.y,
            FinalWidth = (uint)preset.DisplayResolution.x,
            FinalHeight = (uint)preset.DisplayResolution.y
        };

        if (flags.HasFlag(UpscaleFlags.HDR)) {
            _evalFlags |= EvaluationFlags.IsHDR;
        }

        if (flags.HasFlag(UpscaleFlags.MVRenderRes)) {
            _evalFlags |= EvaluationFlags.MVRenderRes;
        }

        if (flags.HasFlag(UpscaleFlags.MVJitter)) {
            _evalFlags |= EvaluationFlags.MVJittered;
        }

        if (flags.HasFlag(UpscaleFlags.DepthInverted)) {
            _evalFlags |= EvaluationFlags.DepthInverted;
        }

        if (flags.HasFlag(UpscaleFlags.AutoExposure)) {
            _evalFlags |= EvaluationFlags.AutoExposure;
        }

        if (flags.HasFlag(UpscaleFlags.AlphaUpscaling)) {
            _evalFlags |= EvaluationFlags.AlphaUpscaling;
        }

        return true;
    }

    public unsafe bool Evaluate(IntPtr colorIn, IntPtr colorOut, float sharpness, UpscaleOptionalParams param) {
        EvaluationParams evalParams = new() {
            DepthIn = param.Depth,
            MvecIn = param.Mvec,
            JitterX = param.Jitter.x,
            JitterY = param.Jitter.y,
            MVecScaleX = param.MvecScale.x,
            MVecScaleY = param.MvecScale.y,
        };

        bool firstInit = DLSS_Initialize(param.Mvec, Mode, _evalFlags);
        if (firstInit) {
            Vector2 displayResolution = new(Screen.width, Screen.height);
            
            _regeneratePresets = true;
            UpscalePreset[] presets = GetAvailablePresets(displayResolution);
            
            foreach(UpscalePreset preset in presets)
            {
                if (preset.Preset == (int)Mode.Preset) {
                    Mode = new() {
                        Preset = (DlssPreset)preset.Preset,
                        InputWidth = (uint)preset.RenderResolution.x,
                        InputHeight = (uint)preset.RenderResolution.y,
                        FinalWidth = (uint)preset.DisplayResolution.x,
                        FinalHeight = (uint)preset.DisplayResolution.y
                    };
                }
            }
            
            _settingsDirty = false;
        }
        
        if (_settingsDirty) {
            Debug.Log($"[DLSS] _settingsDirty");
            SetQualityModeNative(Mode, _evalFlags);
            _settingsDirty = false;
        }
        
        EvaluateNative(colorIn, colorOut, sharpness, in evalParams);

        return true;
    }

    private UpscalePreset[] _availablePresets;
    private Vector2 _availablePresetsResolution;

    private unsafe UpscalePreset[] GetAvailablePresets(Vector2 displayResolution) {
        const int MAX_QUALITY_MODES = 15;
        QualityMode* qualityModes = stackalloc QualityMode[MAX_QUALITY_MODES];
        int numQualityModes = GetQualityModesNative((uint)displayResolution.x, (uint)displayResolution.y, qualityModes);
        Debug.Assert(numQualityModes <= MAX_QUALITY_MODES);
        UpscalePreset[] ret = new UpscalePreset[numQualityModes];

        if (numQualityModes <= 0) return ret;
        
        for (int i = 0; i < numQualityModes; i++) {
            ret[i] = new(
                Preset: (int)qualityModes[i].Preset,
                Name: PresetName(qualityModes[i].Preset),
                RenderResolution: new(qualityModes[i].InputWidth, qualityModes[i].InputHeight),
                DisplayResolution: new(qualityModes[i].FinalWidth, qualityModes[i].FinalHeight)
            );
        }

        return ret;
    }

    [Flags]
    private enum EvaluationFlags {
        None = 0,
        IsHDR = 1 << 0,
        MVRenderRes = 1 << 1,
        MVJittered = 1 << 2,
        DepthInverted = 1 << 3,
        Reserved_0 = 1 << 4,
        DoSharpening = 1 << 5,
        AutoExposure = 1 << 6,
        AlphaUpscaling = 1 << 7,
    };

    [StructLayout(LayoutKind.Sequential)]
    private struct EvaluationParams {
        public IntPtr DepthIn;
        public IntPtr MvecIn;
        public float JitterX;
        public float JitterY;
        public float MVecScaleX;
        public float MVecScaleY;
    };

    private enum DlssPreset {
        MaxPerf,
        Balanced,
        MaxQuality,
        UltraPerformance,
        UltraQuality,
        DLAA,
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct QualityMode {
        public DlssPreset Preset;
        public uint InputWidth;
        public uint InputHeight;
        public uint FinalWidth;
        public uint FinalHeight;
    };
    
    static string PresetName(DlssPreset q) => q switch
    {
        DlssPreset.MaxPerf          => "Performance",
        DlssPreset.Balanced         => "Balanced",
        DlssPreset.MaxQuality       => "Quality",
        DlssPreset.UltraPerformance => "Ultra Performance",
        DlssPreset.UltraQuality     => "Ultra Quality",
        DlssPreset.DLAA             => "DLAA",
        _ => q.ToString()
    };
    
    [DllImport("EnhancedGraphics_Native.dll", EntryPoint = "DLSS_Initialize")]
    private static unsafe extern bool DLSS_Initialize(IntPtr texPtr, QualityMode qualityMode, EvaluationFlags flags);

    [DllImport("EnhancedGraphics_Native.dll", EntryPoint = "DLSS_GetQualityModes")]
    private static unsafe extern int GetQualityModesNative(uint finalWidth, uint finalHeight, QualityMode* outQualityModes);

    [DllImport("EnhancedGraphics_Native.dll", EntryPoint = "DLSS_SetQualityMode")]
    private static unsafe extern void SetQualityModeNative(QualityMode qualityMode, EvaluationFlags flags);
    
    [DllImport("EnhancedGraphics_Native.dll", EntryPoint = "DLSS_Evaluate")]
    private static unsafe extern void EvaluateNative(IntPtr colorIn, IntPtr colorOut, float sharpness, in EvaluationParams param);
}
