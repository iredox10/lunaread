cmake_minimum_required(VERSION 3.25)
project(LunaReadDesktop VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable compiler optimization and warning flags
if(MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()

# Qt6 Configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Sql
    Network
    Svg
    Test
    OpenGL
    OpenGLWidgets
)

# Optional SQLite and Archive libraries
find_package(SQLite3 REQUIRED)
find_package(ZLIB REQUIRED)
find_package(LibArchive REQUIRED)

# Project Include Directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SQLite3_INCLUDE_DIRS}
    ${LibArchive_INCLUDE_DIRS}
)

# Core Sources
set(LUNAREAD_CORE_SOURCES
    src/core/ThemeTokens.cpp
    src/db/DatabaseManager.cpp
    src/utils/NaturalSort.cpp
    src/utils/EncodingDetector.cpp
    src/utils/HashUtils.cpp
    src/parsers/ParserFactory.cpp
    src/parsers/TxtParser.cpp
    src/parsers/EpubParser.cpp
    src/parsers/PdfParser.cpp
    src/parsers/DjVuParser.cpp
    src/parsers/MobiParser.cpp
    src/parsers/Fb2Parser.cpp
    src/parsers/DocxParser.cpp
    src/parsers/OdtParser.cpp
    src/parsers/RtfParser.cpp
    src/parsers/HtmlParser.cpp
    src/parsers/ChmParser.cpp
    src/parsers/UmdParser.cpp
    src/parsers/ComicParser.cpp
    src/parsers/MarkdownParser.cpp
    src/reader/Typesetter.cpp
    src/reader/PageCurlRenderer.cpp
    src/reader/PageCurlOpenGLWidget.cpp
    src/reader/ContinuousScrollRenderer.cpp
    src/reader/DualPageRenderer.cpp
    src/reader/GestureController.cpp
    src/reader/DesktopShortcuts.cpp
    src/reader/ReaderViewport.cpp
    src/reader/DogEarBookmarkWidget.cpp
    src/reader/ReadingHeaderFooterWidget.cpp
    src/features/AnnotationManager.cpp
    src/features/AnnotationExporter.cpp
    src/features/TtsService.cpp
    src/features/DictionaryService.cpp
    src/features/EyeStrainTimerService.cpp
    src/sync/WebDavSyncService.cpp
    src/opds/OpdsClient.cpp
    src/ui/BookshelfView.cpp
    src/ui/BookCardWidget.cpp
    src/ui/MainWindow.cpp
    src/ui/TtsControlBar.cpp
    src/ui/SearchInBookDialog.cpp
    src/ui/TocDrawerWidget.cpp
    src/ui/FontSettingsDialog.cpp
    src/ui/SystemTrayManager.cpp
    src/ui/DictionaryDialog.cpp
    src/ui/AutoScrollOverlayWidget.cpp
    src/ui/ReadingRulerWidget.cpp
    src/ui/ThemeCustomizerDialog.cpp
    src/ui/BookDetailsDialog.cpp
    src/ui/EyeRestDialog.cpp
    src/ui/ReadingStatsDialog.cpp
    src/ui/WebDavSyncDialog.cpp
    src/ui/TextSelectionMenuWidget.cpp
    src/ui/BookshelfFilterSortWidget.cpp
    src/ui/QuickNoteDialog.cpp
    src/features/BackupRestoreService.cpp
    src/ui/BackupRestoreDialog.cpp
    src/ui/OpdsBrowserDialog.cpp
    src/ui/AnnotationExportDialog.cpp
    src/ui/NightFilterOverlayWidget.cpp
    src/ui/KeyboardShortcutsDialog.cpp
    src/ui/BookListRowWidget.cpp
    src/ui/LocalFilesBrowserDialog.cpp
    src/features/TextFilterManager.cpp
    src/features/MetadataFetcherService.cpp
    src/features/PdfPageService.cpp
    src/features/PrivacyLockService.cpp
    src/features/CustomFontManager.cpp
    src/features/BookExporter.cpp
    src/features/ReadingProfileManager.cpp
    src/features/AmbientSoundService.cpp
    src/features/ImageCropService.cpp
    src/ui/BookExporterDialog.cpp
    src/ui/SpeedReadingDialog.cpp
    src/ui/AllAnnotationsDialog.cpp
    src/ui/VocabularyNotebookDialog.cpp
    src/ui/AmbientSoundDialog.cpp
    src/ui/ReadingProfileDialog.cpp
    src/ui/PrivacyLockDialog.cpp
    src/ui/TextReplacementDialog.cpp
    src/ui/QuickNavPopoverWidget.cpp
    src/ui/TapZoneConfigDialog.cpp
)

