# OPC UA server issue: SecureChannel token mismatch after renewal

## Summary

Clients that renew their SecureChannel are disconnected at about 95% of the original token lifetime.
The server's `OpenSecureChannel` (Renew) response returns the **same TokenId as the original token (1)**.
Later the server sends messages secured with **TokenId 2**, which it never announced.
A spec-compliant client (open62541 via Qt OPC UA) rejects those messages with `BadSecureChannelTokenUnknown` and closes the channel, which also deletes the subscription.

## Environment

| Item | Value |
|---|---|
| Server endpoint | `opc.tcp://m3:48400/UA/ComServerWrapper` |
| Security | SecurityMode None, SecurityPolicy None (also advertises Basic256Sha256 Sign and SignAndEncrypt) |
| Client | Qt 6.12.0 OPC UA module, open62541 backend |
| Client app | `qtcon-ua-sub`: one monitored item, `ns=2;s=0:TEST1/SGGN1/OUT.CV`, 1 s sampling |

## Expected behavior (OPC UA Part 4 §5.5.2, Part 6 §6.7.4)

1. The client sends `OpenSecureChannel` with `RequestType=Renew` before the token expires.
2. The server responds with a **new** `ChannelSecurityToken` (new `TokenId`).
3. The server keeps using the old token until it receives a message secured with the new token (or the old one expires).
4. After that, the server secures its messages with the **TokenId it returned in the Renew response**.

## Observed behavior

Captured with a TCP proxy that decodes the chunk headers. The test used a 60 s requested lifetime, which the server accepted. The same sequence happens with 600 s and 1800 s.

| Time | Direction | Event |
|---|---|---|
| 00:33:33.7 | S→C | OPN response: `channel=86 tokenId=1 lifetime=60000ms` (initial open) |
| 00:34:18.7 | C→S | OPN Renew request (75% of lifetime) |
| 00:34:18.7 | S→C | OPN response: `channel=87 tokenId=1 lifetime=60000ms` |
| 00:34:18 to 00:34:30 | both | All messages use `token=1` |
| 00:34:31.796 | S→C | MSG secured with **`token=2`** (about 58 s after open, 95% of lifetime) |
| 00:34:31.8 | client | `Unknown SecurityToken` → `BadSecureChannelTokenUnknown`, channel closed |

The same sequence with the production 1800 s lifetime, from the client log:

```
23:44:45  SecureChannel opened, revised lifetime 1800.0s
00:07:15  SecureChannel renewed with a revised lifetime of 1800.0s    (75%)
00:13:16  Unknown SecurityToken -> BadSecureChannelTokenUnknown       (95%, 28.5 min)
```

The renewal response bytes (last 24 bytes of the OPN response) decode to:
`channelId=87, tokenId=1, createdAt=<ts>, revisedLifetime=60000, serverNonce=null`.
A renewal should have `tokenId != 1`.

## What to check or fix on the server

1. **Renew response TokenId.** Make the `ChannelSecurityToken.TokenId` in the Renew response equal to the token ID the server will later use for outgoing messages. It also must differ from the previous token. The server appears to use an internal counter (+1) for sending but returns the old value in the response.
2. **Token switch timing.** Switch to the new token only when it receives a message secured with it. Otherwise switch at old-token expiry. Do not switch silently at about 95% of the lifetime.
3. Verify the server stack's token ID counter increments exactly once per `OpenSecureChannel` (Issue or Renew), and the same value is used in both the response and the symmetric security headers.

## How to verify the fix

- Connect with a client that requests a short lifetime (e.g. 60 s). The connection must survive past 2 or 3 lifetimes with no `BadSecureChannelTokenUnknown`.
- In a packet capture, the TokenId in the Renew response must equal the TokenId in subsequent server MSG chunks.

## Client-side mitigation (temporary)

The client cannot fix this and stays spec-compliant. Until the server is fixed, a client can:
- Auto-reconnect after an unexpected disconnect (re-create the subscription).
- Request a longer channel lifetime to make renewals rare. The server may revise it.

## Artifacts

- Client log of the 40-minute run: `opcua-run2.log` (session files folder)
- Reproduction (60 s lifetime) with proxy and logs: `diag\` (`proxy.py`, `proxy.log`, `app.log`)
