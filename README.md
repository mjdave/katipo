# Katipo
Katipo is a new type of decentralized network, designed to be as simple as possible, and useful in ways that the web isn't currently.

The Katipō is an endangered venomous spider from New Zealand. The name seemed a good fit.

![Katipo browser for iOS icon](https://github.com/mjdave/katipo/blob/main/katipoBrowser-iOS128.png)

# How to install and run

Katipo requires the tui submodule, which itself requires the glm submodule. To fetch these, after checking out this repository you can simply run in the root katipo directory:
```
git submodule update --init --recursive
```

Katipo builds and runs on macOS, Windows, and most unix based systems. XCode and Visual studio projects are provided, along with a build script using cmake.

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
./katipoHost
```
```
cd apps/katipoClient
./build.sh
./katipoClient
```

All going well, the client should print the test message from the host.

# macOS
Xcode projects are provided within each app directory. The cmake build scripts also work.

# Windows
Visual Studio solutions are provided within each app directory. Cmake has not been tested on windows.

# Explanation and Mission Statement

The web was originally created by academics and nerds with the needs of end users (other academics and nerds) being the main focus. These nerds created bulletin boards, hosting content for a handful of others who dialed in on their phone lines. Later we had GeoCities, a free hosting service where anyone could learn some basic HTML and have their own website. It exploded in popularity but was killed by Yahoo's profit seeking.

With the release of the iPhone, the web rapidly became available to billions of people. It was looking exciting - the web would be available to everyone, except that these new people were not given the tools. They were only allowed to be consumers. The first web was run by nerds for nerds. The second web was run by business for business.

Today, personal or community websites are rare, and are run by a dying breed of enthusiasts. Now our communications and our memories, our photos and our videos, our precious data belongs to Meta, Apple, Microsoft, or Google, stored behind walls of ads or subscriptions. The web infrastructure is now too complex for individuals or small organizations to operate, and it is owned and controlled nearly exclusively by large for-profit businesses.

We have been pushed out of the open web, and into closed platforms, and the profit seeking businesses in control right now want to keep it that way forever. If we want to escape, to take back any control and start heading in a better direction, at some point we have to start actually doing something. 

Katipo is an open source, free, people-focused alternative, being built from the ground up. Katipo's goal is to make software more accessible, cheaper, and more useful to more people. Katipo can help us to take back control of our digital lives, our data, our apps, and our connections.

It can achieve this by focusing on simplicity. 

Katipo will make it much easier to create, host and share your own sites, apps and data. You no longer need to rent a server, or set up port forwarding on your router, and you don't need a domain name. You don't need to learn 4 different languages or plug the whole thing into an AI model to understand it, you don't need to submit anything to be approved for an app store, and you don't need to install a disk image on a virtual machine to run it.

Instead, with Katipo, you can host a site/app/file on any device. You could launch a temporary hosting app from within the Katipo browser on an iPhone for example, and then permitted clients could immediately connect to your server and view your content, play a multiplayer game, chat or share files.

How this works is that when you host, you give it the address of a tracker server, which then acts like proxy for the host. The trackers can be public or private, can handle 1000s of hosts and clients, and can connect to other trackers. Clients then need an IP address of any tracker to connect to, and then they can search through connected hosts and other trackers, and then communicate securely with any available host via chains of trackers. 

For now, this repository only contains the core proof of concept networking library and command line apps. I have started working on a full Katipo browser client with a Vulkan based renderer, and I plan to release this too in the future. This will allow the sharing of apps, games, music and images, and will feature a built in tui editor and the ability to host.

PLEASE NOTE! Currently there is no encryption or Tui sandboxing. This will be worked on soon, but for now, you should not use Katipo to transmit sensitive data or to connect to trackers or hosts that you do not trust.

Katipo is written in c++ and tui. Tui is a simple scripting language and serialization library which replaces html, javascript, css, and json in the Katipo stack. Katipo uses enet for low level networking over UDP. This is not necessarily always going to be the case, but it is a good fit for now.

There is a lot more to explain, and to work on too, this is a quick overview. Katipo is still in the very early stages of development (as of Jan 2026).

I am very keen for help. If I am to continue alone, my focus will be on getting this and tui hardened in my own coding projects, with a goal to releasing cross platform Katipo browsers and apps within the next year or so. 

But I will get more inspired and move faster if people join me. I think maybe there are enough of us not motivated by greed that we can make and use something better. 

We can do that together, here, now.

No AI is used here.

--Dave
- [dave@afk.nz](mailto:dave@afk.nz)
- [@majicdave.bsky.social](https://bsky.app/profile/majicdave.bsky.social)