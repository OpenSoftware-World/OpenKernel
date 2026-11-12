# OpenKernel

## What is it, in a nutshell?

OpenKernel is a modern, modular kernel project designed with a structure that can be easily modified, built on the vision of enabling people to easily create their own operating systems.

## Why the name OpenKernel?

I chose the name OpenKernel to emphasize the kernel’s open-source nature and to highlight that others can easily use and modify it.

## What exactly is this project and what isn’t it?

What it is:

* OpenKernel is a kernel that can be easily modified.
* It was developed using a modern, next-generation development style.

What it is not:

* It is not a ready-made operating system, but you can easily build your own operating system using it.
* It is not a fully modern kernel; in fact, it lacks many modern kernel features.
* It is not reliable or stable. (The reason I say this is that this kernel does not yet include some of the necessary features.)

## What is my goal?

My goal is to build a kernel project that people can easily modify and customize to write their own operating systems, while also ensuring it is highly secure.

## So, what development method am I using?

In the past, I used a small-scale AI-assisted development method in a question-and-answer format with “ChatGPT,” an OpenAI AI model. In other words, I would write the core part of the code, ChatGPT would edit and improve it, and then I would review it and make further corrections. Now, however, I wanted to increase my use of AI in this project a bit because I believe I’ve been able to achieve the results I want with my “AI-SDS” project. So, what development method will I follow now? From now on, I’ll comprehensively outline the concept for the necessary development of OpenKernel; the AI will handle the actual development, after which I’ll review the output and make some adjustments. But don’t worry this won’t turn into a project where the AI writing the code, so there’s no need to worry. For some features, I’ll write the code myself; I’ll have the AI make adjustments, which I’ll then review or in some cases, I’ll write the code entirely myself, leaving the AI with very little to do. In short, instead of being biased against AI, I’ll provide it with a comprehensive system, use it as a team member, and make the necessary adjustments.

## What am I aiming to achieve with OpenKernel using my new-generation development method?

My goal is to make the OpenKernel project truly modern and stable, and to make it usable by others.

## 📖 Documentation

A complete documentation set lives in the [`docs/`](docs/) folder. Start at the [documentation index](docs/README.md), which explains what each file covers and gives the recommended reading order.

| Start here | |
|---|---|
| [Introduction](docs/01-introduction.md) | What OpenKernel is and what it is not |
| [Getting Started](docs/02-getting-started.md) | Toolchain, setup, bootstrapping, build and run |
| [Architecture](docs/03-architecture.md) | Directory layout, Makefile, linker script |
| [Project Status](docs/09-project-status.md) | Honest report of what currently works and what does not |

> **Note:** the project does not currently build or boot on its own. Read [09 - Project Status](docs/09-project-status.md) before you start.

## Star History

<a href="https://www.star-history.com/?repos=OpenSoftware-World%2FOpenKernel&type=date&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=OpenSoftware-World/OpenKernel&type=date&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=OpenSoftware-World/OpenKernel&type=date&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=OpenSoftware-World/OpenKernel&type=date&legend=top-left" />
 </picture>
</a>
