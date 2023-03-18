<br>

<div align="center">
  <a href="https://pengu.lol">
    <img src="https://i.imgur.com/kQOMxqS.jpg" width="144"/>
  </a>
  <h1 align="center">Pengu Loader</h1>
  <p align="center">
    <strong>A JavaScript plugin loader</strong> for League of Legends Client
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

## About

**Pengu Loader** (formerly [**League Loader**](https://github.com/PenguLoader/PenguLoader/tree/league-loader)) is a **plugin loader** designed specifically for the **League of Legends Client** (League Client).

With Pengu Loader, you can load **JavaScript** plug-ins into the Client as dependencies, which can help you personalize the look and feel of the Client, load your custom content, add new features, and improve your overall experience. It also allows you to build a smarter Client that fits your needs and preferences.

## Features
- Customize League Client with plugins
- Theme/personalize your Client
- Support for modern JavaScript features
- Support for built-in and remote DevTools
- Easier to work with LCU API

## Getting started

Please visit the homepage to get started.

### 👉 https://pengu.lol/

## Documentation

- [Pengu Docs](https://pengu.lol/guide/welcome)
- [API docs](./API_DOCS.md)
- [Migration to v1](./MIGRATION_TO_V1.md)
- [Insecure options](./INSECURE_OPTIONS.md)

## Contributing

Follow these steps to contribute to the project:
1. Fork it [(click here to fork now)](https://github.com/PenguLoader/PenguLoader/fork)
2. Create your feature branch `feat/<branch-name>`
3. Commit your changes
4. Push to the branch
5. Create a new Pull Request

### Ways you can contribute

- **documentation and website** - the documentation always needs some work, if you discover that something is not documented or can be improved you can create a PR for it, check out [PenguLoader org](https://github.com/PenguLoader)
- **more base/starter plugins** - push your plugins with a detailed guide to help beginners get started with ease
- **core features** - make sure you have a lot of experience with CEF and low-level programming skills
- **javascript features** - you need too much webdev knowledge

### Project structure

- **loader** - main loader menu UI, written in C# and WPF XAML
- **core** - core module (DLL), it hooks libCEF to make everything magical
- **plugins** - templates for plugin dev beginner

### Build from source

This project requires Visual Studio 2017 with these components:
- Desktop development with C++
- .NET desktop development
- Windows 8.1 SDK

> You can also use VS 2019+ and another SDK version.

Close the repo and update submodules

```
git clone https://github.com/PenguLoader/PenguLoader.git
git submodule update --init --recursive
```

Build steps:
  1. Open **pengu-loader.sln**
  2. Right click on the solution -> **Restore Nuget Packages**
  3. Set arch to **x86**
  4. Right click on each project -> **Build**

To build the @default plugin requires, you need:
- NodeJS 16+
- pnpm

```
cd plugins/@default
pnpm install
pnpm build
```

## License

[![FOSSA Status](https://app.fossa.com/api/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader.svg?type=large)](https://app.fossa.com/projects/git%2Bgithub.com%2Fnomi-san%2Fleague-loader?ref=badge_large)
