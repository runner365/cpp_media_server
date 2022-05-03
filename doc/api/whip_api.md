# WHIP接口

## 1. 什么是whip
全称: WebRTC-HTTP ingestion protocol (WHIP).

rfc地址: [rfc-draft-murillo-whip-00](https://datatracker.ietf.org/doc/html/draft-murillo-whip-00)

简单说，就是通过HTTP接口能导入webrtc媒体流。

## 2. webrtc publish/unpublish
### 2.1 webrtc publish
webrtc推流接口

方法: http post

uri: http://hostname:hostport/publish/*roomId*/*uid*

http post body: offerSdp

返回:
http body: asswerSdp

举例，向host=192.168.1.98:8090, roomId=2001, 自己作为uid=6547推流.

url为: http://192.168.1.98:8090/publish/2001/6547

post data为offerSdp

返回data为answerSdp

### 2.2 webrtc unpublish
本方法为优雅的关闭。

也可以暴力的关闭可以直接在客户端进行PeerConnection.Close(), 或者暴力关闭网页；

webrtc关闭推流接口

方法: http post

uri: http://hostname:hostport/unpublish/*roomId*/*uid*

http post body: null

返回:
```markup
http body: 
{
    "code": 0,
    "desc": "ok"
}
```
举例，向host=192.168.1.98:8090, roomId=2001, 自己作为uid=6547关闭推流.

url为: http://192.168.1.98:8090/unpublish/2001/6547

post data为空

返回data为
```markup
{
    "code": 0,
    "desc": "ok"
}
```
## 3. webrtc subscribe
### 3.1 webrtc subscribe
webrtc拉流接口：

方法: http post

uri: http://hostname:hostport/subscribe/*roomId*/*uid*/*remoteUid*

其中uid为自己的uid，remoteUid为想要订阅的远端uid。

http post body: offerSdp

返回:
http body: asswerSdp

举例，向host=192.168.1.98:8090, roomId=2001, 自己作为uid=6547，从远端remoteUid=4489拉流.

url为: http://192.168.1.98:8090/subscribe/2001/6547/4489

post data为offerSdp

返回data为answerSdp

### 3.2 webrtc unsubscribe
本方法为优雅的关闭。

也可以暴力的关闭可以直接在客户端进行PeerConnection.Close(), 或者暴力关闭网页；

webrtc关闭拉流接口

方法: http post

uri: http://hostname:hostport/unsubscribe/*roomId*/*uid*/*remoteUid*

http post body: null

返回:
```markup
http body: 
{
    "code": 0,
    "desc": "ok"
}
```
举例，向host=192.168.1.98:8090, roomId=2001, 自己作为uid=6547关闭对远端remoteUid=4489拉流.

url为: http://192.168.1.98:8090/unsubscribe/2001/6547/4489

post data为空

返回data为
```markup
{
    "code": 0,
    "desc": "ok"
}
```

## 4. 客户端demo
提供客户端web demo: [webrtc whip sdk](https://github.com/runner365/webrtc-client-sdk/tree/whip)

webrtc sfu服务: [cpp_media_server](https://github.com/runner365/cpp_media_server)