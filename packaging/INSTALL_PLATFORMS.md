# LunaRead Desktop — Cross-Platform Installation & Build Guide

LunaRead is designed to run natively with first-class performance on **Windows**, **macOS**, and **Linux**.

---

## 1. Windows Installation (Windows 10 / 11 64-bit)

### Pre-built Packages
Windows users can download either of the following packages directly from the GitHub Releases page:
- **`LunaRead-1.0.0-Windows-x64-Setup.exe` (Recommended)**:
  - Standard Windows installer (NSIS).
  - Automatically creates Start Menu shortcuts, Desktop icon, file associations (`.pdf`, `.epub`, `.mobi`, `.cbz`), and an uninstaller.
- **`LunaRead-1.0.0-Windows-x64-Portable.zip`**:
  - Standalone portable archive containing `lunaread-desktop.exe` and all bundled Qt DLLs.
  - No administrator privileges required. Unzip anywhere (e.g., USB drive or Desktop) and run `lunaread-desktop.exe`.

### Building on Windows from Source
If building on a Windows computer:
1. **Requirements:**
   - Visual Studio 2022 (Community or higher) with "Desktop development with C++"
   - Qt 6.5+ (Qt 6.7 recommended) via the Qt Online Installer
   - CMake 3.22+
2. **Build with Batch Script:**
   - Double-click [`packaging/windows/build-windows.bat`](file:///home/iredox/Desktop/space/lunaread-desktop-qt/packaging/windows/build-windows.bat) or run from Developer Command Prompt.
3. **Build with PowerShell:**
   ```powershell
   .\packaging\windows\build-windows.ps1 -QtDir "C:\Qt\6.7.2\msvc2022_64"
   ```
   This automatically compiles the project, runs `windeployqt` to gather all required DLLs, and builds the NSIS installer `.exe` and portable `.zip`.

---

## 2. macOS Installation (Apple Silicon & Intel)

### Pre-built Packages
macOS users can download the native disk image from GitHub Releases:
- **`LunaRead-1.0.0-macOS-Universal.dmg` (Recommended)**:
  - Open the `.dmg` file.
  - Drag and drop `LunaRead.app` into the **Applications** folder.
  - Launch LunaRead from Spotlight, Launchpad, or the Applications folder.
- **`LunaRead-1.0.0-macOS.zip`**:
  - Unpack the `.zip` to extract `LunaRead.app`.

### Building on macOS from Source
If building on a Mac:
1. **Requirements (via Homebrew):**
   ```bash
   brew install qt@6 libarchive sqlite cmake
   ```
2. **Run the macOS Build Script:**
   ```bash
   ./packaging/macos/build-macos.sh
   ```
   This builds `LunaRead.app`, invokes `macdeployqt` to bundle the Qt frameworks, and generates `LunaRead.dmg`.

---

## 3. Linux Installation (Fedora, RHEL, Ubuntu, Debian, Arch)

### Pre-built Packages (located in [`dist/`](file:///home/iredox/Desktop/space/lunaread-desktop-qt/dist))
- **RPM Package (Fedora / RHEL / CentOS / openSUSE):**
  ```bash
  sudo dnf install ./dist/lunaread-desktop-1.0.0-Linux.rpm
  # or
  sudo rpm -i ./dist/lunaread-desktop-1.0.0-Linux.rpm
  ```
- **Universal Tarball (Any Linux Distribution):**
  ```bash
  tar -zxvf ./dist/lunaread-desktop-1.0.0-Linux.tar.gz
  ./lunaread-desktop-1.0.0-Linux/bin/lunaread-desktop
  ```
- **Automated Shell Installer:**
  ```bash
  # Local user installation (~/.local/bin):
  ./dist/install.sh

  # System-wide installation (/usr/local/bin):
  sudo ./dist/install.sh
  ```
- **Portable Direct Execution:**
  ```bash
  ./dist/lunaread.sh "/path/to/book.pdf"
  ```

---

## 4. Automated Cloud Builds (GitHub Actions CI/CD)

The repository includes a ready-to-use GitHub Actions workflow at [`.github/workflows/build-and-release.yml`](file:///home/iredox/Desktop/space/lunaread-desktop-qt/.github/workflows/build-and-release.yml).

### How to generate fresh installers for all platforms:
1. **Push commits or create a git tag:**
   ```bash
   git tag v1.0.0
   git push origin v1.0.0
   ```
2. **Or trigger manually:**
   - In GitHub, go to the **Actions** tab.
   - Select **Build & Package (Windows, macOS, Linux)**.
   - Click **Run workflow**.
3. **Download Installers:**
   - GitHub automatically compiles the code on native Microsoft Windows, Apple macOS, and Ubuntu Linux runners simultaneously.
   - The installer artifacts are available under the workflow run and automatically published to the GitHub Release.
