# Amnesia-TDD-TCP

_A fork of Frictional Games' Amnesia: The Dark Descent aiming to create a TCP communication protocol for client application to interact with or control the game_

## 🎯 Project Goals

Unlike other forks of the game, our project focuses primarily on:

- 🗃️ Language agnostic interface

_Control the game from any language, any environment, you bring your own tools._

- 🧪 Developer facing functions

_Developing a standard custom story? Don't ship with amnesia-tdd-tcp, use it to make your development life easier._

## 💻 Setting up your Development Environment

The Windows game build is verified with Visual Studio 2026 and its `v145` MSVC toolset. In the Visual Studio Installer, install the **Desktop development with C++** workload and make sure these individual components are selected:

- MSVC v145 C++ x86/x64 build tools
- A Windows 10 or Windows 11 SDK

The game is a 32-bit application, so the x86 compiler and libraries are required. CMake is not required for this Windows workflow.

From a PowerShell prompt at the repository root, run:

```powershell
.\scripts\build-windows.ps1
```

The script extracts `src/HPL2/dependencies.zip` when needed, builds the HPL2 static library in Release configuration, and then builds the Amnesia game with the same toolchain. The executable is written to `artifacts/Release/Amnesia.exe`; it is not copied into a Steam installation automatically.

You can also open `src/amnesia/src/game/Lux.sln` in Visual Studio, select `Release` and `Win32`, and build the solution after extracting `src/HPL2/dependencies.zip` into `src/HPL2`.

> The bundled Autodesk FBX SDK 2012 library is tied to the Visual Studio 2010 C++ ABI. Modern Windows builds therefore omit the raw `.fbx` mesh importer. Runtime `.msh` and Collada loading, the game, and its TCP interaction remain in the build.

## 🚀 Projects using amnesia-tdd-tcp

| Name        | Technology |  Description                                     |
|-------------|------------|--------------------------------------------------|
| [**Streamnesia**](https://github.com/amnesia-spelos/streamnesia)   | C# .NET       | A web application for managing tasks.       |

_Working on something yourself? PR your project here!_ ☺️

## 💖 Help & Contribution

In the spirit of community, free and open source software, and our love for the game, we would **love** for you to help us out however you can. You don't even need to code!

Feel free to provide ideas, improvements, bug reports, or if you're technically skilled, code improvements!

Just browse through [the GitHub issues](https://github.com/amnesia-spelos/amnesia-tdd-tcp/issues) and take a pick. Or create a new one.

Thank you so much for considering helping out! 💕

Feel free to also read through our [CONTRIBUTING](CONTRIBUTING.md) and [Code of Conduct](CODE_OF_CONDUCT.md) documents.
