using System;
using System.IO;
using System.Runtime.InteropServices;
using UnityEngine;

namespace EnhancedGraphics;
public static class NativeInterop {
    public static void Initialize(string modPath) {
        string dllPath = Path.Combine(modPath, "EnhancedGraphics_Native.dll");
        LoadLibrary(dllPath);
    }
    
    public static void DebugPrint(string str) {
        DebugPrintFromManaged(str);
    }
    
    public enum Events {
        FRAME_START = 0,
        RENDER = 1,
    }

    public static void NotifyRenderEvent(Events eventId, IntPtr data) {
        OnRenderEvent((int)eventId, data);
    }
    
    public static void NotifyRenderEvent(Events eventId) {
        IntPtr data = IntPtr.Zero;
        OnRenderEvent((int)eventId, data);
    }
    
    [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    private static extern IntPtr LoadLibrary(string lpLibFileName);
    
    [DllImport("EnhancedGraphics_Native.dll", CharSet = CharSet.Ansi)]
    private static extern void DebugPrintFromManaged(string str);
    
    [DllImport("EnhancedGraphics_Native.dll")]
    public static extern IntPtr GetUnityEventFunc();
    
    [DllImport("EnhancedGraphics_Native.dll")]
    private static extern IntPtr OnRenderEvent(int eventId, IntPtr data);
}
