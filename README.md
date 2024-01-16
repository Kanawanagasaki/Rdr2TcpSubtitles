# RDR2 TCP Subtitles

A modification for Red Dead Redemption 2 that sets up a tcp server and allows clients to receive subtitle data from it

This repository includes a script for OBS that establishes a connection to a tcp server, listens for subtitles and output it as closed captions on your stream

### Installation:

1. Download **ScriptHookRDR2** V2 from [here](https://www.nexusmods.com/reddeadredemption2/mods/1472) and place the `ScriptHookRDR2.dll` file into your RDR2 root folder
2. Download **Mod Loader** from [here](https://www.nexusmods.com/reddeadredemption2/mods/1472) and place the `dinput8.dll` file into your RDR2 root folder
3. Download **RDR2TcpSubtitles mod** from [here](https://github.com/Kanawanagasaki/Rdr2TcpSubtitles/releases) and place the `Rdr2TcpSubtitles.asi` file into your RDR2 root folder
4. Download **RDR2TcpSubtitles obs script** from [here](https://github.com/Kanawanagasaki/Rdr2TcpSubtitles/releases). Then, open OBS, navigate to Tools > Scripts, and add `rdr2-obs-closed-captions-tcp-client.lua` script. Check `Enable` box to enable the script
5. In Red Dead Redemption set **Subtitles** setting to `off`
6. If you're planning to stream on <span style="color: #ff0000">YouTube</span> and want to use closed captions, you'll need to adjust some settings before you start your stream. Go to the **YouTube Studio page > Stream settings tab**, and make sure to turn on **Closed captions**. Caption source must be `Embedded 608/708`
