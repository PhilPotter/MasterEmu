# MasterEmu
MasterEmu - a C-language Android-based emulator for Sega Master System and Game Gear

This is the open source project upon which all future versions of MasterEmu will be based. I've cleaned it up but erroneous comments referring to retired properietary aspects such as in-app purchases or adverts may still exist. Pull requests and contributors welcome!

To build, the recommended environment is Android Studio, and you must also install the latest NDK. Also, be sure (on Linux at least, that is my build environment) that you clone to a path with no spaces, as otherwise the NDK build will complain and error out. The version code is 1, as my build process involves splitting out separate APKs for each architecture to keep the size down. If you are building your own version, and plan to upgrade an existing install from the Play Store, make sure to increment this before building. New installs won't be affected by this.

Have fun, and I hope people appreciate my work - even more, I hope others contribute so their work can make MasterEmu even better!
