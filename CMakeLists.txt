# CMakeLists.txt: projekt CMake dla elementu NES Emulator
cmake_minimum_required (VERSION 3.24)

# Włącz przeładowywanie kodu XAML na gorąco dla kompilatorów MSVC, jeśli jest to obsługiwane.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project ("NES Emulator" CXX C)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT NESEmulator)

# --- AUTOMATYCZNE POBIERANIE SDL3 ---
include(FetchContent)

FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG        main  # Pobiera najnowszą stabilną wersję SDL3
)

# Wyłączamy budowanie testów i przykładów SDL3, aby projekt kompilował się znacznie szybciej
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

# Linkowanie statyczne — brak SDL3.dll przy exe
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

# Pobierz i dołącz SDL3 do projektu
FetchContent_MakeAvailable(SDL3)

# --- AUTOMATYCZNE POBIERANIE DEAR IMGUI ---
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(imgui)

# ------------------------------------
# ------------------------------------

# Dodaj główne źródło pliku wykonywalnego
add_executable(NESEmulator "NES Emulator.cpp")
set_property(TARGET NESEmulator PROPERTY OUTPUT_NAME "NES Emulator")

# Ustawienie standardu C++23
set_property(TARGET NESEmulator PROPERTY CXX_STANDARD 23)
set_property(TARGET NESEmulator PROPERTY CXX_STANDARD_REQUIRED ON)

# Dodaj pliki źródłowe ImGui oraz backendy (SDL3 + SDL_Renderer) do kompilacji
target_sources(NESEmulator PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)

# Dodaj ścieżki include, żeby kompilator widział nagłówki <imgui.h> oraz <imgui_impl_...h>
target_include_directories(NESEmulator PRIVATE
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

# Połącz z SDL3 statycznie
target_link_libraries(NESEmulator PRIVATE SDL3::SDL3-static)

# --- Flagi optymalizacji Release (MSVC) ---
if(MSVC)
    target_compile_options(NESEmulator PRIVATE
        $<$<CONFIG:Release>:/O2 /Oi /Gy /GL>
    )
    target_link_options(NESEmulator PRIVATE
        $<$<CONFIG:Release>:/LTCG /OPT:REF /OPT:ICF>
        "$<IF:$<CONFIG:Release>,/SUBSYSTEM:WINDOWS,/SUBSYSTEM:CONSOLE>"
        /ENTRY:wmainCRTStartup
    )
endif()