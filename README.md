# ps4apps

Original Android helpers and PS4 homebrew by [dreaming144](https://github.com/dreaming144).

Sources only. No APKs, no signed packages, no keystores, no game dumps.

## Android

| App | Package | Folder |
|---|---|---|
| PS4 Helper | `com.chris.ps4helper` | [`android/ps4-helper`](android/ps4-helper) |
| GoldHEN Guide | `com.chris.goldhenguide` | [`android/goldhen-guide`](android/goldhen-guide) |

Needs an Android SDK with **build-tools 35.0.0** and **platforms/android-35**. Set `ANDROID_HOME` or `ANDROID_SDK_ROOT`. Each `build.sh` makes a local debug keystore if missing (Android debug alias; the keystore is gitignored) and writes an APK next to the script.

```sh
export ANDROID_HOME=/path/to/android-sdk
./android/goldhen-guide/build.sh
./android/ps4-helper/build.sh
```

## Homebrew (jailbroken PS4 / OpenOrbis)

Original titles. Fake-pkg via OpenOrbis. Not dumps of commercial games.

| Title | TITLE_ID | Folder |
|---|---|---|
| Twin Decks | `DJDK00001` | [`homebrew/twin-decks`](homebrew/twin-decks) |
| Portal Run | `PRUN00001` | [`homebrew/portal-run`](homebrew/portal-run) |
| San Pedro Grower | `SPDR00001` | [`homebrew/san-pedro-grower`](homebrew/san-pedro-grower) |
| Kush & Caps | `WEED00001` | [`homebrew/kush-and-caps`](homebrew/kush-and-caps) |
| Chapter Briefs | `BOOK00001` | [`homebrew/chapter-briefs`](homebrew/chapter-briefs) |

Needs the [OpenOrbis PS4 toolchain](https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain). Set `OO_PS4_TOOLCHAIN`. Copy the toolchain sample `libc.prx` and `libSceFios2.prx` into each title’s `sce_module/`, and `right.sprx` into `sce_sys/about/` (those binaries ship with OpenOrbis, not this repo). Then:

```sh
export OO_PS4_TOOLCHAIN=/path/to/PS4Toolchain
./homebrew/twin-decks/build.sh
```

Same pattern for the other homebrew folders.

## Licence

App and homebrew sources in this repo are original unless a file says otherwise (Inter OFL, stb / dr_* headers, Twin Decks CC0 music). Sony SDK / OpenOrbis runtime modules are not included.
