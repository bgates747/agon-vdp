#ifndef VERSION_H
#define VERSION_H

#define		VERSION_MAJOR		2
#define		VERSION_MINOR		16
#define		VERSION_PATCH		0
#define		VERSION_CANDIDATE	0			// Optional
#define		VERSION_TYPE		"Release"	// RC, Alpha, Beta, etc.

#define		VERSION_VARIANT		"Platform"
#define     VERSION_SUBTITLE    "Bistromathics"

// Combined-product subsystem identities. Keep these separate from the
// upstream Agon Platform version so the base firmware remains unambiguous.
#define     PINGO_VERSION       "0.1.0 Alpha 1"
#define     WOLF3DORIG_VERSION  "0.1.0 Alpha 1"
#define     WOLF3DORIG_VERSION_MAJOR              0
#define     WOLF3DORIG_VERSION_MINOR              1
#define     WOLF3DORIG_VERSION_PATCH              0
#define     WOLF3DORIG_VERSION_PRERELEASE         1 // 0=release, 1=alpha
#define     WOLF3DORIG_VERSION_PRERELEASE_NUMBER  1

#endif // VERSION_H
