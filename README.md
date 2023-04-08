<br>

<div align="center">
  <a href="https://pengu.lol">
    <img src="https://i.imgur.com/kQOMxqS.jpg" width="144"/>
  </a>
  <h1 align="center">Pengu Loader</h1>
  <p align="center">
    <strong>A JavaScript plugin loader</strong> for the League of Legends Client
  </p>
  <p>
    <a href="https://pengu.lol">
      <img src ="https://img.shields.io/badge/-pengu.lol-EC1C24.svg?&style=for-the-badge&logo=Authy&logoColor=white"/>
    </a>
    <a href="https://chat.pengu.lol">
      <img src ="https://img.shields.io/badge/-Join%20Discord-5c5fff.svg?&style=for-the-badge&logo=Discord&logoColor=white"/>
    </a>
    <a href="https://github.com/PenguLoader/PenguLoader">
      <img src="https://img.shields.io/github/stars/PenguLoader/PenguLoader.svg?style=for-the-badge" />
    </a>
    <a href="./LICENSE">
      <img src ="https://img.shields.io/github/license/PenguLoader/PenguLoader.svg?style=for-the-badge"/>
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

**Pengu Loader**, previously known as [**League Loader**](https://github.com/PenguLoader/PenguLoader/tree/league-loader), is a **plugin loader** specifically designed for the **League of Legends Client**.

Pengu Loader enables you to load **JavaScript** plugins into the Client as dependencies. This allows you to personalize the Client's appearance, load custom content, add new features, and enhance your overall experience. With Pengu Loader, you can build a smarter and more customized Client that suits your needs and preferences.

## Features
- Customize the League Client using plugins
- Personalize and theme your Client
- Support for modern JavaScript features
- Built-in and remote DevTools support
- Simplified LCU API usage

## Getting Started

Please visit the homepage to begin:

### 👉 https://pengu.lol/

## Documentation

- [Pengu Docs](https://pengu.lol/guide/welcome)
- [API docs](./API_DOCS.md)
- [Migration to v1](./MIGRATION_TO_V1.md)
- [Insecure options](./INSECURE_OPTIONS.md)

## Contributing

To contribute to the project, follow these steps:
1. Fork the repository [(click here to fork now)](https://github.com/PenguLoader/PenguLoader/fork)
2. Create your feature branch `feat/<branch-name>`
3. Commit your changes
4. Push to the branch
5. Submit a new Pull Request

### Ways you can contribute

- **Documentation and website**: The documentation can always be improved. If you find something that is not documented or could be enhanced, create a PR for it. Check out the [PenguLoader organization](https://github.com/PenguLoader) for more information.
- **Additional Base/Starter plugins**: Share your plugins along with a detailed guide to help beginners get started easily.
- **Core features**: Ensure you have extensive experience with CEF and low-level programming skills.
- **JavaScript features**: Extensive web development knowledge is required.

### Project structure

- **Loader** - The main loader menu UI, written in C# and WPF XAML.
- **Core** - The core module (DLL) that hooks into libCEF to enable the plugin's magic.
- **Plugins**: Templates for plugin development beginners.

### Build from source

#### Prerequisites
- Visual Studio 2017
  - Desktop development with C++
  - .NET desktop development
  - Windows 8.1 SDK
- NodeJS 16+
- pnpm

#### Initial Steps
- Clone the repository:
   - `git clone https://github.com/PenguLoader/PenguLoader`
- Update submodules: 
   - `cd PenguLoader`
   - `git submodule update --init --recursive`

#### Build Steps for Pengu Loader
  - Open `pengu-loader.sln`
  - Right-click on the solution -> `Restore Nuget Packages`
  - Set the architecture to `x86`
  - Right-click on each project -> `Build`

#### Build Steps for the `@default` plugin:
  - Make sure you have NodeJS and pnpm installed on your system.
  - Navigate to the `plugins/@default` directory: `cd PenguLoader/plugins/@default`
  - Install the plugin dependencies using pnpm: `pnpm install`
  - Build the plugin using pnpm: `pnpm build`

## License
[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader?ref=badge_large)