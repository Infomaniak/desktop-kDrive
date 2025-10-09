# Animations

This folder contains **Lottie animation files** in `.json` format, used by the kDrive client to display lightweight, scalable vector-based animations in the user interface.

---

## 🚀 How to Add or Update an Animation

> ⚠️ This is the **only part you’ll usually need to care about.**

1. **Remove** the existing `.json` file (if it exists).  
   Example: delete `MyAnimation.json`
2. **Add** your new or updated `.lottie` file to this folder.  
   Example: add `MyAnimation.lottie`
3. **Build the project**.  
   The `.json` version will be **automatically regenerated** during the pre-build step.

---

## 🔄 How these files are generated

The `.json` animations are **automatically generated** from their corresponding `.lottie` bundles during the build process.

Since **WinUI 3** (the UI framework used by the kDrive client) currently supports only the **JSON-based Lottie format** and not the newer `.lottie` container format, these `.json` files are required to render animations properly in the application.

---

## ⚙️ Automatic conversion (Pre-build event)

During each build, the kDrive project runs the PowerShell script  
[`lottieConverter.ps1`](../scripts/lottieConverter.ps1)  
as a **pre-build event**.

This script scans the `Animations` directory for `.lottie` files and converts them into `.json` files using the following process:

1. Each `.lottie` file (a ZIP-based archive) is temporarily renamed to `.zip`.
2. The archive is extracted to a temporary folder.
3. The first animation found inside the `animations/` subfolder is selected.
4. That animation is copied into this `Animations` folder and renamed to match the original file name, but with a `.json` extension.
5. All temporary files and folders are cleaned up automatically.

As a result, developers **do not need to run any manual conversion step** — the `.json` files are kept up to date automatically as part of the build.

---

## 🧩 Usage in the application

Each `.json` animation file can be loaded directly by the Lottie player used in the kDrive client.  
Example (with `Infomaniak.kDrive.CustomControls.LottiePlayer`):

```xml
...
     xmlns:controls="using:Infomaniak.kDrive.CustomControls"
...

<controls:LottiePlayer UriSource="ms-appx:///Animations/Loading.json" />
````

---

## 📁 Folder structure

```
Animations/
│
├── Loading.lottie   ← your source animation file
├── Loading.json     ← auto-generated during build
├── Success.lottie   ← your source animation file
├── Success.json     ← auto-generated during build
└── README.md        ← you are here
```
---

**Last updated:** October 2025
