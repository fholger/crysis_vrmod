# Crysis VR Mod

[![Ko-Fi](https://img.shields.io/badge/Ko--fi-F16061?style=for-the-badge&logo=ko-fi&logoColor=white)](https://ko-fi.com/fholger)

[Crysis VR website](https://crysis.vrmods.eu)

This is a mod for the 2007 Crytek game *Crysis* which makes it possible to experience it in Virtual Reality, with full 6DOF tracking
and motion controller support.
You need to own and have installed the original Crysis (*not* the Remastered version). It is available at:
* [gog.com](https://www.gog.com/en/game/crysis)
* [Steam](https://store.steampowered.com/sub/987/)

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

### Updates

To update to a newer version, you can just download the new version's
installer and run it. It will overwrite the previous version.

*Note*: if you are upgrading from an older version before 1.0, then be aware
that the save location has changed, and saves from the old version will not
show up. You will have to migrate them manually - go to
`<My Documents>\My Games\Crysis` and copy the `SaveGames` folder over to
`<My Documents>\My Games\Crysis VR`.

### Development snapshots

If you are the brave sort, you can also opt to download the [latest development build](https://github.com/fholger/crysis_vrmod/releases/tag/latest).
Be warned that it may be unstable and contain more bugs than the stable releases, so use at your own risk.

## Configuration

When you boot up the VR mod, you will notice a number of additional buttons in the lower left of the menu. Click on the
"VR Settings" to open a menu where you can configure various aspects of the VR playing experience to your liking.

## Playing

Note that this mod features smooth locomotion and vehicle sections; it requires steady VR legs to play. The basic gameplay
should feel familiar to experienced VR players. Some notable controls you should know about:
- there are 3 weapon "holsters" that each store multiple weapons; you can cycle through them by pressing your main hand's grip
  button at the relevant holster location
  - hip holster: stores pistols and bare fists; point your hand downwards near your hip and press grip
  - shoulder holster: stores your two main assault weapons; lift your hand over your shoulders and press grip
  - chest/back holster: stores explosives; put your hand in front of your chest or behind your back and press grip
- you can grab weapons with both hands; this will increase accuracy and reduce spread and recoil
- your nanosuit has 4 distinct modes: armor, speed, strength and cloak. You can access and enable these modes through the suit menu.
  To bring it up, press and hold your right stick/trackpad, then move your hand in the direction of the mode you want to select and let go.
- You can modify your weapon's accessories. The accessories menu is available through the suit menu, to the lower left.
- You can drop weapons by opening the suit menu and pressing the left A button.

There are ingame HUD hints that explain some of the general controls as they come up. The mod also features an extensive manual that you
can find in the main menu through the "VR Manual" button. Refer to it any time you need to look up how to do something. :)

#### Current shortcomings / caveats

- vehicle and stationary guns are aimed with the right thumbstick rather than controller/head orientation


## Notes on performance

Be advised that Crysis in VR needs fairly potent hardware. This is in part due to it being Crysis, and in part due to the method I'm using
to implement stereoscopic rendering not being the most efficient one. Unfortunately, due to the limited engine and renderer access being
available to me, that's about as good as I can make it.

On the CPU side, expect even the most powerful CPUs to struggle and cause reprojection!
I'd recommend playing on a Ryzen 7000 or 5800X3D or an Intel 12th gen or up. Expect more issues as you go down.
A Ryzen 3000 is not going to provide a very good experience.

On the GPU side, it is less clear, and you do have some options to trade visual quality vs performance via the SteamVR rendering resolution
and the ingame quality settings. Still, in my testing an RTX 2080 Ti was struggling (although that may have been in part due to its pairing
with a weaker CPU). A 4090 does perfectly fine (duh), as does an RX 7900 XT.

## Known issues

- Anti-aliasing is known to cause issues and crashes. Do not enable it with this mod!

## Legal notices

This mod depends on and includes the alternative Crysis launcher by [ccomrade](https://github.com/ccomrade/c1-launcher).

This mod is developed and its source code is covered under the CryENGINE 2 Mod SDK license which you can review in the `LICENSE.txt` file.

This mod is not endorsed by or affiliated with Crytek or Electronic Arts.  Trademarks are the property of their respective owners.  Game content copyright Crytek.
