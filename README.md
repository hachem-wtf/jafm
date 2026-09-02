# just a fucking manual

jafm is just a convenient utility program that searches through your man pages instead of making you find the man page yourself. I find it genuinely insane how illiterate the default `man` command can be. I was making a simple HTTP server in C, and, as you know, I wanted to look up the man page for `sockaddr_in`. But every single time I tried `man sockaddr_in`, `man` would go: `No manual entry for sockaddr_in`. And this *should* work, because it works on Linux. But, as it so happens, I am not on Linux all the time.

The documentation for `sockaddr_in` was there. It was inside another man page. So naturally, I had to somehow figure out which random man page contained the thing I was looking for. jafm fixes that. It just looks through **all your man pages**, tells you where it found your query, gives you a selector, and opens the man page for you.

```sh
$ jafm sockaddr_in
```

That's it. I find it genuinely unacceptable that this is not an already solved issue.

## License

jafm is licensed under the MIT License. See [LICENSE](LICENSE) for more information.
