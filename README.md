# OpenLF1 for Game Boy Advance

Little Fighter (1995) for the Game Boy Advance, based on [OpenLF1](https://github.com/OpenLF2/OpenLF1).

## Get the ROM

Open the [web page](https://openlf2.github.io/OpenLF1-gba/) and press **Download GBA ROM**. It fetches the original
game and builds the ROM in your browser. Nothing is uploaded.

Do not share the ROM. It contains the original game's graphics and sounds, which are not free to redistribute.

## Controls

| Button | Menus | Fight |
| --- | --- | --- |
| D-pad | move, change a value | move the fighter |
| A | confirm | attack |
| B | back | jump |
| L / R | change the number of fighters | |
| Start | start | pause |
| Select | | leave the fight |

## Build from source

Needs `arm-none-eabi-gcc` 12 or newer, `make` and Python 3.

```sh
make ZIP=/path/to/Little_Fighter_1995.zip
```

The result is `build/openlf1.gba`.

## License

The code is MIT-licensed ([LICENSE](LICENSE)). This license does not cover the original game or its assets.

## The original game

Little Fighter was created by Marti Wong and Starsky Wong. Visit the
[original Little Fighter website](https://www.lf2.net/en/intro.html) and support them with
[Little Fighter 2 Remastered](https://www.lf2.net/) or on its
[Steam page](https://store.steampowered.com/app/3249650/).

## Trademarks

Little Fighter, Nintendo and Game Boy Advance are names or marks of their respective rights holders. They are used
here only to identify the game or the platform. This is an independent project and is not affiliated with or
endorsed by Nintendo or the creators of Little Fighter.
