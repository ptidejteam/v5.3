dnl Ring - configure.ac for automake 1.9 and autoconf 2.59

dnl Process this file with autoconf to produce a configure script.
AC_PREREQ([2.65])
AC_INIT([Ring],[2.3.0],[ring@lists.savoirfairelinux.net],[ring])

AC_COPYRIGHT([[Copyright (c) Savoir-faire Linux 2004-2016]])
AC_REVISION([$Revision$])

dnl Where to find configure files
AC_CONFIG_SRCDIR(src/ring_api.cpp)
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([-Wno-portability])
m4_ifdef([AM_PROG_AR], [AM_PROG_AR]) # required by automake 1.12
AC_CONFIG_HEADERS([config.h])

# Silent build by default. Use make V=1 to increase verbosity
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

dnl Check for programs
AC_PROG_CC
AC_PROG_CXX
AC_PROG_OBJCXX
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

dnl
dnl  Check the operating system
dnl
HAVE_WIN64="0"
HAVE_IOS="0"
HAVE_OSX="0"

case "${host_os}" in
  "")
    SYS=unknown
    ;;
  linux*)
    SYS=linux
    # Necessary for linking .a to a .so
    LDFLAGS+=" -Wl,-Bsymbolic"
    # Needed for plugin system
    LDFLAGS+=" -ldl"
    ;;
  darwin*)
    SYS=darwin
    AC_COMPILE_IFELSE([AC_LANG_PROGRAM(
        [[#import <TargetConditionals.h>
          #if TARGET_OS_IPHONE == 0
          #error this is not an IPHONE
          #endif
        ]])],
        [HAVE_IOS="1"],
        [HAVE_OSX="1"])
    ;;
  mingw32*)
    SYS=mingw32
    AC_CHECK_TOOL(WINDRES, windres, :)
    AC_CHECK_TOOL(OBJCOPY, objcopy, :)
    AC_DEFINE([_UNICODE], [1], [Define to 1 for Unicode (Wide Chars) APIs.])
    AC_DEFINE([UNICODE], [1], [Define to 1 for Unicode (Wide Chars) APIs.])
    AC_DEFINE([_ISOC99_SOURCE], [1], [Extensions to ISO C89 from ISO C99.])
    AC_DEFINE([_ISOC11_SOURCE], [1], [Extensions to ISO C99 from ISO C11.])
    AC_DEFINE([_POSIX_SOURCE], [1], [IEEE Std 1003.1.])
    AC_DEFINE([_POSIX_C_SOURCE], [200809L], [IEEE Std 1003.1.])
    AC_DEFINE([_XOPEN_SOURCE], [700], [POSIX and XPG 7th edition])
    AC_DEFINE([_XOPEN_SOURCE_EXTENDED], [1], [XPG things and X/Open Unix extensions.])
    AC_DEFINE([_BSD_SOURCE], [1], [ISO C, POSIX, and 4.3BSD things.])
    AC_DEFINE([_SVID_SOURCE], [1], [ISO C, POSIX, and SVID things.])

    AC_DEFINE([WIN32_LEAN_AND_MEAN],[1], [Define to limit the scope of <windows.h>.])

    CPPFLAGS+="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -D__USE_MINGW_ANSI_STDIO=1"
    LDFLAGS+="-no-undefined -avoid-version -Wl,--nxcompat -Wl,--dynamicbase"
    ac_default_prefix="`pwd`/_win32"
    DESTDIR="`pwd`/_win32/"
    AC_SUBST(WINDOWS_ARCH)
    AC_SUBST(PROGRAMFILES)

    case "${host}" in
            amd64*|x86_64*)
                HAVE_WIN64="1"
            ;;
    esac
    ;;
  *)
    SYS="${host_os}"
    ;;
esac
AM_CONDITIONAL(HAVE_DARWIN,  test "${SYS}" = "darwin")
AM_CONDITIONAL(HAVE_LINUX,   test "${SYS}" = "linux")
AM_CONDITIONAL(HAVE_WIN32,   test "${SYS}" = "mingw32")
AM_CONDITIONAL(HAVE_WIN64,   test "${HAVE_WIN64}" = "1")
AM_CONDITIONAL(HAVE_OSX,   test "${HAVE_OSX}" = "1")
AM_CONDITIONAL(HAVE_IOS,   test "${HAVE_IOS}" = "1")

