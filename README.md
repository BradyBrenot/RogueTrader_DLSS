# EnhancedGraphics

Adds DLSS support to Warhammer 40k: Rogue Trader. Non-Nvidia users gain OptiScaler support.

Tested as of Rogue Trader 1.5.0.320

## Why use?

1. Better anti-aliasing than the built-in TAA
2. Improved image quality and image stability (lack of shimmering) compared to the built-in FSR
3. Less blurry/more detailed compared to FSR due to upscaling now happening before post processing.
4. Non-Nvidia / Pre-Pascal users can hook OptiScaler into the DLSS input to use XeSS and more recent FSR than the game ships with.

## Installation

1. Get the latest release from [here](https://github.com/BradyBrenot/RogueTrader_DLSS/releases/latest)
2. Run the game at least once.
3. Extract `EnhancedGraphics.zip` to the Owlcat Mods directory (Win+R, `%userprofile%/AppData/LocalLow/Owlcat Games/Warhammer 40000 Rogue Trader/UnityModManager/`)
4. Get `nvngx_dlss.dll` (minimum 310.6.0) from somewhere, e.g. [TechPowerUp](https://www.techpowerup.com/download/nvidia-dlss-dll/), and place it in the mod's directory (`.../UnityModManager/EnhancedGraphics/`)
5. Non-Nvidia users: follow the **OptiScaler** instructions below
6. Start the game, and load a save file. In the mod menu (ctrl+f10 by default), open Enhanced Graphics, and set the Upscaler Type to `DlssUpscaler`. Select a preset or drag the slider to make a custom preset.

### Note for pre-v2 users:
Delete any files from this mod in the game directory (the one where WH40KRT.exe is). That includes winhttp.dll, version.dll, EnhancedGraphics_Native.asi

## OptiScaler (non-Nvidia users)
1. Get [OptiScaler](https://github.com/optiscaler/OptiScaler/releases)
2. Extract OptiScaler into the game directory
3. Run setup_windows.bat
4. (Not sure which filename you should choose)
5. Choose AMD/Intel as the GPU type
6. Say 'Yes' when asked if you want to "... try to use DLSS inputs ..."
7. Run the game as explained above. You can tweak OptiScaler's settings in its control pantel (default: Insert); you still use the mod's configuration (above) to turn it on and control the scaling ratio

## Bugs

See [here](https://github.com/BradyBrenot/RogueTrader_DLSS/issues) for a list of known issues and to report bugs.

## Credits

1. Original mod: [cstamford](https://github.com/cstamford/RogueTrader_DLSS)
