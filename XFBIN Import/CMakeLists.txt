cmake_minimum_required(VERSION 3.20)
project(xfbinimport-max LANGUAGES CXX)

# ------------------------------------------------------------
#  C++17, nicht 20.
#
#  Gebraucht wird davon nur std::to_chars fuer die locale-feste
#  Zahlenausgabe; alles andere kommt mit weniger aus. C++17 laesst
#  sich dafuer auch gegen die Header der aelteren Max-SDKs
#  uebersetzen - ab 2028 waere das nicht mehr sicher.
#
#  Geprueft: die Dumps sind mit C++17 zeichengleich mit denen aus
#  einem C++20-Build.
# ------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Fuer welche Max-Version gebaut wird. Steuert nur Kleinigkeiten,
# der Quelltext ist versionsunabhaengig.
set(MAX_VERSION "0" CACHE STRING "3ds Max Jahrgang, z.B. 2016")

# ============================================================
#  Pfade (werden von BAUE_ALLE.bat per -D uebergeben)
# ============================================================

set(3DSMAX_SDK_DIR "C:/Program Files/Autodesk/3ds Max 2025 SDK/maxsdk"
    CACHE PATH "3ds Max SDK root (maxsdk-Ordner)")

# ------------------------------------------------------------
#  Das eigenstaendige Werkzeug xfbindump.exe braucht KEIN Max SDK.
#  Genau das ist der Punkt: der Parser laesst sich damit gegen die
#  Python-Lib gegenpruefen, ohne Max ueberhaupt zu starten.
#
#  Ausschalten mit:  -DXFBIN_BUILD_TOOL=OFF
# ------------------------------------------------------------
option(XFBIN_BUILD_TOOL "Eigenstaendiges xfbindump.exe mitbauen" ON)

# Nur das Werkzeug bauen, ohne installiertes Max SDK:
#   cmake -B build -A x64 -DXFBIN_BUILD_PLUGIN=OFF
option(XFBIN_BUILD_PLUGIN "Max-Plugin bauen (braucht das Max SDK)" ON)

# Strenge Warnungen. Standard AN.
#
# Falls die Max-SDK-Header einer bestimmten Version mit /permissive-
# selbst nicht durchgehen (das kommt bei aelteren SDKs vor), einfach
# ausschalten - der Rest bleibt davon unberuehrt:
#     set XFBIN_STRICT=OFF
#     BAUE_ALLE.bat
option(XFBIN_STRICT "Strenge Compiler-Warnungen (/W4 /permissive-)" ON)

# ============================================================
#  Gemeinsame Parser-Quellen
#
#  Bewusst als eigene Liste statt als eigene Bibliothek: das
#  Plugin braucht /Zp8 (Struct-Packing des Max SDK), das Werkzeug
#  nicht. Zwei Targets mit denselben Quellen sind hier einfacher
#  und ehrlicher als eine geteilte Bibliothek mit zwei
#  Packing-Varianten.
# ============================================================

set(XFBIN_PARSER_SOURCES
    src/xfbin_reader.h
    src/xfbin_reader.cpp
    src/xfbin_clump.h
    src/xfbin_clump.cpp
    src/xfbin_nud.h
    src/xfbin_nud.cpp
    src/xfbin_anm.h
    src/xfbin_anm.cpp
    src/xfbin_tex.h
    src/xfbin_tex.cpp
)

# ============================================================
#  Plugin-Target -> XfbinImport.dlu
# ============================================================