dnl FIXME this should be deduced automatically
AC_DEFINE_UNQUOTED([HAVE_COREAUDIO],
                    `if test "${HAVE_OSX}" = "1" || test "${HAVE_IOS}" = "1"; then echo 1; else echo 0; fi`,
                    [Define if you have CoreAudio])

dnl Android is linux, but a bit different
AS_IF([test "$SYS" = linux],[
    AC_MSG_CHECKING([for an Android system])
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM(
        [[#ifndef __ANDROID__
         # error Not Android
         #endif
        ]],[[;]])
    ],[
      HAVE_ANDROID="1"
      AC_MSG_RESULT([yes])
    ],[
      AC_MSG_RESULT([no])
    ])
])

AM_CONDITIONAL(HAVE_ANDROID, test "${HAVE_ANDROID}" = "1")

dnl override platform specific check for dependent libraries
dnl otherwise libtool linking of shared libraries will
dnl fail on anything other than pass_all.
AC_CACHE_VAL(lt_cv_deplibs_check_method,
    [lt_cv_deplibs_check_method=pass_all])

LT_INIT([dlopen win32-dll shared disable-static])

dnl Define C++ as default language
LT_LANG(C++)

DOLT

lt_cv_deplibs_check_method=pass_all

dnl Check for C++11 support
AX_CXX_COMPILE_STDCXX_11([ext],[mandatory])

dnl Check for header files
AC_FUNC_ALLOCA
AC_HEADER_STDC
AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h limits.h \
                  netdb.h netinet/in.h stdlib.h string.h \
                  sys/ioctl.h sys/socket.h sys/time.h unistd.h])

dnl Check for typedefs, structures, and compiler characteristics
AC_HEADER_STAT
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_C_VOLATILE
AC_CHECK_TYPES([ptrdiff_t])

PKG_PROG_PKG_CONFIG()

dnl On some OS we need static linking
AS_IF([test -n "${PKG_CONFIG}" ],[
    AS_IF([test "${SYS}" = "mingw32" -o "${SYS}" = "darwin"],[
        PKG_CONFIG="${PKG_CONFIG} --static"
    ])
])

dnl Check if we are using clang
AC_MSG_CHECKING([if compiling with clang])
AC_COMPILE_IFELSE(
[AC_LANG_PROGRAM([], [[
#ifndef __clang__
       not clang
#endif
]])],
[CLANG=yes], [CLANG=no])

AC_MSG_RESULT([$CLANG])

dnl
dnl  Check for the contrib directory
dnl
AC_ARG_WITH(contrib,
  [AS_HELP_STRING([--with-contrib[=DIR]],
    [search for 3rd party libraries in DIR/include and DIR/lib])
])
AC_MSG_CHECKING([for 3rd party libraries path])
AS_IF([test -z "${with_contrib}" || test "${with_contrib}" = "yes"], [
  CONTRIB_DIR="${srcdir}/contrib/${host}"
  AS_IF([test ! -d "${CONTRIB_DIR}"], [
    echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR="${srcdir}/contrib/`$CC -dumpmachine`"
    AS_IF([test ! -d "${CONTRIB_DIR}"], [
      echo "${CONTRIB_DIR} not found" >&AS_MESSAGE_LOG_FD
      CONTRIB_DIR=""
      AC_MSG_RESULT([not found])
    ])
  ])
], [
  AS_IF([test "${with_contrib}" != "no"], [
    CONTRIB_DIR="${with_contrib}"
  ], [
    CONTRIB_DIR=""
    AC_MSG_RESULT([disabled])
  ])
])

AS_IF([test -n "${CONTRIB_DIR}"], [
  AS_IF([test -d "${CONTRIB_DIR}/lib"],[
    CONTRIB_DIR=`cd "${CONTRIB_DIR}" && pwd`
  ], [
    echo "${CONTRIB_DIR}/lib not found" >&AS_MESSAGE_LOG_FD
    CONTRIB_DIR=""
    AC_MSG_RESULT([not usable])
  ])
])

