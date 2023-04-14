# Crysis VR Mod

This is a mod for the 2007 Crytek game *Crysis* which makes it possible to experience it in Virtual Reality.
You need to own and have installed the original Crysis (*not* the Remastered version). It is available at:
* [gog.com](https://www.gog.com/en/game/crysis)
* [Steam](https://store.steampowered.com/sub/987/)

This mod is still very early in its development. It has working stereoscopic rendering with 6DOF headset tracking.
There is currently no motion controller or roomscale support, so you'll have to play seated with a keyboard+mouse or gamepad.
The mod uses OpenXR and was successfully tested with the SteamVR and Oculus runtimes, but is expected to work with WMR, too.

## Installation

Download and install Crysis. Then head over to the mod's [Releases](https://github.com/fholger/crysis_vrmod/releases) and
find the latest release at the top of the page. Under the "Assets" section find and download the `crysis-vrmod-x.y.exe` installer.
Run it and point it to where you have installed Crysis. If you are not sure and own it on Steam, right-click on Crysis
in your Steam library, then select "Manage" -> "Browse local files", and it will show you the game's install location.

The installer will install the VR mod and all required dependencies. Once it's done, head over to your Crysis folder and
launch the `CrysisVR.exe` file inside the `Bin64` folder to start playing Crysis in VR.

*Note*: the installer is not digitally signed, so Windows will warn you and stop you from executing it. You'll have to tell
it to run the installer, anyway.

### Development snapshots

If you are the brave sort, you can also opt to download the [latest development build](https://github.com/fholger/crysis_vrmod/releases/tag/latest). Be warned that it may be unstable and contain more bugs than the stable releases, so use at your own risk.

## Playing

The mod is currently a seated experience and requires that you calibrate your seated position in your VR runtime. 
For SteamVR, once in position, go to your desktop and bring up the SteamVR desktop menu and select "Reset seated position".
The in-VR option to recenter your view does *not* work, unfortunately. Note: you may have to alt-tab out of the game to
access the SteamVR menu on the desktop, which will pause the game and stop it from properly rendering in VR. It will resume
once you refocus the game window.

As there is currently no support for motion controllers, you have to play with a gamepad or keyboard and mouse. You will see
a small white dot in front of you that functions as a crosshair replacement in VR. You can freely move the crosshair around
in a small region in front of you. If you move the crosshair beyond the edges of this region, your view will rotate.

If you do not like the deadzone, you can disable it or modify its radius. Bring down the console (`~` key by default) and type
in `vr_yaw_deadzone_angle 0`.

In-game cutscenes display in VR by default, but since they take over the camera, you might prefer to have them display in 2D, instead. To achieve this, bring down the console (`~` key by default) and type in `vr_cutscenes_2d 1` to switch cutscenes
into 2D mode.

## Notes on performance

Be advised that Crysis in VR needs fairly potent hardware. This is in part due to it being Crysis, and in part due to the method I'm using to implement stereoscopic rendering not being the most efficient one. Unfortunately, due to the limited engine and renderer access being available to me, that's about as good as I can make it.

On the CPU side, expect even the most powerful CPUs to struggle in some scenes, although you should mostly be fine playing on a Ryzen 7000 or 5800X3D or an Intel 12th gen or up. Expect more issues as you go down. A Ryzen 3000 is not going to provide a very good experience.

On the GPU side, it is less clear, and you do have some options to trade visual quality vs performance via the SteamVR rendering resolution and the ingame quality settings. Still, in my testing an RTX 2080 Ti was struggling (although that may have been in part due to its pairing with a weaker CPU). A 4090 does perfectly fine (duh), as does an RX 7900 XT.
There are also a few tricks I can hopefully implement at some point that would improve the GPU performance a bit.

## Known issues

- The desktop mirror does not display anything beyond the menu or HUD. If you wish to record gameplay, use the SteamVR mirror view, instead.
- Some of the later weapons may not work properly.
- Anti-aliasing is known to cause issues and crashes. If you are having trouble, try lowering or disabling the anti-aliasing and restart the game.

## Roadmap

- Sort out bugs and issues in the current release
- Improve playability of the current keyboard+mouse seated experience
- Consider simple motion controller and roomscale support

## Legal notices

This mod depends on and includes the alternative Crysis launcher by [ccomrade](https://github.com/ccomrade/c1-launcher).

This mod is developed and its source code is covered under the CryENGINE 2 Mod SDK license which you can review in the `LICENSE.txt` file.

This mod is not endorsed by or affiliated with Crytek or Electronic Arts.  Trademarks are the property of their respective owners.  Game content copyright Crytek.