if(XFBIN_BUILD_PLUGIN)

    if(NOT EXISTS "${3DSMAX_SDK_DIR}/include/max.h")
        message(FATAL_ERROR
            "3ds Max SDK nicht gefunden unter: ${3DSMAX_SDK_DIR}\n"
            "Entweder -D3DSMAX_SDK_DIR=... setzen oder mit "
            "-DXFBIN_BUILD_PLUGIN=OFF nur das Werkzeug bauen.")
    endif()

    add_library(XfbinImport SHARED
        ${XFBIN_PARSER_SOURCES}
        src/xfbinimport.h
        src/xfbinimport.cpp
        src/xfbinimport_dll.cpp
        xfbinimport.def
    )

    set_target_properties(XfbinImport PROPERTIES
        SUFFIX      ".dlu"
        PREFIX      ""
        OUTPUT_NAME "XfbinImport"
    )

    # 3ds Max laeuft ausschliesslich mit der dynamischen CRT (/MD).
    # Ein versehentliches /MT ergibt getrennte Heaps und damit
    # Abstuerze, die erst weit spaeter auffallen.
    set_property(TARGET XfbinImport PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    # x64-Pflichtcheck - Max 2022+ gibt es nur als 64 Bit.
    if(CMAKE_SIZEOF_VOID_P LESS 8)
        message(FATAL_ERROR "XfbinImport laesst sich nur als x64 bauen (cmake -A x64).")
    endif()

    # Als "external" einbinden: dann rauschen die SDK-Header nicht bei /W4.
    target_include_directories(XfbinImport SYSTEM PRIVATE "${3DSMAX_SDK_DIR}/include")
    target_link_directories(XfbinImport PRIVATE "${3DSMAX_SDK_DIR}/lib/x64/Release")

    target_link_libraries(XfbinImport PRIVATE
        core.lib
        maxutil.lib
        bmm.lib
        Geom.lib
        mesh.lib
        Maxscrpt.lib
        MNMath.lib
        Paramblk2.lib
        comctl32.lib
    )

    target_compile_definitions(XfbinImport PRIVATE
        _UNICODE UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )

    if(MSVC)
        # /Zp8 ist beim Max SDK Pflicht - das Struct-Packing muss passen,
        # sonst liest das Plugin die SDK-Strukturen falsch aus.
        target_compile_options(XfbinImport PRIVATE
            /EHsc
            /Zp8
            /bigobj
            /utf-8
            /Zc:__cplusplus
            /external:W0
        )

        # ------------------------------------------------------
        #  /permissive- nur, wenn die SDK-Header es aushalten
        #
        #  Es geht dabei NICHT um den eigenen Quelltext, sondern um
        #  die Header von Autodesk. /permissive- schaltet laut
        #  Microsoft-Doku /Zc:referenceBinding, /Zc:strictStrings
        #  und /Zc:rvalueCast auf konformes Verhalten - und genau
        #  daran scheitern aeltere SDKs:
        #
        #    strictStrings -> C2440 in ParamDimension.h
        #    rvalueCast    -> C2102 in inode.h, iTreeVw.h, maxapi.h
        #    ausserdem     -> C2216 friend/static in winutil.h
        #
        #  Eine Jahresgrenze waere geraten - 2016 bis 2018 gingen
        #  ohne den Schalter durch, 2019 nicht, und wo genau die
        #  Grenze liegt, steht nirgends verbindlich. Also wird
        #  einmal ausprobiert: max.h mit /permissive- uebersetzen
        #  und schauen, ob es klappt. Das Ergebnis ist im Cache,
        #  kostet also nur beim ersten Konfigurieren Zeit, und
        #  stimmt fuer jedes SDK - auch fuer kuenftige.
        # ------------------------------------------------------
        set(_permissive_ok FALSE)

        if(XFBIN_STRICT)
            include(CheckCXXSourceCompiles)

            set(CMAKE_REQUIRED_INCLUDES  "${3DSMAX_SDK_DIR}/include")
            set(CMAKE_REQUIRED_FLAGS     "/permissive- /Zp8 /EHsc /bigobj")
            set(CMAKE_REQUIRED_DEFINITIONS
                -D_UNICODE -DUNICODE -DWIN32_LEAN_AND_MEAN -DNOMINMAX)
            set(CMAKE_REQUIRED_QUIET TRUE)

            check_cxx_source_compiles(
                "#include <max.h>\nint main() { return 0; }"
                XFBIN_SDK_LIKES_PERMISSIVE)

            unset(CMAKE_REQUIRED_INCLUDES)
            unset(CMAKE_REQUIRED_FLAGS)
            unset(CMAKE_REQUIRED_DEFINITIONS)
            unset(CMAKE_REQUIRED_QUIET)

            if(XFBIN_SDK_LIKES_PERMISSIVE)
                set(_permissive_ok TRUE)
            endif()
        endif()

        if(XFBIN_STRICT AND _permissive_ok)
            message(STATUS "SDK vertraegt /permissive- - Konformitaetspruefung an")
            target_compile_options(XfbinImport PRIVATE
                /W4
                /permissive-
                /wd4100   # unreferenzierter formaler Parameter (SDK-Callbacks)
                /wd4458   # Deklaration verdeckt Klassenmember (SDK-Makros)
            )
        elseif(XFBIN_STRICT)
            message(STATUS "SDK-Header vertragen /permissive- nicht - nur /W4")
            target_compile_options(XfbinImport PRIVATE
                /W4
                /wd4100
                /wd4458
            )
        else()
            target_compile_options(XfbinImport PRIVATE /W3)
        endif()

        target_link_options(XfbinImport PRIVATE /DEBUG /OPT:REF /OPT:ICF)
    endif()

endif() # XFBIN_BUILD_PLUGIN

# ============================================================
#  Werkzeug-Target -> xfbindump.exe  (ohne Max SDK)
# ============================================================

if(XFBIN_BUILD_TOOL)

    add_executable(xfbindump
        ${XFBIN_PARSER_SOURCES}
        tools/xfbindump_main.cpp
    )

    if(MSVC)
        set_property(TARGET xfbindump PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        target_compile_options(xfbindump PRIVATE /EHsc /utf-8 /Zc:__cplusplus)
        if(XFBIN_STRICT)
            target_compile_options(xfbindump PRIVATE /W4 /permissive-)
        endif()
        target_compile_definitions(xfbindump PRIVATE
            _UNICODE UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
    else()
        target_compile_options(xfbindump PRIVATE -Wall -Wextra)
    endif()

endif() # XFBIN_BUILD_TOOL

message(STATUS "XfbinImport: Max ${MAX_VERSION}  Plugin=${XFBIN_BUILD_PLUGIN}  "
               "Werkzeug=${XFBIN_BUILD_TOOL}  Strict=${XFBIN_STRICT}  C++${CMAKE_CXX_STANDARD}")
