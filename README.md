<p align="center">
    <a href="https://github.com/xenia-canary/xenia-canary/tree/canary_experimental/assets/icon">
        <img height="256px" src="https://raw.githubusercontent.com/xenia-canary/xenia/master/assets/icon/256.png" />
    </a>
</p>

<h1 align="center">Xenia-Xenon</h1>

Xenia-Xenon is an unofficial fork of [Xenia Canary](https://github.com/xenia-canary/xenia-canary)
for **Xbox Series X|S in Dev Mode** (UWP, Direct3D 12). It builds on the Xenia Canary UWP port
(Blades frontend) and is not associated with the Xenia developers.

## Features

- **Internal game storage.** Games live in `D:\DevelopmentFiles\XeniaCanary\Games`, the large
  Dev Mode partition (~180 GB on Series X|S). They show up in the game list automatically.
- **Web file transfer.** A built-in file manager you open from any browser (PC or phone) on the
  same network. Drag in ISO / ZAR / GOD files or whole extracted game folders, with progress,
  rename, delete, download and free-space display. No internet or extra software needed.

## Installing on Xbox (Dev Mode)

1. Put the console in Dev Mode and open Device Portal (`https://<console-ip>:11443`).
2. **Add** the `.appx` plus the `Microsoft.VCLibs.x64.14.00.appx` dependency.
3. In Dev Home, select **Xenia Canary** → Menu → **View details** → set **App type** to **Game**.
   As an "App" it gets too little memory and closes on start.

## Copying games to the console

1. Open Xenia → **paths** tab → **Start Transfer Server** (it stays on for next launches).
2. Open the address shown (e.g. `http://192.168.1.20:8080`) in a browser on the same network.
3. Drop games into the **Games** section. A wired connection gives the best speed.

## Building

Needs Visual Studio 2022 Build Tools with *C++ (v143) Universal Windows Platform tools* and a
Windows 10/11 SDK.

```
git submodule update --init --depth 1
python xenia-build.py premake
msbuild xenia-canary-uwp\xenia-canary-uwp.vcxproj /p:Configuration=Release /p:Platform=x64 ^
  /p:SolutionDir=%CD%\xenia-canary-uwp\ /p:WindowsTargetPlatformVersion=10.0.26100.0 ^
  /p:AppxBundle=Never /p:UapAppxPackageBuildMode=SideloadOnly
```

Restore the `Microsoft.Windows.CppWinRT` NuGet package into `build\packages` first, and sign the
resulting `.appx` with a certificate whose subject matches the manifest publisher.

<h1>Original Readme</h1>

Xenia Canary is an experimental fork of the Xenia emulator. For more information, see the
[Xenia Canary wiki](https://github.com/xenia-canary/xenia-canary/wiki).

Come Join the Xbox Emulation Hub Discord server for help, support and general dissusion: https://discord.gg/WCmxvvxHqu
For developer chat join `#dev` but stay on topic. Lurking is not only fine, but encouraged!
Please check the [FAQ](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page before asking questions.
We've got jobs/lives/etc, so don't expect instant answers.

Discussing illegal activities will get you banned.

## Status

Buildbot | Status | Releases
-------- | ------ | --------
Windows | [![CI](https://github.com/xenia-canary/xenia-canary/actions/workflows/Windows_build.yml/badge.svg?branch=canary_experimental)](https://github.com/xenia-canary/xenia-canary/actions/workflows/Windows_build.yml) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/cd506034fd8148309a45034925648499)](https://app.codacy.com/gh/xenia-canary/xenia-canary/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) | [Latest](https://github.com/xenia-canary/xenia-canary-releases/releases/latest) ◦ [All](https://github.com/xenia-canary/xenia-canary-releases/releases) ◦ [Old](https://github.com/xenia-canary/xenia-canary/releases)
Linux | [![CI](https://github.com/xenia-canary/xenia-canary/actions/workflows/Linux_build.yml/badge.svg?branch=canary_experimental)](https://github.com/xenia-canary/xenia-canary/actions/workflows/Linux_build.yml)
Netplay Build | | [Latest](https://github.com/AdrianCassar/xenia-canary/releases/latest)

## Quickstart

See the [Quickstart](https://github.com/xenia-canary/xenia-canary/wiki/Quickstart) page.

## FAQ

See the [frequently asked questions](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page.

## Game Compatibility

See the [Game compatibility list](https://github.com/xenia-canary/game-compatibility/issues)
for currently tracked games, and feel free to contribute your own updates,
screenshots, and information there following the [existing conventions](https://github.com/xenia-canary/game-compatibility/blob/canary/README.md).

## Building

See [building.md](docs/building.md) for setup and information about the
`xb` script. When writing code, check the [style guide](docs/style_guide.md)
and be sure to run clang-format!

## Contributors Wanted!

Have some spare time, know advanced C++, and want to write an emulator?
Contribute! There's a ton of work that needs to be done, a lot of which
is wide open greenfield fun.

**For general rules and guidelines please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).**

Fixes and optimizations are always welcome (please!), but in addition to
that there are some major work areas still untouched:

* Help work through [missing functionality/bugs in games](https://github.com/xenia-canary/xenia-canary/labels/compat)
* Reduce the size of Xenia's [huge log files](https://github.com/xenia-canary/xenia-canary/issues/1526)
* Skilled with Linux? A strong contributor is needed to [help with porting](https://github.com/xenia-canary/xenia-canary/labels/platform-linux)

See more projects [good for contributors](https://github.com/xenia-canary/xenia-canary/labels/good%20first%20issue). It's a good idea to ask on Discord and check the issues page before beginning work on
something.

## Disclaimer

The goal of this project is to experiment, research, and educate on the topic
of emulation of modern devices and operating systems. **It is not for enabling
illegal activity**. All information is obtained via reverse engineering of
legally purchased devices and games and information made public on the internet
(you'd be surprised what's indexed on Google...).
