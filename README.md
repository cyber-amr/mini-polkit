# mini-polkit

**mini-polkit** is a secure and minimal PolicyKit authentication agent designed for lightweight and embedded Linux environments. It provides a secure, modular and customizable way to interact with PolicyKit authorization requests via external prompt tools.

> 🔒 Built for simplicity. Designed for power users.

---

## Features

- Minimal codebase written in pure `C` (307 LOC)
- No runtime bloat, only `glib` and `polkit`
- Easy integration with `dmenu`, `rofi`, `zenity` or any custom script
- Performant and highly secure password handling (no lingering in memory)
- X11 and Wayland ready, depending on your prompt manager :)
- Suitable for both tiling window managers and floating setups (and maybe TTYs?)

## Performance (on my ancient system)

| Benchmark                  |             |
|----------------------------|:-----------:|
| **Language**               | C           |
| **Lines of Code**          | 307         |
| **Memory Usage (RSS)**     | ≤5.6 MB     |
| **Startup Time**           | ≤15 ms      |
| **Authentication Latency** | ≤40 ms      |
| **Process Count**          | 1           |
| **D-Bus Roundtrips**       | 1           |

---

## Usage

When `PolKit` requests authentication, `mini-polkit` runs your command, reads the stdout as the password, then securely passes it to PolKit via D-Bus — thats it :3

To show the `PolKit` message inside your prompt, use `{{MESSAGE}}`:

```sh
mini-polkit "zenity --password --title='{{MESSAGE}}'"
```

Run **mini-polkit** in the background (e.g. using &), to make it act as a full-time agent

```sh
mini-polkit "zenity --password" &
```

You can autostart it via `.xinitrc`, `.bash_profile`, `systemd` or whatever you like. As long as your tool writes the password to stdout, you're good.

---

## Installation

### Clone, build and install manually:

(deps. gcc, make, pkg-config, polkit-agent-1, glib->v2.0)

```sh
git clone https://github.com/cyber-amr/mini-polkit.git
cd mini-polkit
doas make install
```

---

## Security Notes

* Password is securely passed to PolicyKit via DBus and wrecked right afterwards from memory.
* Input is read from stdin to avoid command-line exposure.
* It is **your responsibility** to ensure the prompt tool handles input securely (e.g., `dmenu` with `-P`, `rofi` with `-password`).

---

## License

MIT License — [`LICENSE`](/LICENSE)