AS_IF([test -n "${CONTRIB_DIR}"], [
  AC_MSG_RESULT([${CONTRIB_DIR}])
  export PATH=${CONTRIB_DIR}/bin:$PATH
  CPPFLAGS="${CPPFLAGS} -I${CONTRIB_DIR}/include"
  CFLAGS="${CFLAGS} -DPJ_AUTOCONF=1 "
  CXXFLAGS="${CXXFLAGS} -DPJ_AUTOCONF=1 "
  AS_IF([test "${HAVE_WIN64}" = "1"],[
      CFLAGS+=" -DPJ_WIN64=1"
      CXXFLAGS+=" -DPJ_WIN64=1"
  ])
  CFLAGS+=" -I${CONTRIB_DIR}/include"
  CXXFLAGS+=" -I${CONTRIB_DIR}/include"
  OBJCFLAGS="${OBJCFLAGS} -I${CONTRIB_DIR}/include"
  AS_IF([test "${SYS}" = "mingw32"],[
        PKG_CONFIG_PATH_CUSTOM="${CONTRIB_DIR}/lib/pkgconfig:${CONTRIB_DIR}/lib64/pkgconfig"
        AC_SUBST(PKG_CONFIG_PATH_CUSTOM)
        export PKG_CONFIG_PATH_CUSTOM
  ])
  export PKG_CONFIG_PATH="${CONTRIB_DIR}/lib/pkgconfig:${CONTRIB_DIR}/lib64/pkgconfig:$PKG_CONFIG_PATH"
  LDFLAGS="${LDFLAGS} -L${CONTRIB_DIR}/lib"

  AS_IF([test "${SYS}" = "darwin"], [
    export LD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$LD_LIBRARY_PATH"
    export DYLD_LIBRARY_PATH="${CONTRIB_DIR}/lib:$DYLD_LIBRARY_PATH"
  ])
], [
  AS_IF([test -n "${with_contrib}" && test "${with_contrib}" != "no"], [
    AC_MSG_ERROR([Third party libraries not found!])
  ])
])
AC_SUBST(CONTRIB_DIR)

dnl Add extras/tools to the PATH
TOOLS_DIR="${srcdir}/extras/tools/build/bin"
AS_IF([test -d "${TOOLS_DIR}"], [
  TOOLS_DIR=`cd "${TOOLS_DIR}" && pwd`
  export PATH="${TOOLS_DIR}":$PATH
])

dnl Check for zlib
PKG_CHECK_MODULES(ZLIB, zlib,, AC_MSG_ERROR([zlib not found]))

dnl Check for pjproject
PKG_CHECK_MODULES(PJPROJECT, libpjproject,, AC_MSG_ERROR([Missing pjproject files]))

PKG_CHECK_MODULES([YAMLCPP], [yaml-cpp >= 0.5.1],, AC_MSG_ERROR([yaml-cpp not found]))
PKG_CHECK_MODULES([JSONCPP], [jsoncpp >= 1.6.5],, AC_MSG_ERROR([jsoncpp not found]))

if test "${HAVE_ANDROID}" = "1"; then
    dnl Check for OpenSL
    AC_ARG_WITH([opensl],
                AS_HELP_STRING([--without-opensl],
                               [Ignore presence of opensl and disable it]))

    AS_IF([test "x$with_opensl" != "xno"],
          [AC_CHECK_HEADER(["SLES/OpenSLES.h"],
                           [have_opensl=yes], [have_opensl=no])],
          [have_opensl=no])
    AC_DEFINE_UNQUOTED([HAVE_OPENSL],
                       `if test "x$have_opensl" = "xyes"; then echo 1; else echo 0; fi`,
                       [Define if you have OpenSL])
fi
AM_CONDITIONAL([BUILD_OPENSL], test "x$have_opensl" = "xyes")

if test "${SYS}" = "linux"; then
dnl Check for alsa development package - name: libasound2-dev
ALSA_MIN_VERSION=1.0
AC_ARG_WITH([alsa],
    [AS_HELP_STRING([--without-alsa], [disable support for alsa])],
    [],
    [with_alsa=yes])
AS_IF([test "x$with_alsa" = "xyes"], [
       PKG_CHECK_MODULES(ALSA, alsa >= ${ALSA_MIN_VERSION},, AC_MSG_ERROR([Missing alsa development files]))
    ]);

