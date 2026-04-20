# CE Brainfuck

`cebf` is a Windows CE desktop-style Brainfuck interpreter. The port keeps the classic three-pane UI with desktop pull-down menus, but uses a CE-native implementation and build layout.

Project layout:

- [`src/main.c`](./src/main.c) contains the Windows CE application and Brainfuck engine.
- [`src/resources.rc`](./src/resources.rc) defines the application icon and pull-down menus.
- [`src/app-icon.svg`](./src/app-icon.svg) is the editable icon source.
- [`legacy/`](./legacy/) contains the six historical prototypes that were previously mixed together at the project root.

Build with:

```sh
make
```

The build uses `/opt/arm-mingw32ce/bin/arm-mingw32ce-*`, matching the other CE projects in this workspace.
