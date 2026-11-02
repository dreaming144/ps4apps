#!/bin/sh
set -e
cd "$(dirname "$0")"
SDK="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -z "$SDK" ] || [ ! -d "$SDK/build-tools/35.0.0" ]; then
  echo "Set ANDROID_HOME or ANDROID_SDK_ROOT to an Android SDK with build-tools 35.0.0 and platforms/android-35" >&2
  exit 1
fi
BT="$SDK/build-tools/35.0.0"
JAR="$SDK/platforms/android-35/android.jar"
KS="$(pwd)/debug.keystore"
if [ ! -f "$KS" ]; then
  keytool -genkeypair -keystore "$KS" -storepass android -alias androiddebugkey \
    -keypass android -keyalg RSA -keysize 2048 -validity 10000 \
    -dname "CN=Android Debug,O=Android,C=US"
fi
rm -rf build
mkdir -p build/classes
"$BT/aapt2" compile --dir res -o build/res.zip
"$BT/aapt2" link -I "$JAR" -A assets --manifest AndroidManifest.xml -o build/base.apk build/res.zip
javac --release 8 -cp "$JAR" -d build/classes src/com/chris/ps4helper/*.java 2>&1 | grep -v warning || true
"$BT/d8" --min-api 21 --lib "$JAR" --output build build/classes/com/chris/ps4helper/*.class
(cd build && zip -q base.apk classes.dex)
"$BT/zipalign" -f 4 build/base.apk build/aligned.apk
"$BT/apksigner" sign --ks "$KS" --ks-pass pass:android --out PS4-Helper.apk build/aligned.apk
echo "Wrote $(pwd)/PS4-Helper.apk"
