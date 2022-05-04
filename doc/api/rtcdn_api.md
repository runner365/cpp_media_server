# rtcdn api接口
rtcdn的webrtc http接口规范，是国内webrtc爱好者提出，在国内的一些开源sfu共同约定实现的接口规范。

## 1. rtcdn api规范
文档地址: [rtcdn draft](https://github.com/rtcdn/rtcdn-draft)

参考rtcdn draft, 具体实现如下。

## 2. 实现
### 2.1 publish api
方法: http post

uri: /rtc/v1/publish

post json:
```markup
    {
        streamurl: 'webrtc://domain/app/stream',
        sdp: string,  // offer sdp
        clientip: string
    }
```
其中：
* app: 为roomId
* stream: 为userid

response data:
```markup
    {
        "code": 0,
        "server": "cpp_media_server",
        "sdp": sdp,
        "sessionid": sessoinid //sessionid for the publish
    }
```

### 2.2 play api
方法: http post

uri: /rtc/v1/play

post json:
```markup
    {
        streamurl: 'webrtc://domain/app/stream',
        sdp: string,  // offer sdp
        clientip: string
    }
```
其中：
* app: 为roomId
* stream: 为userid

response data:
```markup
    {
        "code": 0,
        "server": "cpp_media_server",
        "sdp": sdp,
        "sessionid": sessoinid //sessionid for the play
    }
```

### 2.3 unpublish api(可选)
unpublish的操作，可以在客户端直接进行PeerConnection.Close()，或者直接关闭web网页；

本http api方法用于优雅关闭推流。

方法: http post

uri: /rtc/v1/unpublish

post json:
```markup
    {
        streamurl: 'webrtc://domain/app/stream',
        sessionid:string // 推流时返回的唯一id
    }
```
其中：
* app: 为roomId
* stream: 为userid

返回:
```markup
    {
        code: 0,
        msg: "ok"
    }
```

### 2.4 unplay api(可选)
unplay的操作，可以在客户端直接进行PeerConnection.Close()，或者直接关闭web网页；

本http api方法用于优雅关闭拉流。

方法: http post

uri: /rtc/v1/unplay

post json:
```markup
    {
        streamurl: 'webrtc://domain/app/stream',
        sessionid:string // 推流时返回的唯一id
    }
```
其中：
* app: 为roomId
* stream: 为userid

返回:
```markup
    {
        code: 0,
        msg: "ok"
    }
```

## 3. 代码实现
提供客户端web demo: [web rtcdn sdk](https://github.com/runner365/webrtc-client-sdk/tree/rtcdn)

webrtc sfu服务: [cpp_media_server](https://github.com/runner365/cpp_media_server)