# Core Headers
set(LUNAREAD_CORE_HEADERS
    src/core/Types.h
    src/core/ThemeTokens.h
    src/db/DatabaseManager.h
    src/utils/NaturalSort.h
    src/utils/EncodingDetector.h
    src/utils/HashUtils.h
    src/parsers/BookParser.h
    src/parsers/ParserFactory.h
    src/parsers/TxtParser.h
    src/parsers/EpubParser.h
    src/parsers/PdfParser.h
    src/parsers/DjVuParser.h
    src/parsers/MobiParser.h
    src/parsers/Fb2Parser.h
    src/parsers/DocxParser.h
    src/parsers/OdtParser.h
    src/parsers/RtfParser.h
    src/parsers/HtmlParser.h
    src/parsers/ChmParser.h
    src/parsers/UmdParser.h
    src/parsers/ComicParser.h
    src/parsers/MarkdownParser.h
    src/reader/Typesetter.h
    src/reader/PageCurlRenderer.h
    src/reader/PageCurlOpenGLWidget.h
    src/reader/ContinuousScrollRenderer.h
    src/reader/DualPageRenderer.h
    src/reader/GestureController.h
    src/reader/DesktopShortcuts.h
    src/reader/ReaderViewport.h
    src/reader/DogEarBookmarkWidget.h
    src/reader/ReadingHeaderFooterWidget.h
    src/features/AnnotationManager.h
    src/features/AnnotationExporter.h
    src/features/TtsService.h
    src/features/DictionaryService.h
    src/features/EyeStrainTimerService.h
    src/features/BackupRestoreService.h
    src/sync/WebDavSyncService.h
    src/opds/OpdsClient.h
    src/ui/BookshelfView.h
    src/ui/BookCardWidget.h
    src/ui/MainWindow.h
    src/ui/TtsControlBar.h
    src/ui/SearchInBookDialog.h
    src/ui/TocDrawerWidget.h
    src/ui/FontSettingsDialog.h
    src/ui/SystemTrayManager.h
    src/ui/DictionaryDialog.h
    src/ui/AutoScrollOverlayWidget.h
    src/ui/ReadingRulerWidget.h
    src/ui/ThemeCustomizerDialog.h
    src/ui/BookDetailsDialog.h
    src/ui/EyeRestDialog.h
    src/ui/ReadingStatsDialog.h
    src/ui/WebDavSyncDialog.h
    src/ui/TextSelectionMenuWidget.h
    src/ui/BookshelfFilterSortWidget.h
    src/ui/QuickNoteDialog.h
    src/ui/BackupRestoreDialog.h
    src/ui/OpdsBrowserDialog.h
    src/ui/AnnotationExportDialog.h
    src/ui/NightFilterOverlayWidget.h
    src/ui/KeyboardShortcutsDialog.h
    src/ui/BookListRowWidget.h
    src/ui/LocalFilesBrowserDialog.h
    src/features/TextFilterManager.h
    src/features/MetadataFetcherService.h
    src/features/PdfPageService.h
    src/features/PrivacyLockService.h
    src/features/CustomFontManager.h
    src/features/BookExporter.h
    src/features/ReadingProfileManager.h
    src/features/AmbientSoundService.h
    src/features/ImageCropService.h
    src/ui/BookExporterDialog.h
    src/ui/SpeedReadingDialog.h
    src/ui/AllAnnotationsDialog.h
    src/ui/VocabularyNotebookDialog.h
    src/ui/AmbientSoundDialog.h
    src/ui/ReadingProfileDialog.h
    src/ui/PrivacyLockDialog.h
    src/ui/TextReplacementDialog.h
    src/ui/QuickNavPopoverWidget.h
    src/ui/TapZoneConfigDialog.h
)

# Executable target
add_executable(lunaread-desktop
    src/main.cpp
    ${LUNAREAD_CORE_SOURCES}
    ${LUNAREAD_CORE_HEADERS}
)

target_link_libraries(lunaread-desktop PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Svg
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    SQLite3::SQLite3
    ZLIB::ZLIB
    LibArchive::LibArchive
)

# Testing target
enable_testing()
add_executable(lunaread-tests
    tests/test_main.cpp
    tests/test_utils.cpp
    tests/test_parsers.cpp
    tests/test_db.cpp
    tests/test_opds.cpp
    tests/test_sync.cpp
    ${LUNAREAD_CORE_SOURCES}
    ${LUNAREAD_CORE_HEADERS}
)

target_link_libraries(lunaread-tests PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Sql
    Qt6::Network
    Qt6::Svg
    Qt6::Test
    Qt6::OpenGL
    Qt6::OpenGLWidgets
    SQLite3::SQLite3
    ZLIB::ZLIB
    LibArchive::LibArchive
)

add_test(NAME AllTests COMMAND lunaread-tests)

# Dedicated Database Test target
add_executable(test-db
    tests/test_db_main.cpp
    tests/test_db.cpp
    src/db/DatabaseManager.cpp
    src/db/DatabaseManager.h
    src/core/Types.h
)

target_link_libraries(test-db PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Sql
    Qt6::Test
    SQLite3::SQLite3
)

add_test(NAME DbTests COMMAND test-db)


# Standalone OPDS test target
add_executable(lunaread-opds-tests
    tests/test_opds.cpp
    src/opds/OpdsClient.cpp
    src/opds/OpdsClient.h
)
target_compile_definitions(lunaread-opds-tests PRIVATE STANDALONE_OPDS_TEST)
target_link_libraries(lunaread-opds-tests PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Test
)
add_test(NAME OpdsTests COMMAND lunaread-opds-tests)

# Standalone PageCurl OpenGL test target
add_executable(test-page-curl
    tests/test_page_curl_standalone.cpp
    tests/test_page_curl_opengl.cpp
    src/reader/PageCurlOpenGLWidget.cpp
    src/reader/PageCurlOpenGLWidget.h
    src/core/ThemeTokens.cpp
    src/core/ThemeTokens.h
)
target_link_libraries(test-page-curl PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    Qt6::OpenGL
    Qt6::OpenGLWidgets
)
add_test(NAME PageCurlOpenGLTest COMMAND test-page-curl)

# Standalone AutoScroll Overlay test target
add_executable(test-auto-scroll
    tests/test_auto_scroll_standalone.cpp
    tests/test_auto_scroll.cpp
    src/ui/AutoScrollOverlayWidget.cpp
    src/ui/AutoScrollOverlayWidget.h
    src/core/ThemeTokens.cpp
    src/core/ThemeTokens.h
)
target_link_libraries(test-auto-scroll PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
)
add_test(NAME AutoScrollTest COMMAND test-auto-scroll)


