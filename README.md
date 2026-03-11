# Katipo
Katipo is a new type of network, a small, free and open platform that lives entirely outside of the existing HTML/CSS/JS/Apache/Chrome based web.

The [Katipo Browser](https://github.com/mjdave/katipoBrowser) is built on top of Katipo. It can be thought of as a mixture between a web browser and a game engine. You can make games, you can make apps and sites, and Katipo makes it easy to share, load, use, and modify apps and games made by others.

The Katipō is an endangered venomous spider from New Zealand.

![Katipo browser for iOS icon](https://github.com/mjdave/katipo/blob/main/katipoBrowser-iOS128.png)

Katipo is being built from first principles by hand without the use of AI. Katipo's goal is to help make a new generation of software that is created and used by real people for meaningful things.

Katipo makes it much easier to create, host and share your own sites, apps and data. From within the [Katipo Browser](https://github.com/mjdave/katipoBrowser) app, which is available now as a pre-alpha, you will be able to easily host a site/app/file from any device. Devices will be able to automatically find each other if they are on the same network, or are near by over bluetooth. Public connections are also easy to set up via exposed ports, or optional shared "tracker" based services.

Both site hosts and clients are private, they are actually both clients. Katipo uses a tracker based system to provide networking between clients and hosts that can both be behind firewalls, without the need to open any ports. How this works is that one or more trackers sit between the host and the client, and act like a proxy. You can make small private networks with a single tracker, or (once implemented) chain trackers together to create much larger networks.

Trackers are the "servers", the public facing hub with ports open for incoming connections. However a tracker is little more than a virtual router, shuffling encrypted data packets between many hosts and clients.

Katipo also provides excellent security with end to end encryption between all parties at all times. Hosts can't track clients between sessions, trackers can't read any of the data going between hosts and clients, and the majority of the data will be stored on client devices. Katipo will also support optional encrypted tracker based backups (built-in cloud storage) and sync between devices, and it will be very close to password-less.

This repository contains the (functioning) core networking library. A free Vulkan/SDL3 based open source [Katipo Browser](https://github.com/mjdave/katipoBrowser) client app is being worked on and will be released for Windows, Mac, and iOS soon, with more platforms planned. The browser will allow both browsing/searching of sites, and hosting of sites and trackers. Tracker based features like data backups, port scanning for other trackers on the same network, and in/outboxes to support direct messaging and chat are planned.

PLEASE NOTE! For now, you should not use Katipo to transmit sensitive data or to connect to trackers or hosts that you do not trust. The design and general implementation is secure, end to end encryption works. However there are some loose ends, and likely to be remaining exploitable bugs - as is this case with nearly all software - but a few more than normal to start with.

![Katipo network diagram](https://github.com/mjdave/katipo/blob/main/katipoBasicNetwork.png)

Katipo replaces the transport and application layers in the standard IP model. The Katipo transport layer is based on UDP, and uses enet, with only IPv4 support (for now).

The Katipo application layer uses the [tui](https://github.com/mjdave/tui) interpreted programming language (also made by the Katipo developer). Configuration and code are mostly written in tui, with the option of using C++ or interfacing with any other language. Tui configuration files are very similar to json, and tui code is similar to lua.

# Current status

Katipo is still missing a few key features and some of the components don't quite fit. But it is a functional proof of concept, and it's good enough for a few things already. Over time it will be good enough for more.

End to end encryption has just been added with libsodium, and the source repository has a couple of working examples, a fileServer and a basic example that returns a data table.

All 3 apps have been compiled and tested on macos 26, Windows 11, and Ubuntu running under WSL.

# How to install and run

Katipo requires the tui submodule, which itself requires the glm submodule. To fetch these, after checking out this repository you can simply run in the root katipo directory:
```
git submodule update --init --recursive
```

Katipo builds and runs on macOS, Windows, and most unix based systems. XCode and Visual Studio projects are provided, along with a build script using cmake.

# Linux

You will need to install cmake and the basic build tools if you don't have them already:

```
sudo apt-get install build-essential cmake
```

There are three command line apps to build
```
cd apps/katipoTracker
./build.sh
./katipoTracker
```
```
cd apps/katipoHost
./build.sh
./katipoHost --site exampleSites/basicSite
```
You can now connect to the host from within the Katipo Browser app, by navigating to "basicSite", or 127.0.0.1/basicSite, or you can use the command line client:
```
cd apps/katipoClient
./build.sh
./katipoClient --get basicSite
```


All going well, the client should print the test message from the host.

# macOS
Xcode projects are provided within each app directory. The cmake build scripts also work.

# Windows
Visual Studio solutions are provided within each app directory. Cmake has not been tested on windows.

# Host HTTP get requests (optional)
There is an option to add a http.get method for the host. This allows communication with a lot of existing services over http, but requires curl.

If you would like the host to be able to make http get requests, you will need curl. This is already installed on macos, on windows you are on your own for now, and on linux you can install libcurl developer libraries with:
```
sudo apt-get install libcurl4-openssl-dev
```
Enabling this also (for now) requires setting COMPILE_WITH_HTTP_INTERFACE to 1 in apps/katipoHost/Controller.h and uncommenting the line #set(KATIPO_COMPILE_WITH_HTTP_INTERFACE TRUE) in apps/katipoHost/CmakeLists.txt

# Explanation and Mission Statement

The web was originally created by academics and nerds with the needs of end users (other academics and nerds) being the main focus. These nerds created bulletin boards, hosting content for a handful of others who dialed in on their phone lines. Later we had GeoCities, a free hosting service where anyone could learn some basic HTML and have their own website. It exploded in popularity but was killed by Yahoo's profit seeking.

With the release of the iPhone, the web rapidly became available to billions of people. It was looking exciting, except that these new people were not given the tools. They were only allowed to be consumers. The first web was run by nerds for nerds. The second web was run by business for business.

Today, personal or community websites are rare, and are run by a dying breed of enthusiasts. Now our communications and our memories, our photos and our videos, our precious data belongs to Meta, Apple, Microsoft, or Google, stored behind walls of ads or subscriptions. The web infrastructure is now too complex for individuals or small organizations to operate at a low level, and it is owned and controlled nearly exclusively by large for-profit businesses.

We have been pushed out of the open web, and into closed platforms, and the profit seeking businesses in control right now want to keep making it worse. If we want to escape, to take back any control and start heading in a better direction, at some point we have to start actually doing something. Now feels like a really good time.

So I'm Dave, and that's what motivates me to work on Katipo. But I think many of you are still wondering what makes me so arrogant to even attempt this? Is this even real or has Dave gone insane?

It's very much real, and I'm probably insane. Really I just saw it was possible and necessary, and that I could build it and that nobody else seemed to be doing that. 

I have been making my own engines for my games for 20+ years, and have learned some things. I have a very solid understanding of every part of making 3D games from scratch, from the low level hardware interactions to high level design and publishing. Game engines are very much like browsers, and Katipo is a natural evolution of my multiplayer game engines. I just saw this was a good idea, for me, maybe for others, and I went for it.

I am very much aware that Katipo is not perfect. Some of the early decisions need to be reworked, some things need to be replaced, there are bugs and room for optimization, and a lot is still missing. 

There are some things that I feel strongly about, that are very core to this design, like end to end encryption, client based data storage, one single data/code language. But the actual implementation details just need to be simple and clear. I don't really mind what technologies this is built on, and in fact the best solution would be to build them all with Tui/C++ only on top of low level system libraries. But the ones I have chosen so far are all excellent, and tend to best fit within these general goals, and I think if you were looking to contribute, these would be good to keep in mind:

Code in this project should ideally be
1) Easily human readable/understandable
2) Close to the metal
3) Easy to compile and run with very few dependencies
4) Small

But sometimes you just have to use whatever you can to get the job done too.

In general I am very keen for help and support. From here, I think Katipo is hungry for sites to host. Todo list apps, notes, book readers, photo libraries, syncing apps between platforms, and of course games! 

Katipo also needs support for more platforms. If you are interested in building and releasing an Android Katipo Browser for example, or would like to help improve the build systems and create proper libraries for Tui and Katipo, or would like to help in any way at all really, please do get in touch!

I want to make it really clear too, this project is released under the Unlicense license, which means you don't even have to credit me, just don't blame me. I don't mind at all if you want to compile and release your own Katipo Browser if you like, fork away. Use any of this stuff for anything, use it to make money, but don't repeat history.

No AI is used here, and no AI support will be added by me. But again, please fork if that's your thing.

--Dave
- [dave@afk.nz](mailto:dave@afk.nz)
- [@majicdave.bsky.social](https://bsky.app/profile/majicdave.bsky.social)