AC_DEFINE_UNQUOTED([HAVE_ALSA], `if test "x$with_alsa" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have alsa])
fi
AM_CONDITIONAL(BUILD_ALSA, test "x$with_alsa" = "xyes")


if test "${SYS}" = "linux"; then
dnl Check for pulseaudio development package - name: libpulse-dev
LIBPULSE_MIN_VERSION=0.9.15
AC_ARG_WITH([pulse],
    [AS_HELP_STRING([--without-pulse], [disable support for pulseaudio])],
    [],
    [with_pulse=yes])

AS_IF([test "x$with_pulse" = "xyes"], [
       PKG_CHECK_MODULES(PULSEAUDIO, libpulse >= ${LIBPULSE_MIN_VERSION},, AC_MSG_ERROR([Missing pulseaudio development files]))
      ]);

AC_DEFINE_UNQUOTED([HAVE_PULSE], `if test "x$with_pulse" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have pulseaudio])
fi
AM_CONDITIONAL(BUILD_PULSE, test "x$with_pulse" = "xyes")

AC_ARG_WITH([jack],
    AS_HELP_STRING([--without-jack], [Ignore presence of jack and disable it]))

AS_IF([test "x$with_jack" != "xno"],
      [PKG_CHECK_MODULES(JACK, jack, [have_jack=yes], [have_jack=no])],
      [have_jack=no])

AS_IF([test "x$have_jack" = "xyes"],,
      [AS_IF([test "x$with_jack" = "xyes"],
             [AC_MSG_ERROR([jack requested but not found])
      ])
])

dnl check for libportaudio
AS_IF([test "${SYS}" = "mingw32"],[
AC_ARG_WITH([portaudio], [AS_HELP_STRING([--without-portaudio],
            [disable support for portaudio])], [], [with_portaudio=yes])
])
AS_IF([test "x$with_portaudio" = "xyes"],
      [PKG_CHECK_MODULES(PORTAUDIO, [portaudio-2.0],
        [AC_DEFINE([HAVE_PORTAUDIO], 1, [Define if you have portaudio])],
        [AC_MSG_ERROR([Missing libportaudio development files])])
      ])

AM_CONDITIONAL(BUILD_PORTAUDIO, test "x$with_portaudio" = "xyes")
AM_CONDITIONAL(HAVE_PORTAUDIO, test "x$with_portaudio" = "xyes")

