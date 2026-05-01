# pitjs+wickjs+cplot+jrw demo

An interactive data visualization demo combining lightweight libraries below:

- [pitjs](https://github.com/rscrsc/pit-js) - Reactive Web Components for Unix minds (signals, effects, components)
- [wickjs](https://github.com/rscrsc/wick-js) - 2D canvas drawing library
- cplot - C plotting library (generates BMP files)
- jrw - C json serdes library

## Features

- Interactive point plotting with mouse clicks
- Noise data generation
- Real-time statistics (avg, min, max)
- Multiple views: Plot, Stats, Help
- Export plots as BMP files
- WebSocket-based real-time communication
- Toast notifications for user feedback

## Dependencies

This demo requires **websocketd** to run. The C backend communicates via stdin/stdout, and websocketd bridges this to a WebSocket server for the browser.

Install websocketd:
```bash
# macOS
brew install websocketd

# Linux
wget https://github.com/joewalnes/websocketd/releases/download/v0.4.1/websocketd-0.4.1-linux_amd64.zip
unzip websocketd-0.4.1-linux_amd64.zip
sudo mv websocketd /usr/local/bin/
```

## Build

```bash
make
```

## Run

```bash
make run
```

Then open `http://localhost:8080` in your browser.

## Controls

| Key  | Action                |
|------|-----------------------|
| click | Add point at mouse    |
| r     | Reset all data        |
| g     | Generate noise        |
| p     | Save plot.bmp         |
| 1     | Switch to Plot view   |
| 2     | Switch to Stats view  |
| 3     | Switch to Help view   |

## Project Structure

```
.
├── include/
│   ├── cplot.h      # C plotting library
│   ├── jr.h         # JSON reader
│   └── jw.h         # JSON writer
├── src/
│   └── main.c       # Backend (WebSocket stdin/stdout)
├── ui/
│   ├── index.html   # Frontend application
│   └── lib/
│       ├── pit.js   # Reactive framework
│       ├── wick.js  # Canvas drawing
│       └── lit-html.js # HTML template literals
└── Makefile
```

## License

0BSD - See [LICENSE](LICENSE)
