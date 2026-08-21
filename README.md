# Player Color

A DLL mod for customizing player colors in Rivals of Aether.

Copyright (C) 2026 bob4040

## Features

- Set separate main and dark colors for local Players 1–4
- Set separate main and dark colors for your online card and your opponent's card
- Use per-player color animations, including rainbow, purple gradient, and pulsing brightness

## Requirements

- Rivals of Aether 2.1.9.1
- Windows
- [roa-mod-loader](https://github.com/raicool/roa-mod-loader) 0.0.4

## Installation

1. Install `roa-mod-loader`.
2. Copy `player-color.dll` and `player-color.txt` into the game's `mods` folder.
3. Start the game.

Restart the game after changing the configuration.

## Uninstallation

Delete these files from the `mods` folder:

```text
player-color.dll
player-color.txt
player-color.log
```

## Color Configuration

Colors use the `#RRGGBB` format.

```text
local_1p_main=#FF0000
local_1p_dark=#800000
local_2p_main=#0000FF
local_2p_dark=#000080
local_3p_main=#00FF00
local_3p_dark=#008000
local_4p_main=#FFFF00
local_4p_dark=#808000
online_self_main=#FF00FF
online_self_dark=#800080
online_opponent_main=#FF0080
online_opponent_dark=#800040
```

`main` is the brighter color used for borders and similar elements. `dark` is the darker color used for icon backgrounds and similar elements.

## Animations

To apply the same animation to everyone:

```text
animation=rainbow
animation_speed=0.08
```

Available modes:

- `off`: no animation
- `rainbow`: cycles through the colors of the rainbow
- `purple`: smoothly shifts through purple tones
- `pulse`: changes the brightness of the configured color

Animations can also be configured per player:

```text
animation=off
animation_speed=0.08
local_1p_animation=rainbow
local_2p_animation=purple
local_3p_animation=pulse
local_4p_animation=off
online_self_animation=purple
online_opponent_animation=pulse
```

To set a player-specific speed, use a key such as `local_1p_animation_speed=0.12`.

## Online Play

Color changes are applied only to your own screen. Your opponent does not need to install this mod. The mod does not change gameplay rules or match results.

## Building from Source

Visual Studio 2022 with the C++ build tools is required.

```powershell
.\scripts\build.ps1
```

The DLL is written to:

```text
build\Release\player-color.dll
```

The build script checks that the DLL is x86 and runs the automated tests.

## License

GNU General Public License v3.0. See [LICENSE](LICENSE) for details.