AC_DEFINE_UNQUOTED([HAVE_JACK], `if test "x$have_jack" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have jack])
AM_CONDITIONAL(BUILD_JACK, test "x$have_jack" = "xyes")

dnl Check for the samplerate development package - name: libsamplerate0-dev
LIBSAMPLERATE_MIN_VERSION=0.1.2
PKG_CHECK_MODULES(SAMPLERATE, samplerate >= ${LIBSAMPLERATE_MIN_VERSION},, AC_MSG_ERROR([Missing libsamplerate development files]))

dnl Check for the sndfile development package - name: libsndfile-dev
PKG_CHECK_MODULES(SNDFILE, sndfile,, AC_MSG_ERROR([Missing sndfile development files]))

dnl Coverage is default-disabled
AC_ARG_ENABLE([coverage], AS_HELP_STRING([--enable-coverage], [Enable coverage]))

AS_IF([test "x$enable_coverage" = "xyes"], [
        CXXFLAGS="${CXXFLAGS} --coverage"
        LDFLAGS="${LDFLAGS} --coverage"])

# DBUSCPP
dnl Check for dbuscpp, the C++ bindings for D-Bus
AC_ARG_WITH([dbus],
    [AS_HELP_STRING([--without-dbus], [disable support for dbus])],
    [],
    [with_dbus=yes])
AS_IF([test "x$with_dbus" = "xyes"], [
       PKG_CHECK_MODULES(DBUSCPP, dbus-c++-1,, AC_MSG_WARN([Missing dbus development files]))

       AS_AC_EXPAND(LIBDIR, $libdir)
       AC_SUBST(LIBDIR)

       AC_CONFIG_FILES([bin/dbus/Makefile
                        bin/dbus/cx.ring.Ring.service])

       AC_CHECK_PROG(HAVE_DBUSXML2CPP, dbusxx-xml2cpp, true, false)
       if test "x$HAVE_DBUSXML2CPP" = "xfalse"; then
           AC_MSG_ERROR([dbusxx-xml2cpp not found])
       fi

       AS_IF([test "x$CLANG" = "xyes"], [
             AC_MSG_ERROR([dbus does not compile when clang is used, rerun with --without-dbus])
       ]);
       AM_CONDITIONAL(RING_DBUS, true)],
       AM_CONDITIONAL(RING_DBUS, false));

# Rest C++ with restbed
AC_ARG_WITH([restcpp],
    AS_HELP_STRING([--with-restcpp], [enable rest support with C++]))

AS_IF([test "x$with_restcpp" = "xyes"], [
    PKG_CHECK_MODULES(RESTBED, librestbed,, AC_MSG_WARN([Missing restbed files]))

    AS_AC_EXPAND(SBINDIR, $sbindir)
    AC_SUBST(SBINDIR)

    AC_CONFIG_FILES([bin/restcpp/Makefile])

    AM_CONDITIONAL(RING_RESTCPP, true)],
    AM_CONDITIONAL(RING_RESTCPP, false));

dnl Check for libav
PKG_CHECK_MODULES(LIBAVCODEC, libavcodec >= 53.5.0,, AC_MSG_ERROR([Missing libavcodec development files]))
LIBAVCODEC_CFLAGS="${LIBAVCODEC_CFLAGS} -D__STDC_CONSTANT_MACROS"

PKG_CHECK_MODULES(LIBAVFORMAT, libavformat >= 54.20.3,, AC_MSG_ERROR([Missing libavformat development files]))

PKG_CHECK_MODULES(LIBSWSCALE, libswscale >= 1.1.0,, AC_MSG_ERROR([Missing libswscale development files]))

PKG_CHECK_MODULES(LIBAVDEVICE, libavdevice >= 53.0.0,, AC_MSG_ERROR([Missing libavdevice development files]))

PKG_CHECK_MODULES(LIBAVUTIL, libavutil >= 52.5.0,, AC_MSG_ERROR([Missing libavutil development files]))

dnl Video is default-enabled
AC_ARG_ENABLE([video], AS_HELP_STRING([--disable-video], [Disable video]))

AS_IF([test "x$enable_video" != "xno"],
      [
        AC_DEFINE(RING_VIDEO, [], [Video support enabled])
        AM_CONDITIONAL(RING_VIDEO, true)
        AS_IF([test "$SYS" = linux && test -z "${HAVE_ANDROID_FALSE}"],[
            PKG_CHECK_MODULES(UDEV, libudev,, AC_MSG_ERROR([Missing libudev development files]))
        ],[
        ])
    ],
    [
     AM_CONDITIONAL(RING_VIDEO, false)
     ]);

dnl check for GnuTLS
PKG_CHECK_MODULES([GNUTLS], [gnutls >= 3.4.14], [HAVE_GNUTLS=1], [HAVE_GNUTLS=0])


# PTHREAD
# required dependency(ies): libxpat
AX_PTHREAD


# SDES Key Exchange
# required dependency(ies): libpcre
AC_ARG_WITH([sdes],
    [AS_HELP_STRING([--without-sdes], [disable support for sdes key exchange])],
    [],
    [with_sdes=yes])
AS_IF([test "x$with_sdes" = "xyes"], [
      PKG_CHECK_MODULES(PCRE, [libpcre],,AC_MSG_ERROR([libpcre not found]))]);

AC_DEFINE_UNQUOTED([HAVE_SDES], `if test "x$with_sdes" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have sdes support])
AM_CONDITIONAL(BUILD_SDES, test "x$with_sdes" = "xyes" )


dnl Check for libcppunit-dev
CPPUNIT_MIN_VERSION=1.12
PKG_CHECK_MODULES(CPPUNIT, cppunit >= ${CPPUNIT_MIN_VERSION}, AM_CONDITIONAL(BUILD_TEST, test 1 = 1 ), AM_CONDITIONAL(BUILD_TEST, test 0 = 1 ))


