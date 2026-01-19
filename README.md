# SAHFE
## Spatial Algorithmic Hashing Fusion Engine

SAHFE is a 3D fusion system developed as a part of a senior project

## Requirements and Setup
The development environment of this project is containerized to fascillitate repeatable and long-term development as this project is handed off to other people who have much better things to do than worrying about `gcc` versioning and if `pandas` is accessible in a working Jupyter `python` kernel. These instructions are up to date as of: **01/18/26**, please keep them up to date accordingly!

> Getting Podman

Podman is a free, open-source alternative to Docker that is pretty much a drop in replacement, without any of the silly requirements that Docker puts on you. If you already have Docker, the container and compose files should work fine, but you might have to make modifications for paths, and the following fixes for errors may not work for you!

1. Visit the Podman Desktop website [here](https://podman-desktop.io/) to download and install Podman
2. Once its downloaded, run the installer and make sure that you check to also download `podman` and `podman-compose`

> Building the container

Reboot VsCode and when prompted, hit open / enter container. Podman and the Containerfile will do the rest to manage your environmenet, and any library dependencies will be handled by CMake

> For Linux / MacOS

If you are having problems with the container properly mounting the files for the project, try adding a file in `.devcontainer` called `.env` and adding this line:

`PODMAN_USERNS=keep-id`

Save, and reboot VsCode

> Error: Docker Required to use Container Tools

VsCode will look for Docker by default and not Podman.

1. go to settings `Cntrl` + `,`
2. Look for `Dev > Containers: Docker Path`, update it to podman
3. (if needed) also update `Dev > Containers: Docker-Compose Path`

> Error: Error response from daemon: fill out specgen: getting absolute path of \\wsl.localhost\Ubuntu\mnt\wslg\runtime-dir\wayland-0: unsupported UNC path

This is VSCode trying to look for WSL GUI support which Podman doesnt need or support for our purposes, turn it off

1. go to settings `Cntrl` + `,`
2. look for `Dev > Containers: Wayland Socket`, set it to `none`
3. save and reboot VS code

> I have opened my container fine, but VSCode isnt tracking my edited files right with git!

Git is either mad about permissions or the crlf / lf discrepencies between a Windows machine and any other machine. Open a terminal *in the container*, and run:

`git config core.autocrlf input`
`git config core.filemode false`

In the git tab on the left, refresh and it should be fixed