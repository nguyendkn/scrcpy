# WebRTC Streaming

scrcpy supports WebRTC streaming, allowing you to view and control your Android device directly from a web browser without installing any additional software.

## Features

- **Real-time streaming**: Low-latency video streaming via WebRTC
- **Web browser access**: No client installation required
- **Cross-platform**: Works on any device with a modern web browser
- **Built-in web interface**: Simple HTML5 client included
- **Network streaming**: Access from any device on the same network

## Requirements

### Build Dependencies

- **libdatachannel**: WebRTC library for C/C++
- **pthreads**: Threading support (usually available by default)

### Installation

#### Ubuntu/Debian
```bash
sudo apt install libdatachannel-dev
```

#### Build from source
```bash
git clone https://github.com/paullouisageneau/libdatachannel.git
cd libdatachannel
cmake -B build -DUSE_GNUTLS=0 -DUSE_NICE=0
cmake --build build
sudo cmake --install build
```

## Building scrcpy with WebRTC

1. **Configure with WebRTC support:**
   ```bash
   meson setup build_webrtc -Dwebrtc=true
   ```

2. **Build:**
   ```bash
   meson compile -C build_webrtc
   ```

3. **Or use the provided script:**
   ```bash
   ./build_webrtc.sh
   ```

## Usage

### Basic Usage

1. **Start scrcpy with WebRTC:**
   ```bash
   scrcpy --webrtc
   ```

2. **Open your web browser and navigate to:**
   ```
   http://localhost:8080
   ```

3. **Click "Start Stream" to begin streaming**

### Advanced Usage

#### Custom Port
```bash
scrcpy --webrtc --webrtc-port=9000
```

#### WebRTC with other options
```bash
scrcpy --webrtc --webrtc-port=8080 --max-size=1280 --bit-rate=2M
```

#### Network Access
To access from other devices on the network, use your computer's IP address:
```
http://192.168.1.100:8080
```

## Web Interface

The built-in web interface provides:

- **Video display**: Real-time video stream from your Android device
- **Connection status**: Shows WebRTC connection state
- **Simple controls**: Start/stop streaming buttons
- **Responsive design**: Works on desktop and mobile browsers

### Browser Compatibility

- **Chrome/Chromium**: Full support
- **Firefox**: Full support
- **Safari**: Full support (iOS 11+)
- **Edge**: Full support

## Network Configuration

### Firewall Settings

Make sure the WebRTC port (default 8080) is accessible:

```bash
# Ubuntu/Debian
sudo ufw allow 8080

# CentOS/RHEL
sudo firewall-cmd --add-port=8080/tcp --permanent
sudo firewall-cmd --reload
```

### STUN/TURN Servers

For connections across different networks, you may need to configure STUN/TURN servers. The default configuration uses Google's public STUN server:

```
stun:stun.l.google.com:19302
```

## Limitations

- **Audio**: Currently video-only (audio support planned for future releases)
- **Control**: Input control via web interface not yet implemented
- **Recording**: WebRTC streams cannot be recorded directly
- **Multiple clients**: Limited to 10 concurrent connections

## Troubleshooting

### Connection Issues

1. **Check firewall settings**
2. **Verify port availability:**
   ```bash
   netstat -ln | grep 8080
   ```
3. **Test with localhost first**
4. **Check browser console for errors**

### Performance Issues

1. **Reduce video quality:**
   ```bash
   scrcpy --webrtc --max-size=720 --bit-rate=1M
   ```

2. **Check network bandwidth**
3. **Close other applications using network/CPU**

### Browser Issues

1. **Enable hardware acceleration** in browser settings
2. **Clear browser cache and cookies**
3. **Try incognito/private mode**
4. **Update browser to latest version**

## Development

### Extending the Web Interface

The web interface is embedded in the scrcpy binary. To customize it:

1. **Edit the HTML template** in `app/src/webrtc_server.c`
2. **Rebuild scrcpy**
3. **Test your changes**

### Adding Features

The WebRTC implementation provides extension points for:

- **Custom signaling protocols**
- **Additional media tracks**
- **Enhanced web interface**
- **Authentication mechanisms**

## Security Considerations

- **Local network only**: By default, WebRTC server binds to localhost
- **No authentication**: The web interface has no built-in authentication
- **Unencrypted signaling**: WebSocket signaling is not encrypted by default

For production use, consider:

- **Adding HTTPS/WSS support**
- **Implementing authentication**
- **Using a reverse proxy** (nginx, Apache)
- **Restricting network access**

## Future Enhancements

Planned features for future releases:

- **Audio streaming support**
- **Input control via web interface**
- **Multiple device support**
- **Enhanced security options**
- **Mobile-optimized interface**
- **Recording capabilities**