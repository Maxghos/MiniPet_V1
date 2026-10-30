#!/usr/bin/env bash
# Codex cloud only. Android Studio on a personal computer uses its bundled JDK.
export JAVA_HOME=/workspace/android-tools/jdk
export ANDROID_HOME=/workspace/android-sdk
export ANDROID_SDK_ROOT=/workspace/android-sdk
export ANDROID_USER_HOME=/workspace/.android
export GRADLE_USER_HOME=/workspace/.gradle
export PATH="$JAVA_HOME/bin:$ANDROID_HOME/platform-tools:$PATH"
