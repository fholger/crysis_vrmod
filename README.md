# Crysis VR Mod

This is a mod for the 2007 Crytek game *Crysis* which makes it possible to experience it in Virtual Reality.
You need to own and have installed the original Crysis (*not* the Remastered version). It is available at:
* [gog.com](https://www.gog.com/en/game/crysis)
* [Steam](https://store.steampowered.com/sub/987/)

This mod is still very early in its development. It has working stereoscopic rendering with 6DOF headset tracking.
There is currently no motion controller or roomscale support, so you'll have to play seated with a keyboard+mouse or gamepad.
The mod currently requires SteamVR to run.

## Installation

Download and install Crysis. Then head over to the mod's [Releases](https://github.com/fholger/crysis_vrmod/releases) and
find the latest release at the top of the page. Expand the "Assets" and download the `crysis-vrmod-x.y.exe` installer.
Run it and point it to where you have installed Crysis. If you are not sure and own it on Steam, right-click on Crysis
in your Steam library, then select "Manage" -> "Browse local files", and it will show you the game's install location.

The installer will install the VR mod and all required dependencies. Once it's done, head over to your Crysis folder and
double-click the `LaunchVRMod.bat` file to start playing Crysis in VR.

*Note*: the installer is not digitally signed, so Windows will warn you and stop you from executing it. You'll have to tell
it to run the installer, anyway.

### Development snapshots

If you are the brave sort, you can also opt to download the [latest development build](https://github.com/fholger/crysis_vrmod/releases/tag/latest). Be warned that it may be unstable and contain more bugs than the stable releases, so use at your own risk.

## Playing

The mod is currently a seated experience and requires that you calibrate your seated position in SteamVR. Once in position,
bring up the SteamVR menu and click on the little position icon in the lower right of the menu to calibrate your seated position.

As there is currently no support for motion controllers, you have to play with a gamepad or keyboard and mouse. You will see
a small white dot in front of you that functions as a crosshair replacement in VR. You can freely move the crosshair around
in a small region in front of you. If you move the crosshair beyond the edges of this region, your view will rotate.

In-game cutscenes display in VR by default, but since they take over the camera, you might prefer to have them display in 2D, instead. To achieve this, bring down the console (`~` key by default) and type in `vr_cutscenes_2d 1` to switch cutscenes
into 2D mode.

## Known issues

- The desktop mirror does not display anything beyond the menu or HUD. If you wish to record gameplay, use the SteamVR mirror view, instead.
- Some of the later weapons may not work properly.
- Performance can be an issue even on potent hardware. You can reduce the resolution or the ingame quality settings if you struggle with GPU performance. Note, however, that some scenes will tax even the strongest CPUs available right now.

## Roadmap

- Sort out bugs and issues in the current release
- Improve playability of the current keyboard+mouse seated experience
- Consider port to OpenXR
- Consider simple motion controller and roomscale support

## Legal notices

This mod depends on and includes the alternative Crysis launcher by [ccomrade](https://github.com/ccomrade/c1-launcher).

This mod is developed and its source code is covered under the CryENGINE 2 Mod SDK license which you can review in the `LICENSE.txt` file.

This mod is not endorsed by or affiliated with Crytek or Electronic Arts.  Trademarks are the property of their respective owners.  Game content copyright Crytek.
