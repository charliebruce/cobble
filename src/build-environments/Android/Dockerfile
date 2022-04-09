FROM ubuntu

RUN apt-get update -y

# Tools
RUN apt-get install -y wget unzip

# Android Build
RUN apt-get install -y openjdk-8-jdk-headless

# Android NDK r21d
RUN wget https://dl.google.com/android/repository/android-ndk-r21d-linux-x86_64.zip -q -O /android-ndk-r21d-linux-x86_64.zip
RUN unzip -q android-ndk-r21d-linux-x86_64.zip && mv /android-ndk-r21d /android-ndk

# Install Android SDK, Version 29, to Android/Sdk/platforms/android-29/
# export JAVA_OPTS='-XX:+IgnoreUnrecognizedVMOptions --add-modules java.se.ee'
RUN wget https://dl.google.com/android/repository/sdk-tools-linux-4333796.zip -q -O /sdk-tools-linux-4333796.zip 
RUN mkdir /Android && mkdir /Android/Sdk
RUN unzip -q sdk-tools-linux-4333796.zip -d /Android/Sdk/
RUN ln -s /Android/Sdk/tools/bin/sdkmanager /usr/local/bin/sdkmanager
RUN yes | sdkmanager --licenses
# Discard messages (it tries to show progress and does not seem to have a quiet flag)
RUN sdkmanager "platforms;android-29" >/dev/null

