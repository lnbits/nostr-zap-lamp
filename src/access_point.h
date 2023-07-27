// custom access point pages
static const char PAGE_ELEMENTS[] PROGMEM = R"(
{
  "uri": "/settings",
  "title": "Settings",
  "menu": true,
  "element": [
    {
      "name": "text",
      "type": "ACText",
      "value": "Nostr Zap Lamp Settings",
      "style": "font-family:Arial;font-size:16px;font-weight:400;color:#191970;margin-botom:15px;"
    },
    {
      "type": "ACInput",
      "label": "Password for PoS AP WiFi",
      "name": "ap_password",
      "value": "ToTheMoon1"
    },
    {
      "type": "ACInput",
      "label": "npub in Hex format to watch for zaps",
      "name": "npub_hex_to_watch",
      "value": ""
    },
    {
      "type": "ACInput",
      "label": "Nostr relay to use",
      "name": "relay",
      "value": "legend.lnbits.com/nostrclient/api/v1/relay"
    },
    {
      "name": "save",
      "type": "ACSubmit",
      "value": "Save",
      "uri": "/save"
    },
    {
      "name": "adjust_width",
      "type": "ACElement",
      "value": "<script type='text/javascript'>window.onload=function(){var t=document.querySelectorAll('input[]');for(i=0;i<t.length;i++){var e=t[i].getAttribute('placeholder');e&&t[i].setAttribute('size',e.length*.8)}};</script>"
    }
  ]
 }
)";

static const char PAGE_SAVE[] PROGMEM = R"(
{
  "uri": "/save",
  "title": "Elements",
  "menu": false,
  "element": [
    {
      "name": "caption",
      "type": "ACText",
      "format": "Settings have been saved to %s",
      "style": "font-family:Arial;font-size:18px;font-weight:400;color:#191970"
    },
    {
      "name": "validated",
      "type": "ACText",
      "style": "color:red"
    },
    {
      "name": "echo",
      "type": "ACText",
      "style": "font-family:monospace;font-size:small;white-space:pre;"
    },
    {
      "name": "ok",
      "type": "ACSubmit",
      "value": "OK",
      "uri": "/settings"
    }
  ]
}
)";