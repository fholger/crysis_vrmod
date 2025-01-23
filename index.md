---
layout: splash
author_profile: false

header:
  overlay_image: /assets/images/title.jpg
  overlay_filter: 0.5
  actions:
    - label: "Download"
      url: "https://github.com/fholger/crysis_vrmod/releases/latest"
    - label: "Discord"
      url: "http://flat2vr.com"

gallery:
  - url: /assets/images/screenshots/gallery/screen1.jpg
    image_path: /assets/images/screenshots/gallery/screen1_th.jpg
  - url: /assets/images/screenshots/gallery/screen2.jpg
    image_path: /assets/images/screenshots/gallery/screen2_th.jpg
  - url: /assets/images/screenshots/gallery/screen3.jpg
    image_path: /assets/images/screenshots/gallery/screen3_th.jpg
  - url: /assets/images/screenshots/gallery/screen4.jpg
    image_path: /assets/images/screenshots/gallery/screen4_th.jpg
  - url: /assets/images/screenshots/gallery/screen5.jpg
    image_path: /assets/images/screenshots/gallery/screen5_th.jpg
  - url: /assets/images/screenshots/gallery/screen6.jpg
    image_path: /assets/images/screenshots/gallery/screen6_th.jpg

intro:
  - excerpt: >-
      *Welcome to Lingshan!*
---

{% include feature_row id="intro" type="center" %}

{% include gallery caption="Crysis VR impressions" %}

Crysis VR is a third-party mod for the 2007 PC gaming classic by CryTek. It allows players to experience the world of Crysis in virtual reality. Features:

- full 6DoF roomscale VR experience: play seated or standing with VR motion controllers
- aim your guns naturally with your hands; use two hands to stabilize the guns further
- visible player arms with basic IK
- physical melee attacks
- left-handed support
- bHaptics vest support

The mod is completely free. Simply download [the latest release](https://github.com/fholger/crysis_vrmod/releases/latest)
and install it to your local Crysis folder. If you do not own Crysis, you can get it from
[Steam](https://store.steampowered.com/sub/987/) or
[gog.com](https://www.gog.com/en/game/crysis).

The mod includes a manual that you can access from the menu if you are ever confused how to do something in VR.

If you enjoyed the mod and would like to say thanks, you can leave me a tip on [Ko-Fi](https://ko-fi.com/fholger).

{% include video id="rVGW-ZcZUpk" provider="youtube" %}


### Installation instructions

- Download and install Crysis, if you haven't already. Note: this mod is for the original Crysis, *not* the remastered edition. It is not compatible with the latter!
- Download the Crysis VR installer from the link above and run it. Note: the installer is not signed, so your browser and/or Windows will likely complain about not trusting the installer. You will have to tell Windows to run the installer, anyway.
- Make sure to select the right path to your Crysis installation. If you are not sure where it resides, in Steam you can right-click on the game and select "Manage -> Browse local files" to find the path on your machine. Similarly, in GOG Galaxy you can right-click on the game and go to "Manage installation -> Show folder".
- Finish the installer.

Connect your headset to your PC, ensure you have properly set your active OpenXR runtime, then launch Crysis VR by executing the `CrysisVR.exe` in the `Bin64` subfolder of your Crysis install folder.

NOTE: by default, the installer runs without admin privileges as the Crysis game folder should be user-writeable.
If you run into issues, you may need to explicitly run the installer as an administrator.
To do so, right-click on it and select "Run as administrator".

### Update instructions

Simply download the installer for the new release and repeat the installation instructions. The installer will automatically replace the mod files with the latest version, and afterwards you are ready to go. You should not lose any saves or settings in the process.

*Note*: if you are upgrading from an older version before 1.0, then be aware
that the save location has changed, and saves from the old version will not
show up. You will have to migrate them manually - go to
`<My Documents>\My Games\Crysis` and copy the `SaveGames` folder over to
`<My Documents>\My Games\Crysis VR`.

### Notes for content creators

If you want to record the game's desktop window, you may want to disable the frustum culling optimizations in the VR Settings for a wider, centered view.

I also strongly recommend to check out some of the later chapters of the game. You can download a set of save games at the start of each mission
[here](/assets/CrysisVR_SaveGames.7z). Simply extract them to `<My Documents>\My Games\Crysis VR`.

### Support

Please note that this is a hobby project done in my free time, I can only provide limited support. Feel free to post issues on the [GitHub bugtracker](https://github.com/fholger/crysis_vrmod/issues), or visit the [Flat2VR Discord](http://flat2vr.com) and join the Crysis channels over there. 

### Legal notices

This site is not endorsed by or affiliated with Crytek or Electronic Arts.  Trademarks are the property of their respective owners. Game content copyright Crytek.