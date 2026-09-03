# just a fucking manual

`jafm` searches through **all your man pages** for a symbol, figures out which
page actually documents it, and opens it for you.

I find it genuinely insane how fucking illiterate the default `man` command can be. I was making a simple HTTP server in C and wanted the man page for `sockaddr_in`. Every single time I tried `man sockaddr_in` it went: `No manual entry for sockaddr_in`. And this *should* work, because it works on Linux. But, as it so happens, I am not on Linux all the time. The docs for `sockaddr_in` were right there, inside another page (`inet`). So I had to somehow guess which random man page contained the thing I was looking for. `jafm` fixes that. It reads every page, ranks the hits so the real one is on top, shows you a picker with a live preview, and opens whatever you choose.

```sh
$ jafm sockaddr
```

That's it. I find it genuinely unacceptable that this is not an already solved issue. Watch the demo [here](https://www.youtube.com/watch?v=ab9yrSoBfeY).

## What you get

`jafm` searches every page in your `manpath`, in parallel, in a fraction of a second. It **ranks by relevance** so the page that actually documents your symbol floats to the top, tells you *what* it found and its **kind** (`sockaddr_in (struct)`), and gives you an **interactive picker** with a live `man` **preview** on the side and the match highlighted in the page.

## Usage

```text
$ jafm --help
jafm 0.1.0 - just a fucking manual

greps your whole manpath for a symbol and opens the page that actually
documents it, because `man <symbol>` gives up way too easily.

usage: jafm [-1] [-l] [-e] [-t type] [-j threads] <symbol>
       jafm [-v | -h]

options:
  -1, --first      open the best match without asking
  -l, --list       just print what it found, open nothing
  -e, --exact      match the symbol as a whole word only
  -t, --type KIND  only show matches where the symbol is a KIND
                   (struct, union, enum, typedef, function)
  -j, --jobs N     scan with N worker threads (default 8)
  -v, --version    print version and bail
  -h, --help       print this and bail

by hachem <im@hachem.wtf>
```

In the picker: `↑`/`↓` or `k`/`j` to move, `Enter` to open, `q` to bail. Pipe
its stdin instead of a tty and it drops to a numbered prompt.

```sh
$ jafm sockaddr              # pick from the list
$ jafm -1 getentropy         # open the best match straight away
$ jafm -t struct -l sockaddr # every struct named *sockaddr*, printed
$ jafm -e -1 inet            # whole word, open the winner
```

## Build
Needs a C compiler, `zlib`, and `pthreads`. Nothing else.

```sh
$ sudo make install            # -> /usr/local/bin + man page
$ make install PREFIX=~/.local # or somewhere without sudo
```

It really is one file, so if you don't want the Makefile:

```sh
$ cc -O2 jafm.c -o jafm -lz -lpthread
```

## License
`jafm` is licensed under the MIT License. See [LICENSE](LICENSE) for more.
