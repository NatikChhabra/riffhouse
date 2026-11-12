# Licences.txt: the licence of Riffhouse and of every part built into it, put beside the app by the
# installer. Made from the licence files of the sources this build fetched, so it can never drift.
set(_notices "${CMAKE_BINARY_DIR}/Licences.txt")
set(_eigen "${namcore_SOURCE_DIR}/Dependencies/eigen")
file(WRITE "${_notices}" "Riffhouse is free software under the GNU Affero General Public License 3.0 or later.\n"
    "Its source code, and the source of every part listed here, is at https://github.com/NatikChhabra/riffhouse\n"
    "Below: the licence of Riffhouse, then each part it is built with.\n")

function(_riffhouse_notice title file)
    if (EXISTS "${file}")
        file(READ "${file}" _text)
        file(APPEND "${_notices}" "\n\n==============================================================================\n${title}\n==============================================================================\n\n${_text}")
    else()
        message(WARNING "Riffhouse: licence text missing for ${title}: ${file}")
    endif()
endfunction()

_riffhouse_notice("Riffhouse (AGPL-3.0-or-later)" "${CMAKE_SOURCE_DIR}/LICENSE")
_riffhouse_notice("JUCE (used under the AGPL-3.0)" "${juce_SOURCE_DIR}/LICENSE.md")
_riffhouse_notice("Neural Amp Modeler Core (MIT)" "${namcore_SOURCE_DIR}/LICENSE")
_riffhouse_notice("Eigen (MPL 2.0; source at https://gitlab.com/libeigen/eigen)" "${_eigen}/COPYING.MPL2")
_riffhouse_notice("nlohmann/json (MIT, Copyright 2013-2025 Niels Lohmann)" "${CMAKE_CURRENT_LIST_DIR}/notices/nlohmann-json-MIT.txt")
_riffhouse_notice("Signalsmith Stretch (MIT)" "${signalsmith_stretch_SOURCE_DIR}/LICENSE.txt")
_riffhouse_notice("Signalsmith Linear (MIT)" "${signalsmith_linear_SOURCE_DIR}/LICENSE.txt")
_riffhouse_notice("LAME MP3 encoder 3.100 (LGPL 2.0 or later; source at https://lame.sourceforge.io)" "${lame_SOURCE_DIR}/COPYING")
_riffhouse_notice("Fonts: Inter (SIL Open Font License 1.1)" "${CMAKE_SOURCE_DIR}/resources/fonts/OFL-Inter.txt")
_riffhouse_notice("Fonts: Instrument Sans, Doto, Big Shoulders, Kaushan Script, Alfa Slab One (SIL Open Font License 1.1)" "${CMAKE_SOURCE_DIR}/resources/fonts/OFL-others.txt")
_riffhouse_notice("Fonts: Yellowtail (Apache License 2.0)" "${_eigen}/COPYING.APACHE")
