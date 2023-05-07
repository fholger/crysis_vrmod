# Crysis VR Mod

This is a mod for the 2007 Crytek game *Crysis* which makes it possible to experience it in Virtual Reality.
You need to own and have installed the original Crysis (*not* the Remastered version). It is available at:
* [gog.com](https://www.gog.com/en/game/crysis)
* [Steam](https://store.steampowered.com/sub/987/)

This mod is still very early in its development. It has working stereoscopic rendering with 6DOF headset tracking.
There is currently no roomscale support, so you'll have to play seated.
Early support for motion controllers is available and enabled by default (Index and Touch-like controllers only),
though keep in mind that they may not properly work in all situations.
You can always play with a keyboard+mouse or gamepad, instead.

The mod uses OpenXR and was successfully tested with the SteamVR and Oculus runtimes.

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

## Configuration

There is currently no way to change VR-specific settings in the game's options, so you have to use a config file.
The mod includes a sample `system.vrsample.cfg` that you can use as a base, located in your Crysis install folder.
Rename it to `system.cfg` (or copy its contents and paste them into your existing `system.cfg` if you already have one).
Then edit it with your favorite text editor and uncomment and change any of the options you want to change.
The file contains explanations for each of the settings that should hopefully help you figure out what to do.

You might not need to change any settings, at all. But if you want to change your motion controls to left-handed mode,
or if you want to disable motion controls entirely, you will need to modify those settings accordingly.

## Playing

The mod is currently a seated experience and requires that you calibrate your seated position in your VR runtime. 
For SteamVR, once in position, go to your desktop and bring up the SteamVR desktop menu and select "Reset seated position".
The in-VR option to recenter your view does *not* work, unfortunately. Note: you may have to alt-tab out of the game to
access the SteamVR menu on the desktop, which will pause the game and stop it from properly rendering in VR. It will resume
once you refocus the game window.

### Playing with motion controllers

Motion controls are currently only supported for Index controllers and Touch controllers. (Touch-like controllers like Pico
should probably work, too.) Note this is an early development snapshot, and the controls are far from perfect!

By default, your right hand aims your weapons and is also used to control the menu. Your left hand thumbstick will move
you around, while the right hand thumbstick rotates the camera (currently only smooth turn is supported).
See "Configuration" above if you wish to change either hand assignment.

#### Bindings

*Weapon hand*:
- Trigger: fire (click for Index controllers)
- B/Y button: press to change weapons, hold to toggle weapon zoom
- A/X button: press to reload, hold to change firing mode
- Grip (Oculus only): sprint / vehicle boost / afterburner
- Thumbstick click: melee attack

*Non-weapon hand*:
- Trigger: open suit menu (click for Index controllers)
- Trigger (Index only): sprint / vehicle boost / afterburner
- B/Y button: press to open menu, hold to display objectives/map
- A/X button: activate binoculars
- Grip: use / enter vehicle / pick up objects (you need to be looking at them)
- Thumbstick click: toggle night vision

*Movement hand*:
- Thumbstick: move player / vehicle
- A/X button: exit vehicle
- Thumbstick click: toggle vehicle lights

*Non-movement hand*:
- Thumbstick: left/right = rotate camera, up = stand / jump, down = crouch / prone
- A/X button: vehicle brakes
- B/Y button: press to switch vehicle seats, hold to switch to 3rd person vehicle view
- Thumbstick click: vehicle horn

#### Current shortcomings / caveats

- there is currently no individual weapon control per hand, so when dual-wielding pistols both pistols are controlled
  by your selected weapon hand.
- vehicle and stationary guns are aimed with the right thumbstick rather than controller/head orientation
- the same applies to 2D views like the binoculars or certain other zoomed weapon views
- *do not forget to activate weapon zoom mode*! Even though it currently does nothing visually, it significantly reduces
  recoil and spread and is thus necessary to aim at a distance. I will find a more immersive solution for this later.
  (Note: weapon zoom is currently bound to the weapon hand's B/Y button. Long press it to toggle weapon zoom.)


### Playing with mouse+keyboard or gamepad

If you wish to play with a gamepad or keyboard and mouse, you first need to disable motion controllers.
See the "Configuration" section above.
When playing this way, you will see a small white dot in front of you that functions as a crosshair replacement in VR.
You can freely move the crosshair around in a small region in front of you. If you move the crosshair beyond the edges of this
region, your view will rotate.

If you do not like the deadzone, you can disable it or modify its radius. Bring down the console (`~` key by default) and type
in `vr_yaw_deadzone_angle 0`.

### Cutscenes

In-game cutscenes display in VR by default, but since they take over the camera, you might prefer to have them display in 2D, instead. To achieve this, bring down the console (`~` key by default) and type in `vr_cutscenes_2d 1` to switch cutscenes
into 2D mode.


## Notes on performance

Be advised that Crysis in VR needs fairly potent hardware. This is in part due to it being Crysis, and in part due to the method I'm using to implement stereoscopic rendering not being the most efficient one. Unfortunately, due to the limited engine and renderer access being available to me, that's about as good as I can make it.

On the CPU side, expect even the most powerful CPUs to struggle and cause reprojection!
I'd recommend playing on a Ryzen 7000 or 5800X3D or an Intel 12th gen or up. Expect more issues as you go down.
A Ryzen 3000 is not going to provide a very good experience.

On the GPU side, it is less clear, and you do have some options to trade visual quality vs performance via the SteamVR rendering resolution and the ingame quality settings. Still, in my testing an RTX 2080 Ti was struggling (although that may have been in part due to its pairing with a weaker CPU). A 4090 does perfectly fine (duh), as does an RX 7900 XT.

## Known issues

- The desktop mirror does not display anything beyond the menu or HUD. If you wish to record gameplay, use the SteamVR mirror view, instead.
- Some of the later weapons may not work properly.
- Anti-aliasing is known to cause issues and crashes. If you are having trouble, try lowering or disabling the anti-aliasing and restart the game.

## Legal notices

This mod depends on and includes the alternative Crysis launcher by [ccomrade](https://github.com/ccomrade/c1-launcher).

This mod is developed and its source code is covered under the CryENGINE 2 Mod SDK license which you can review in the `LICENSE.txt` file.

This mod is not endorsed by or affiliated with Crytek or Electronic Arts.  Trademarks are the property of their respective owners.  Game content copyright Crytek.
