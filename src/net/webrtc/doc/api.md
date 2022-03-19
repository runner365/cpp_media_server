# websocket api


## Join
request:
<pre>
{
    "roomId": "123456",
    "uid": "1111111"
}
</pre>

response:
<pre>
{
    "users":[
        {
            "uid": "1000"
        }
    ]
}
</pre>

Notification:<br/>
method: 'userin' <br/>
data: {"uid":"xxxxx"}

## Publish
request:
<pre>
{
    "roomId": "123456",
    "uid": "1111111"
    "sdp": "xxxxx"
}
</pre>
response:
<pre>
{
    "code": 0,
    "desc": "ok",
    "pcid": "xxxxx",
    "sdp": "xxxxx"
}
</pre>

## UnPublish
request:
<pre>
{
    "roomId": "123456",
    "uid": "1111111",
    "pcid": "xxxxx"
}
</pre>
response:
<pre>
{
    "code": 0,
    "desc": "ok"
}
</pre>

## Notification: 'publish'
data:
<pre>
{
    "roomId" "xxxxx",
    "uid": "xxxx",
    "pcid": "xxxx",
    "publishers": [
        {
            "pid": "xxxx",
            "type": "video",
            "mid": 0,
            "ssrc": 12345678
        },
        {
            "pid": "xxxx",
            "type": "audio",
            "mid": 0,
            "ssrc": 87654321
        }
    ]
}
</pre>

## Subscribe
request:
<pre>
{
    "roomId": "xxxx",
    "uid": "xxxxx",
    "remoteUid": "xxxxx",
    "sdp": "xxxx",
    "publishers":[
        {
            "pid": "xxxx",
            "type": "video",
            "mid": 1,
            "ssrc": 12345678
        },
        {
            "pid": "xxxx",
            "type": "audio",
            "mid": 0,
            "ssrc": 87654321
        }
    ]
}
</pre>
response:
<pre>
{
    "code": 0,
    "sdp": "xxxx"
}
</pre>

## Notification UnPublish
notification:
<pre>
{
    "uid": "xxxxx",
    "pcid": "xxxxxxxx",
    "publishers":[
        {
            "ssrc": 123456,
            "type": "audio",
            "mid": 0,
            "pid": "111111"
        },
        {
            "ssrc": 654321,
            "type": "video",
            "mid": 1,
            "pid": "222222"
        }
    ]
}
</pre>