if test "${SYS}" != "mingw32"; then
# GSM CODEC
# required dependency(ies): libgsm
dnl check for libgsm1 (doesn't use pkg-config)
dnl Check for libgsm
AC_ARG_WITH([gsm], [AS_HELP_STRING([--without-gsm],
            [disable support for gsm codec])], [], [with_gsm=yes])

LIBGSM=
AS_IF([test "x$with_gsm" != xno],
        [AC_CHECK_HEADER([gsm/gsm.h], , AC_MSG_FAILURE([Unable to find the libgsm1 headers (you may need to install the dev package).  You may use --without-gsm to compile without gsm codec support.]))]
        [AC_CHECK_LIB([gsm], [gsm_decode], [], [
            AC_MSG_FAILURE([libgsm link test failed. You may use --without-gsm to compile without gsm codec support.])
        ])
    ])

AC_DEFINE_UNQUOTED([HAVE_GSM], `if test "x$with_gsm" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libgsm])
fi
AM_CONDITIONAL(BUILD_GSM, test "x$with_gsm" = "xyes" )


# SPEEX CODEC
# required dependency(ies): libspeex
dnl Check for libspeex
AC_ARG_WITH([speex],
        [AS_HELP_STRING([--without-speex], [disable support for speex codec])],
        [],
        [with_speex=yes])

AS_IF([test "x$with_speex" != xno], [PKG_CHECK_MODULES([SPEEX], [speex])])

AC_DEFINE_UNQUOTED([HAVE_SPEEX], `if test "x$with_speex" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libspeex])
AM_CONDITIONAL(BUILD_SPEEX, test "x$with_speex" = "xyes" )


# SPEEX DSP
# required dependency(ies): libspeexdsp
dnl check in case the libspeexdsp is not installed
AC_ARG_WITH([speexdsp],
    [AS_HELP_STRING([--without-speexdsp],
    [disable support for speexdp Noise Suppression and Automatic Gain Control])],
    [],
    [with_speexdsp=yes])

AS_IF([test "x$with_speexdsp" != xno], [PKG_CHECK_MODULES([SPEEXDSP], [speexdsp])])

