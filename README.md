<br>

<div align="center">
  <a href="https://pengu.lol">
    <img src="https://i.imgur.com/kQOMxqS.jpg" width="144"/>
  </a>
  <h1 align="center">Pengu Loader</h1>
  <p align="center">
    A <strong>JavaScript plugin loader</strong> for the League of Legends Client
  </p>
  <p>
    <a href="https://pengu.lol">
      <img src ="https://img.shields.io/badge/-pengu.lol-607080.svg?&style=for-the-badge&logo=gitbook&logoColor=white"/>
    </a>
    <a href="https://chat.pengu.lol">
      <img src ="https://img.shields.io/discord/1069483280438673418?style=for-the-badge&logo=discord&logoColor=white&label=discord&color=5c5fff"/>
    </a>
    <a href="https://github.com/PenguLoader/PenguLoader">
      <img src="https://img.shields.io/github/stars/PenguLoader/PenguLoader.svg?style=for-the-badge&logo=github" />
    </a>
  </p>
</div>

<br>

## Table of Contents

- [About](#about)
- [Features](#features)
- [Getting Started](#getting-started)
- [Documentation](#documentation)
- [Contributing](#contributing)
  - [Ways to Contribute](#ways-you-can-contribute)
  - [Project Structure](#project-structure)
- [Build from Source](#build-from-source)
- [License](#license)

## About

**Pengu Loader**, previously known as
[**League Loader**](https://github.com/PenguLoader/PenguLoader/tree/league-loader),
is a **plugin loader** specifically designed for the **League of Legends
Client**.

Pengu Loader enables you to load **JavaScript** plugins into the Client as
dependencies. This allows you to personalize the Client's appearance, load
custom content, add new features, and enhance your overall experience. With
Pengu Loader, you can build a smarter and more customized Client that suits your
needs and preferences.

## Features

- Customize the League Client with plugins
- Personalize and theme your Client
- Support for modern JavaScript features
- Built-in and remote DevTools support
- Simplified LCU API usage
- Deep support for RCP hooks

## Getting Started

Please visit the homepage to begin:

### 👉 https://pengu.lol/

## Documentation

- [Pengu docs](https://pengu.lol/guide/welcome)
- [Insecure options](https://github.com/PenguLoader/PenguLoader/blob/256dfa8412e5b9973ff1caeb4bb1b1d6346978d8/INSECURE_OPTIONS.md)

## Contributing

To contribute to the project, follow these steps:

1. Fork the repository
   [(click here to fork now)](https://github.com/PenguLoader/PenguLoader/fork)
2. Create your feature branch `feat/<branch-name>`
3. Commit your changes
4. Push to the branch
5. Submit a new Pull Request

### Ways you can contribute

- **Documentation and website**: The documentation can always be improved. If
  you find something that is not documented or could be enhanced, create a PR
  for it. Check out the
  [PenguLoader organization](https://github.com/PenguLoader) for more
  information.
- **Additional Base/Starter plugins**: Share your plugins along with a detailed
  guide to help beginners get started easily.
- **Translations**: Add your new language and translations.
- **Core features**: Ensure you have extensive experience with CEF and low-level
  programming skills.
- **JavaScript features**: Extensive web development knowledge is required.

### Project structure

- **loader** - The main loader menu UI, written in C# and WPF XAML.
- **core** - The core module (DLL) that hooks into libCEF to enable the plugin's
  magic.
- **plugins**: The built-in plugins/preload scripts that support loading user
  plugins and enable RCP hooking.

## Build from source

### Prerequisites

- Visual Studio 2022, with these components:
  - .NET desktop development
  - Desktop development with C++
  - Windows 10 SDK
- **NodeJS** 18+ and **pnpm** 8+

### Initial Steps

- Clone the repository:
  ```
  git clone https://github.com/PenguLoader/PenguLoader
  ```

- Update submodules:
  ```
  cd PenguLoader
  git submodule update --init --recursive
  ```

### Build Steps for preload scripts:

- Make sure you have NodeJS and pnpm installed.

- Navigate to the `plugins` directory:
  ```
  cd plugins
  ```

- Install dependencies and build:
  ```
  pnpm install
  pnpm build
  ```

### Build Steps for Pengu Loader

- Open `pengu-loader.sln`
- Right-click on the solution -> `Restore Nuget Packages`
- Set the architecture to `x64` and build mode to `Release`
- Right-click on each project -> `Build`

> For developing and debugging purpose, you should set build mode to `Debug` and
> build preload scripts with `pnpm build-dev`.

## Disclaimer

THE PROGRAM IS PROVIDED “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGMENT, OR OF FITNESS FOR A PARTICULAR PURPOSE. LICENSOR DOES NOT WARRANT THAT THE FUNCTIONS CONTAINED IN THE PROGRAM WILL MEET YOUR REQUIREMENTS OR THAT OPERATION WILL BE UNINTERRUPTED OR ERROR FREE. LICENSOR MAKES NO WARRANTIES RESPECTING ANY HARM THAT MAY BE CAUSED BY MALICIOUS USE OF THIS SOFTWARE. LICENSOR FURTHER EXPRESSLY DISCLAIMS ANY WARRANTY OR REPRESENTATION TO AUTHORIZED USERS OR TO ANY THIRD PARTY.

Pengu Loader isn't endorsed by Riot Games and doesn't reflect the views or opinions of Riot Games or anyone officially involved in producing or managing Riot Games properties. Riot Games, and all associated properties are trademarks or registered trademarks of Riot Games, Inc.

## License

[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader?ref=badge_large)