AC_DEFINE_UNQUOTED([HAVE_SPEEXDSP], `if test "x$with_speexdsp" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have libspeexdsp])
AM_CONDITIONAL(BUILD_SPEEXDSP, test "x$with_speexdsp" = "xyes" )

dnl iLBC is enabled if it's installed
AC_ARG_WITH([libilbc],
  [AS_HELP_STRING([--with-libilbc],
    [support ilbc audio @<:@default=check@:>@])],
  [],
  [with_libilbc=check])
AS_CASE(["$with_libilbc"],
  [yes], [PKG_CHECK_MODULES([libilbc], [libilbc], [HAVE_LIBILBC=1])],
  [no], [HAVE_LIBILBC=0],
  [PKG_CHECK_MODULES([libilbc], [libilbc], [HAVE_LIBILBC=1], [HAVE_LIBILBC=0])])
AM_CONDITIONAL([BUILD_ILBC], [test "$HAVE_LIBILBC" -eq 1])

dnl opus is enabled if it's installed
AC_ARG_WITH([opus],
  [AS_HELP_STRING([--with-opus], [support opus audio @<:@default=check@:>@])], [], [with_opus=check])
AS_CASE(["$with_opus"],
  [yes], [PKG_CHECK_MODULES([opus], [opus], [HAVE_OPUS=1])],
  [no], [HAVE_OPUS=0],
  [PKG_CHECK_MODULES([opus], [opus], [HAVE_OPUS=1], [HAVE_OPUS=0])])
AM_CONDITIONAL([BUILD_OPUS], [test "$HAVE_OPUS" -eq 1])

dnl check for openDHT
PKG_CHECK_MODULES([OPENDHT], [opendht >= 0.6.1],, AC_MSG_WARN([Missing OpenDHT]))

dnl IPv6 mode is default-disabled
AC_ARG_ENABLE([ipv6], AS_HELP_STRING([--enable-ipv6], [Enable IPv6 support]))

AC_DEFINE_UNQUOTED([HAVE_IPV6], `if test "x$enable_ipv6" = "xyes"; then echo 1; else echo 0; fi`, [Define if you have IPv6])
AM_CONDITIONAL(BUILD_IPV6, test "x$enable_ipv6" = "xyes" )

# LIBUPNP
dnl check for libupnp
AC_ARG_WITH([upnp], [AS_HELP_STRING([--without-upnp],
            [disable support for upnp])], [], [with_upnp=yes])

AS_IF([test "x$with_upnp" = "xyes"],
      [PKG_CHECK_MODULES(LIBUPNP, [libupnp],
        [AC_DEFINE([HAVE_LIBUPNP], 1, [Define if you have libupnp])],
        [AC_MSG_WARN([Missing libupnp development files])
         AC_DEFINE([HAVE_LIBUPNP], 0, [Define if you have libupnp])])
      ])

# LIBNATPMP
dnl check for libnatpmp
AC_ARG_WITH([natpmp], [AS_HELP_STRING([--without-natpmp],
            [disable support for NAT-PMP])], [], [with_natpmp=yes])

AS_IF([test "x$with_natpmp" != xno],
        [AC_CHECK_HEADER([natpmp.h],
            AC_CHECK_LIB([natpmp], [initnatpmp], [], [with_natpmp=no]),
            [AC_MSG_WARN([Unable to find the libnatpmp headers (you may need to install the dev package). You may use --without-natpmp to compile without NAT-PMP support.]);
             with_natpmp=no])
        ],[])

AC_DEFINE_UNQUOTED([HAVE_LIBNATPMP], `if test "x$with_natpmp" != xno; then echo 1; else echo 0; fi`, [Define if you have libnatpmp])

AC_DEFINE_UNQUOTED([HAVE_SHM], `if test -z "${HAVE_LINUX_TRUE}" && test -z "${HAVE_ANDROID_FALSE}" ; then echo 1; else echo 0; fi`,
    [Define if you have shared memory support])

# DOXYGEN
# required dependency(ies): doxygen
# check for doxygen, mostly stolen from http://log4cpp.sourceforge.net/
# ----------------------------------------------------------------------------
AC_DEFUN([BB_ENABLE_DOXYGEN],
    [
    AC_ARG_ENABLE(doxygen, [  --enable-doxygen        enable documentation generation with doxygen (disabled by default)])
    AC_ARG_ENABLE(dot, [  --enable-dot            use 'dot' to generate graphs in doxygen (auto)])
    if test "x$enable_doxygen" = xyes; then
        AC_PATH_PROG(DOXYGEN, doxygen, , $PATH)
        test x$DOXYGEN = x && AC_MSG_ERROR([could not find doxygen])
        AC_PATH_PROG(DOT, dot, , $PATH)
        test x$DOT = x -a "x$enable_dot" = xyes && AC_MSG_ERROR([could not find dot])
    fi
    AM_CONDITIONAL(ENABLE_DOXYGEN, test "x$DOXYGEN" != "x")
])

# Actually perform the doxygen check
BB_ENABLE_DOXYGEN


dnl debug mode is default-disabled
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Build in debug mode, adds stricter warnings, disables optimization]))

AS_IF([test "x$enable_debug" = "xyes"],
      [CXXFLAGS="${CXXFLAGS} -g -fno-omit-frame-pointer -Wall -Wextra -Wnon-virtual-dtor -O0"])

dnl What Makefiles to generate

#TODO: split this list across where the relevant check is performed
AC_CONFIG_FILES([Makefile \
                 bin/Makefile \
                 src/Makefile \
                 src/sip/Makefile \
                 src/im/Makefile \
                 src/ringdht/Makefile \
                 src/media/Makefile \
                 src/media/audio/Makefile \
                 src/media/audio/pulseaudio/Makefile \
                 src/media/audio/alsa/Makefile \
                 src/media/audio/opensl/Makefile \
                 src/media/audio/jack/Makefile \
                 src/media/audio/coreaudio/Makefile \
                 src/media/audio/portaudio/Makefile \
                 src/media/audio/sound/Makefile \
                 src/config/Makefile \
                 src/client/Makefile \
                 src/hooks/Makefile \
                 src/media/video/Makefile \
                 src/media/video/v4l2/Makefile \
                 src/media/video/androidvideo/Makefile \
                 src/media/video/osxvideo/Makefile \
                 src/media/video/winvideo/Makefile \
                 src/media/video/test/Makefile \
                 src/security/Makefile \
                 src/upnp/Makefile \
                 ringtones/Makefile \
                 man/Makefile \
                 doc/Makefile \
                 doc/doxygen/Makefile])

# Go!
AC_OUTPUT
